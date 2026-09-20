/**
 * @file ParserBenchmark.cpp
 * @brief 手写解析器吞吐基准
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <ExpressionEngine/Expression/Dictionary.h>
#include <ExpressionEngine/Expression/ExpressionLexer.h>
#include <ExpressionEngine/Expression/ExpressionParser.h>
#include <ExpressionEngine/Expression/FunctionRegistry.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/QuantityParser.h>
#include <ExpressionEngine/Units/Unit.h>

#include "LegacyQuantityParser.h"

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__)
#include <cpuid.h>
#endif

// 全局替换堆分配函数只为统计解析过程中的分配次数：计数用原子量，静态存储期，
// 不会在计数前动态初始化。只覆盖普通形式，对齐分配（aligned new）不计入。
namespace
{
    std::atomic<std::size_t> g_allocationCount{0};
}

void *operator new(std::size_t size)
{
    g_allocationCount.fetch_add(1, std::memory_order_relaxed);
    void *pointer = std::malloc(size == 0 ? 1 : size);
    if (pointer == nullptr)
    {
        throw std::bad_alloc();
    }
    return pointer;
}

void *operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void *pointer) noexcept
{
    std::free(pointer);
}

void operator delete[](void *pointer) noexcept
{
    std::free(pointer);
}

void operator delete(void *pointer, std::size_t) noexcept
{
    std::free(pointer);
}

void operator delete[](void *pointer, std::size_t) noexcept
{
    std::free(pointer);
}

namespace
{
    /// 计时批次数量：中位数与平均值都取自这批样本，单批内部连跑多次以摊薄时钟粒度
    constexpr int g_batchCount = 21;

    /// 单批的目标时长：太短会被 steady_clock 的粒度淹没，太长则受不了偶发抖动
    constexpr double g_batchTargetNanoseconds = 5.0e6;

    /// 预热时长：先让指令缓存、分支预测与库内惰性初始化都稳定下来
    constexpr double g_warmupNanoseconds = 30.0e6;

    /// 接收解析结果，防止整批调用被优化掉；写回发生在计时区间之外
    volatile std::uintptr_t g_resultSink = 0;

    /// 单项基准的统计结果
    struct BenchmarkStats
    {
        double      medianNanoseconds{0}; ///< 归一到单次的耗时中位数
        double      meanNanoseconds{0};   ///< 归一到单次的耗时平均值
        std::size_t timedIterations{0};   ///< 计入统计的调用次数
        std::size_t allocations{0};       ///< 计时区间内的堆分配总次数
    };

    /// 一条基准用例：组/用例名加语料文本
    struct BenchmarkCase
    {
        std::string label; ///< 形如「数量解析/典型输入」
        std::string text;  ///< 语料文本
    };

    /// 取 CPU 品牌串；取不到时退回环境变量
    [[nodiscard]] std::string cpuBrandString()
    {
#if defined(_MSC_VER)
        int  registers[4] = {0, 0, 0, 0};
        char brand[49]    = {};
        for (int leaf = 0; leaf < 3; ++leaf)
        {
            __cpuid(registers, 0x80000002 + leaf);
            std::memcpy(brand + static_cast<std::size_t>(leaf) * 16, registers, 16);
        }
        return brand;
#elif defined(__GNUC__)
        unsigned int eax       = 0;
        unsigned int ebx       = 0;
        unsigned int ecx       = 0;
        unsigned int edx       = 0;
        char         brand[49] = {};
        for (int leaf = 0; leaf < 3; ++leaf)
        {
            if (__get_cpuid(static_cast<unsigned int>(0x80000002 + leaf), &eax, &ebx, &ecx, &edx) == 0)
            {
                return {};
            }
            std::memcpy(brand + static_cast<std::size_t>(leaf) * 16, &eax, 4);
            std::memcpy(brand + static_cast<std::size_t>(leaf) * 16 + 4, &ebx, 4);
            std::memcpy(brand + static_cast<std::size_t>(leaf) * 16 + 8, &ecx, 4);
            std::memcpy(brand + static_cast<std::size_t>(leaf) * 16 + 12, &edx, 4);
        }
        return brand;
#else
        return {};
#endif
    }

    /// 终端显示宽度：UTF-8 中文按两列计，其余按一列计，仅用于对齐输出
    [[nodiscard]] std::size_t displayWidth(std::string_view text)
    {
        std::size_t width = 0;
        for (std::size_t index = 0; index < text.size();)
        {
            const auto byte = static_cast<unsigned char>(text[index]);
            if (byte < 0x80)
            {
                ++index;
                ++width;
            } else
            {
                index += byte >= 0xF0 ? 4 : (byte >= 0xE0 ? 3 : 2);
                width += 2;
            }
        }
        return width;
    }

    /// 把标签补齐到指定显示宽度
    [[nodiscard]] std::string padLabel(std::string_view label, std::size_t width)
    {
        const std::size_t current = displayWidth(label);
        return std::string(label) + std::string(current < width ? width - current : 1, ' ');
    }

    /// 把短式子用分隔符重复拼到不低于指定长度，用于构造长输入
    [[nodiscard]] std::string repeatToLength(std::string_view unit, std::string_view separator, std::size_t minimumLength)
    {
        std::string result(unit);
        while (result.size() < minimumLength)
        {
            result += separator;
            result += unit;
        }
        return result;
    }

    /**
     * @brief 测量一个可调用对象
     * @details 先预热到时长稳定，再按单次耗时把每批凑到约 5 ms 连跑 [g_batchCount] 批，
     *          逐批归一成单次耗时后取中位数与平均值；分配次数按批累计。
     * @param callable 每次调用返回一个值，供防止优化用的汇聚点使用
     * @return 统计结果
     */
    template<typename Callable>
    [[nodiscard]] BenchmarkStats measure(Callable &&callable)
    {
        const auto warmupDeadline =
                std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double, std::nano>(g_warmupNanoseconds));
        std::uintptr_t warmupSink = 0;
        while (std::chrono::steady_clock::now() < warmupDeadline)
        {
            warmupSink ^= callable();
        }
        g_resultSink = warmupSink;

        // 标定：先单跑一次估出单次耗时，据此决定每批的迭代次数
        const auto     probeStart       = std::chrono::steady_clock::now();
        std::uintptr_t probeSink        = callable();
        const auto     probeFinish      = std::chrono::steady_clock::now();
        const double   probeNanoseconds = std::max(1.0, std::chrono::duration<double, std::nano>(probeFinish - probeStart).count());
        g_resultSink                    = probeSink;
        const auto iterationsPerBatch   = static_cast<std::size_t>(std::clamp(g_batchTargetNanoseconds / probeNanoseconds, 1.0, 1.0e6));

        std::vector<double> samples;
        samples.reserve(g_batchCount);
        std::size_t allocations = 0;
        std::size_t iterations  = 0;
        for (int batch = 0; batch < g_batchCount; ++batch)
        {
            const std::size_t allocationsBefore = g_allocationCount.load(std::memory_order_relaxed);
            std::uintptr_t    batchSink         = 0;
            const auto        start             = std::chrono::steady_clock::now();
            for (std::size_t iteration = 0; iteration < iterationsPerBatch; ++iteration)
            {
                batchSink ^= callable();
            }
            const auto finish = std::chrono::steady_clock::now();
            allocations += g_allocationCount.load(std::memory_order_relaxed) - allocationsBefore;
            iterations += iterationsPerBatch;
            g_resultSink = batchSink;
            samples.push_back(std::chrono::duration<double, std::nano>(finish - start).count() / static_cast<double>(iterationsPerBatch));
        }

        double sum = 0.0;
        for (const double sample: samples)
        {
            sum += sample;
        }
        const auto middle = samples.begin() + samples.size() / 2;
        std::nth_element(samples.begin(), middle, samples.end());

        BenchmarkStats stats;
        stats.medianNanoseconds = *middle;
        stats.meanNanoseconds   = sum / static_cast<double>(samples.size());
        stats.timedIterations   = iterations;
        stats.allocations       = allocations;
        return stats;
    }

    /// 打印一项统计结果；长输入的每字节耗时与 MB/s 在这里给出
    void printStats(const std::string &label, const std::string &text, const BenchmarkStats &stats)
    {
        const double bytes              = static_cast<double>(text.size());
        const double nanosecondsPerByte = stats.medianNanoseconds / bytes;
        const double megabytesPerSecond = nanosecondsPerByte > 0.0 ? 1000.0 / nanosecondsPerByte : 0.0;
        const double thousandsPerSecond = 1.0e6 / stats.medianNanoseconds;
        const double allocationsPerCall = static_cast<double>(stats.allocations) / static_cast<double>(stats.timedIterations);
        std::printf("%s中位数 %8.3f us/次  平均 %8.3f us/次  %9.1f k次/秒  %8.3f MB/s  %7.3f ns/字节  分配 %6.2f 次/次  %zu 字节  计时 %zu 次\n", padLabel(label, 34).c_str(),
                    stats.medianNanoseconds / 1000.0, stats.meanNanoseconds / 1000.0, thousandsPerSecond, megabytesPerSecond, nanosecondsPerByte, allocationsPerCall, text.size(),
                    stats.timedIterations);
    }

    /// 测一项并打印一行结果
    template<typename Callable>
    void runCase(const std::string &label, const std::string &text, Callable &&callable)
    {
        printStats(label, text, measure(callable));
    }

    /// 解析一个数量，返回结果的位模式
    [[nodiscard]] std::uintptr_t parseQuantity(std::string_view text)
    {
        const ExpressionEngine::Units::Quantity quantity = ExpressionEngine::Units::QuantityParser::parse(text);
        return static_cast<std::uintptr_t>(std::bit_cast<std::uint64_t>(quantity.getValue()));
    }

    /// 解析一个表达式后立即释放，返回 AST 根地址
    [[nodiscard]] std::uintptr_t parseExpression(std::string_view text)
    {
        const ExpressionEngine::Expression::ExpressionPtr expression = ExpressionEngine::Expression::ExpressionParser::parse(nullptr, text);
        return reinterpret_cast<std::uintptr_t>(expression.get());
    }

    /// 取值的位模式，只为把求值结果交给防止优化用的汇聚点
    [[nodiscard]] std::uintptr_t sinkOf(const ExpressionEngine::Expression::Value &value)
    {
        return std::visit(
                []<typename Alternative>(const Alternative &item) -> std::uintptr_t
                {
                    using Type = std::decay_t<Alternative>;
                    if constexpr (std::is_same_v<Type, ExpressionEngine::Units::Quantity>)
                    {
                        return static_cast<std::uintptr_t>(std::bit_cast<std::uint64_t>(item.getValue()));
                    } else if constexpr (std::is_same_v<Type, double>)
                    {
                        return static_cast<std::uintptr_t>(std::bit_cast<std::uint64_t>(item));
                    } else if constexpr (std::is_same_v<Type, bool>)
                    {
                        return static_cast<std::uintptr_t>(item);
                    } else if constexpr (std::is_same_v<Type, std::string>)
                    {
                        return item.size();
                    } else if constexpr (std::is_same_v<Type, ExpressionEngine::Expression::ValueSequence>)
                    {
                        return item.size();
                    } else
                    {
                        // 几何取值不参与数值比较，借排版给一个依赖内容的稳定量
                        return ExpressionEngine::Expression::toString(ExpressionEngine::Expression::Value(item)).size();
                    }
                },
                value);
    }

    /// 求值用例共用的宿主：字典给名字，注册表给自定义函数
    ExpressionEngine::Expression::Dictionary       g_evaluationDictionary; ///< 求值用例的名字来源
    ExpressionEngine::Expression::FunctionRegistry g_evaluationRegistry;   ///< 求值用例的自定义函数来源

    /// 装配求值夹具；放在 main 开头，保证计时前字典与注册表都已就绪
    void prepareEvaluationFixtures()
    {
        static_cast<void>(g_evaluationRegistry.registerFunction(
                {.name         = "taxed",
                 .function     = [](const ExpressionEngine::Expression::FunctionCall &call) { return ExpressionEngine::Units::Quantity(1.13) * ExpressionEngine::Expression::toQuantity(call.argumentValue(0), "taxed 的实参"); },
                 .minArguments = 1,
                 .maxArguments = 1}));
        g_evaluationDictionary.define("Length", ExpressionEngine::Units::Quantity(3000.0, ExpressionEngine::Units::Unit::Length));
        g_evaluationDictionary.define("Width", ExpressionEngine::Units::Quantity(2000.0, ExpressionEngine::Units::Unit::Length));
    }

    /// 解析并求值一次，返回取值的位模式；建树与求值都在计时区间内
    [[nodiscard]] std::uintptr_t parseAndEvaluate(const std::string_view text)
    {
        const ExpressionEngine::Expression::ExpressionPtr expression =
                ExpressionEngine::Expression::ExpressionParser::parse(&g_evaluationDictionary, text, g_evaluationRegistry);
        return sinkOf(expression->evaluate());
    }

    /// 只跑表达式词法直到 End，不建 AST
    [[nodiscard]] std::uintptr_t lexExpression(std::string_view text)
    {
        ExpressionEngine::Expression::ExpressionLexer lexer(text);
        std::uintptr_t                                sink = 0;
        for (ExpressionEngine::Expression::ExpressionToken token = lexer.next(); token.kind != ExpressionEngine::Expression::ExpressionTokenKind::End; token = lexer.next())
        {
            sink += token.text.size() + static_cast<std::size_t>(token.kind);
        }
        return sink;
    }

    /// 跑一次语料，只为提前暴露语料本身解析不过的情况
    template<typename Callable>
    [[nodiscard]] bool validate(const std::string &label, Callable &&callable)
    {
        try
        {
            g_resultSink = callable();
            return true;
        } catch (const std::exception &error)
        {
            std::printf("%s语料解析失败，跳过计时：%s\n", padLabel(label, 34).c_str(), error.what());
            return false;
        }
    }
} // namespace

int main()
{
    prepareEvaluationFixtures();

    std::printf("== ExpressionEngine 解析器吞吐基准 ==\n");
    std::printf("CPU: %s\n", cpuBrandString().c_str());
#ifdef BENCH_BUILD_TYPE
    std::printf("构建类型: %s\n", BENCH_BUILD_TYPE);
#else
    std::printf("构建类型: 未声明\n");
#endif
#if defined(_MSC_VER)
    std::printf("编译器: MSVC %d (C++ 标准 %ld)\n", _MSC_VER, _MSVC_LANG);
#elif defined(__GNUC__)
    std::printf("编译器: GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
#ifdef BENCH_COMPILER_FLAGS
    std::printf("编译选项: %s\n", BENCH_COMPILER_FLAGS);
#endif
    std::printf("计时: 预热 %.0f ms 后跑 %d 批，每批按标定凑到约 %.0f ms，中位数/平均值为逐批归一后的结果\n", g_warmupNanoseconds / 1.0e6, g_batchCount,
                g_batchTargetNanoseconds / 1.0e6);
    std::printf("说明: 表达式解析一列的每次调用含 AST 的构造与释放；词法一列只跑到 End，不建 AST\n\n");

    // 数量语料：五个短用例加一个拼到 200 字符以上的长式子
    const std::vector<BenchmarkCase> quantityCases = {
            {"数量解析/典型输入 1.5 mm", "1.5 mm"}, {"数量解析/分数 1/2 mm", "1/2 mm"},
            {"数量解析/英制 5' 6\"", "5' 6\""},     {"数量解析/组合 2 m/s", "2 m/s"},
            {"数量解析/科学计数 1e3 kg", "1e3 kg"}, {"数量解析/长式子(>200 字符)", repeatToLength("1+2*3-4/5+6^2+sin(30)+sqrt(16)+abs(-7)", "+", 200)},
    };

    // 表达式语料：七个短用例加一条 300 字符以上的长链与 50 层括号
    const std::vector<BenchmarkCase> expressionCases = {
            {"表达式解析/属性相乘 Box.Length*2", "Box.Length * 2"},
            {"表达式解析/当前对象 .Length", ".Length"},
            {"表达式解析/跨文档 <<Part>>", "<<Part>>.Box.Length"},
            {"表达式解析/单元格区间 [1:5]", "Box.Cells[1:5]"},
            {"表达式解析/三元 1>0?2:3", "1 > 0 ? 2 : 3"},
            {"表达式解析/函数 sin+cos", "sin(90) + cos(0)"},
            {"表达式解析/多参数 max(1;5,3)", "max(1; 5, 3)"},
            {"表达式解析/长链(>300 字符)", repeatToLength("1+2*3-4/5+6^2+7%3+8", "+", 300)},
            {"表达式解析/括号嵌套 50 层", std::string(50, '(') + "1+2*3" + std::string(50, ')')},
    };

    // 求值语料：解析 + 求值整条链路，覆盖内置函数、自定义函数、序列与字典引用
    const std::vector<BenchmarkCase> evaluationCases = {
            {"求值/纯算术 1+2*3", "1 + 2 * 3"},
            {"求值/数量加法 2 mm + 3 mm", "2 mm + 3 mm"},
            {"求值/内置函数 sqrt+abs", "sqrt(16) + abs(-7)"},
            {"求值/自定义函数 taxed(100 mm)", "taxed(100 mm)"},
            {"求值/序列构建与下标", "list(1; 2; 3)[1]"},
            {"求值/序列区间聚合", "sum(list(1; 2; 3; 4)[0:2])"},
            {"求值/字典引用 Length * 2", "Length * 2"},
            {"求值/字典多名字", "Length + Width * 2"},
    };

    // 词法语料：只取短式与长链两条，用来区分词法与语法分析的开销
    const std::vector<BenchmarkCase> lexerCases = {
            {"表达式词法/典型输入 Box.Length*2", expressionCases[0].text},
            {"表达式词法/长链(>300 字符)", expressionCases[7].text},
    };

#if defined(BENCH_HAS_LEGACY_QUANTITY)
    /// 数量用例的标签前缀：生成侧只替换这一截，两张表的列宽保持一致
    constexpr std::string_view quantityLabelPrefix = "数量解析/";

    // 数量一侧的对比：同一份语料分别跑手写解析器与 flex/bison 生成式解析器。
    // 两侧都落在同一个 Quantity 实现上（生成代码用的门面类继承 Units::Quantity），
    // 差别只剩词法与语法机器本身；生成侧每次调用还要建/销毁扫描 buffer，这部分走 malloc，
    // 不计入结果行里的「分配」列，属于生成方案自带的固定成本。
    std::printf("\n== 数量解析对比：手写解析器 vs flex/bison 生成式解析器 ==\n");
#endif

    for (const BenchmarkCase &entry: quantityCases)
    {
        if (!validate(entry.label, [&entry] { return parseQuantity(entry.text); }))
        {
            continue;
        }
        const BenchmarkStats handwrittenStats = measure([&entry] { return parseQuantity(entry.text); });
        printStats(entry.label, entry.text, handwrittenStats);
#if defined(BENCH_HAS_LEGACY_QUANTITY)
        const std::string generatedLabel = std::string("生成解析/") + entry.label.substr(quantityLabelPrefix.size());
        const std::string comparisonLabel = std::string("数量对比/") + entry.label.substr(quantityLabelPrefix.size());
        // 先各跑一次取结果位模式：两侧若数值不同，说明对比的两条路径做的不是同一件事，必须报出来。
        const std::uintptr_t handwrittenBits = parseQuantity(entry.text);
        if (validate(generatedLabel, [&entry] { return ExpressionEngine::Benchmarks::parseLegacyQuantity(entry.text); }))
        {
            const std::uintptr_t  generatedBits  = ExpressionEngine::Benchmarks::parseLegacyQuantity(entry.text);
            const BenchmarkStats generatedStats = measure([&entry] { return ExpressionEngine::Benchmarks::parseLegacyQuantity(entry.text); });
            printStats(generatedLabel, entry.text, generatedStats);
            // 倍数用两侧各自的中位数相除：生成代码每次调用额外建/销毁扫描 buffer，固定开销也计入其中。
            const double slowdownRatio = generatedStats.medianNanoseconds / handwrittenStats.medianNanoseconds;
            std::printf("%s生成 ÷ 手写 = %6.2f 倍耗时（中位数，越大越慢）  结果%s\n", padLabel(comparisonLabel, 34).c_str(), slowdownRatio,
                        handwrittenBits == generatedBits ? "一致" : "不一致");
            if (handwrittenBits != generatedBits)
            {
                std::printf("%s手写 %.17g  生成 %.17g\n", padLabel("数值差异/" + entry.label.substr(quantityLabelPrefix.size()), 34).c_str(),
                            std::bit_cast<double>(static_cast<std::uint64_t>(handwrittenBits)), std::bit_cast<double>(static_cast<std::uint64_t>(generatedBits)));
            }
        }
#endif
    }
    for (const BenchmarkCase &entry: expressionCases)
    {
        if (validate(entry.label, [&entry] { return parseExpression(entry.text); }))
        {
            runCase(entry.label, entry.text, [&entry] { return parseExpression(entry.text); });
        }
    }
    for (const BenchmarkCase &entry: evaluationCases)
    {
        if (validate(entry.label, [&entry] { return parseAndEvaluate(entry.text); }))
        {
            runCase(entry.label, entry.text, [&entry] { return parseAndEvaluate(entry.text); });
        }
    }
    for (const BenchmarkCase &entry: lexerCases)
    {
        if (validate(entry.label, [&entry] { return lexExpression(entry.text); }))
        {
            runCase(entry.label, entry.text, [&entry] { return lexExpression(entry.text); });
        }
    }
    return 0;
}
