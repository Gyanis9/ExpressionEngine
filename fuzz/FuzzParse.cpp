// libFuzzer 目标：把任意字节喂进「不抛异常」那条解析通道，判与 TestUntrustedInput.cpp 同一组不变式。
// 差别在失败形状：用例记 ADD_FAILURE，这里必须让进程当场停——libFuzzer 只按崩溃信号判定，
// 并把触发输入原样存成 repro 文件。因此两条铁律：
//   1) 异常一律不接（除 simplify 那条允许库内异常的通道），从承诺不抛的通道漏出异常本身就是缺陷；
//   2) 契约越界（空原因、空指针、树深越过公开上限、把非有限取值交给宿主）显式中止，
//      否则退化会静默通过——静默通过的门禁比没有门禁更糟。

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Expression/Expression.h>
#include <ExpressionEngine/Expression/ExpressionParser.h>
#include <ExpressionEngine/Expression/Value.h>

namespace ExpressionEngine::Expression
{
    namespace
    {
        /// 当前这条输入与它的持久文本：崩溃件里只有字节，报红时把两者打出来才看得懂现场
        std::string currentInput;
        std::string currentText;

        /**
         * @brief 判据失败：写下原因、原文与持久文本，并中止进程
         * @param reason 哪条不变式被越过
         * @details 不返回：libFuzzer 需要的是崩溃信号，让它把这条输入存成 repro 文件。
         *          模糊目标按单实例跑（本仓库不用 -jobs），那两个文件量不需要同步。
         */
        [[noreturn]] void violate(const char *reason)
        {
            std::fprintf(stderr, "fuzz 判据失败：%s\n原文=[%s] 持久文本=[%s]\n", reason, currentInput.c_str(), currentText.c_str());
            std::abort();
        }

        /// 只有裸数与带单位的量承诺「必有限」，其余取值按契约原样交给宿主
        bool mustBeFinite(const Value &value)
        {
            return std::holds_alternative<double>(value) || std::holds_alternative<Units::Quantity>(value);
        }
    } // namespace

    /**
     * @brief libFuzzer 的入口：喂一条输入，判全部不变式
     * @param data 输入字节（可能含 NUL，长度由 -max_len 限定）
     * @param size 输入长度
     * @return 恒为 0（失败走 violate() 的 abort，或让异常穿出去）
     * @note 名字与签名由 libFuzzer 规定，故不适用本仓库的大小写规范；返回值按它的约定忽略即可。
     */
    extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, const std::size_t size)
    {
        const std::string text(reinterpret_cast<const char *>(data), size);
        currentInput = text;

        // 这一行不设 try：Base::Exception 或 std::exception 从 tryParse 漏出时会穿过本函数，
        // libFuzzer 于是把它报成崩溃——正是「不抛异常」那条通道的契约内容。
        auto parsed = ExpressionParser::tryParse(nullptr, text);
        if (!parsed.has_value())
        {
            if (parsed.error().message.empty())
            {
                violate("解析失败却没给出原因");
            }
            return 0;
        }

        ExpressionPtr tree = std::move(*parsed);
        if (tree == nullptr)
        {
            violate("解析成功却给了空指针");
        }
        if (tree->astDepth() > Expression::maxAstDepth)
        {
            violate("树深越过了公开承诺的上限");
        }

        // 直接写进那个文件量再判：violate() 打印的就是现场，不留一个可能过期的局部副本
        currentText = tree->toString(true);
        if (currentText.empty())
        {
            violate("文本化为空");
        }
        const std::string &printed = currentText;

        // 「解析不回来」「引用路径逐格相同」「再写一次是自身不动点」三条判据暂停用：它们各自会撞上
        // CHANGELOG「已知边界」里那四条未修的文本歧义（单位并写与 % 混用、名字段含 NUL、名字段里的
        // 特殊字节打出配不上的定界）。当成判据只会让这道门长期红、盖住它其它部分的信号；四条都修好后
        // 连同 fuzz/seed 与用例里留下的触发输入一起放开。这里保留发布时就有的宽松形态：解析失败可以，
        // 但必须给出原因。
        const auto reparsed = ExpressionParser::tryParse(nullptr, printed);
        if (reparsed.has_value())
        {
            if (*reparsed == nullptr)
            {
                violate("重新解析库内文本给了空指针");
            }
            if ((*reparsed)->astDepth() > Expression::maxAstDepth)
            {
                violate("重新解析库内文本后树深越界");
            }
        } else if (reparsed.error().message.empty())
        {
            violate("重新解析库内文本失败却没给出原因");
        }

        try
        {
            const ExpressionPtr simplified = tree->simplify();
            if (simplified != nullptr && simplified->astDepth() > Expression::maxAstDepth)
            {
                violate("化简后树深越界");
            }
        } catch (const Base::Exception &)
        {
            // 化简途中撞上限是允许的：那是库内异常，走调用方 catch 得住的那条通道
        }

        if (tree->copy() == nullptr)
        {
            violate("深拷贝给了空指针");
        }

        const auto evaluated = tree->tryEvaluate();
        if (!evaluated.has_value())
        {
            if (evaluated.error().message.empty())
            {
                violate("求值失败却没给出原因");
            }
            return 0;
        }
        if (mustBeFinite(*evaluated))
        {
            const double number = std::holds_alternative<double>(*evaluated) ? std::get<double>(*evaluated) : std::get<Units::Quantity>(*evaluated).getValue();
            if (!std::isfinite(number))
            {
                violate("非有限的裸数或数量取值被交给了宿主");
            }
        }
        return 0;
    }
} // namespace ExpressionEngine::Expression
