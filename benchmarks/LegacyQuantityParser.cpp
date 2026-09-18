/**
 * @file LegacyQuantityParser.cpp
 * @brief FreeCAD flex/bison 生成代码的对比实现（仅基准使用）
 * @details 在一个 TU 里原样包含 Quantity.tab.c 与 Quantity.lex.c，并补齐它们要求的宿主环境。
 *          生成代码是第三方 C 风格代码，只在 benchmarks/CMakeLists.txt 里对这一个文件放宽告警，
 *          全局告警级别不变。
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#include "LegacyQuantityParser.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <string>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Units/Quantity.h>

namespace ExpressionEngine::Benchmarks
{
    namespace
    {
        // === 生成代码要求的宿主环境 ================================================
        // Quantity.lex.c / Quantity.tab.c 假定宿主 TU 里有一个名字就叫 Quantity 的类，
        // 以及 QuantResult、num_change()、Quantity_yyerror()、yylex()。这些名字都在本
        // 私有命名空间里补齐，不与库里的 Units::Quantity 冲突。

        /**
         * @brief 生成代码使用的数量门面
         * @details 直接继承 Units::Quantity：112 个单位常量与全部算术接口（构造、四则、pow、getValue）
         *          都由基类继承而来，只补一个从基类到派生类的转换构造，让
         *          `yylval = Quantity::MilliMetre;` 这类赋值成立。
         *          两侧基准因此落在同一个 Quantity 实现上，耗时差异只来自词法与语法机器。
         */
        class Quantity : public Units::Quantity
        {
        public:
            using Units::Quantity::Quantity;

            Quantity(const Units::Quantity &value) : Units::Quantity(value) {}
        };

        // 生成代码里的数学函数按 C 风格无限定名书写，这里把 std 版本引入本命名空间。
        using std::acos;
        using std::asin;
        using std::atan;
        using std::atan2;
        using std::cos;
        using std::exp;
        using std::fabs;
        using std::log;
        using std::log10;
        using std::pow;
        using std::sin;
        using std::sinh;
        using std::sqrt;
        using std::tan;
        using std::tanh;

        /// bison 的语义动作把解析结果写回这个全局量，与上游 Quantity.cpp 的用法一致。
        Quantity QuantResult;

        /**
         * @brief 去掉数字文本里的分组分隔符，并把小数分隔符统一成 '.'
         * @param text 数字文本，就地读取，不修改
         * @param decimalDelimiter 文本里的十进制分隔符
         * @param groupDelimiter 文本里的千位分组分隔符
         * @return 转换后的数值；文本长度超出缓冲上限时返回 0.0
         */
        double num_change(char *text, char decimalDelimiter, char groupDelimiter)
        {
            constexpr std::size_t capacity = 40;
            std::array<char, capacity> buffer{};
            std::size_t cursor = 0;
            for (char *character = text; *character != '\0'; ++character)
            {
                if (*character == groupDelimiter)
                {
                    continue;
                }
                buffer[cursor++] = (*character == decimalDelimiter && decimalDelimiter != '.') ? '.' : *character;
                if (cursor >= capacity)
                {
                    return 0.0;
                }
            }
            buffer[cursor] = '\0';
            return std::strtod(buffer.data(), nullptr);
        }

        /**
         * @brief 生成代码的报错出口
         * @details 与手写解析器一致地抛 ParserError，让两侧的失败路径都能被调用方捕获。
         */
        void Quantity_yyerror(const char *message)
        {
            throw Base::ParserError(message);
        }

        // bison 的调用点需要先看到扫描器入口，Scanner 自身的声明在 Quantity.lex.c 里。
        int yylex(void);

        // 上游 Quantity.cpp 同样把初始栈深压到 20：默认 200 会为每次解析多构造 200 个 Quantity。
        #define YYINITDEPTH 20
        #include "Quantity.tab.c"
        #include "Quantity.lex.c"
        #undef YYINITDEPTH
        #undef YYSTYPE
        #undef yyparse
        #undef yyerror

        /**
         * @brief 跑一次生成代码：建扫描 buffer、复位结果、调解析器、清理 buffer 与扫描状态
         * @param text 数量文本，由调用方保证生命周期覆盖本次调用
         * @return 结果数值的位模式
         */
        std::uintptr_t parseWithGeneratedCode(const std::string &text)
        {
            YY_BUFFER_STATE buffer = yy_scan_string(text.c_str());
            try
            {
                QuantResult = Quantity(std::numeric_limits<double>::min());
                Quantity_yyparse();
                const std::uintptr_t result = static_cast<std::uintptr_t>(std::bit_cast<std::uint64_t>(QuantResult.getValue()));
                yy_delete_buffer(buffer);
                BEGIN(INITIAL);
                return result;
            }
            catch (...)
            {
                yy_delete_buffer(buffer);
                BEGIN(INITIAL);
                throw;
            }
        }
    } // namespace

    std::uintptr_t parseLegacyQuantity(const std::string &text)
    {
        return parseWithGeneratedCode(text);
    }
} // namespace ExpressionEngine::Benchmarks
