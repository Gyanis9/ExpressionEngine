// 本文件钉住手写数量解析器对原文法语义的保持：数字形态、单位结合、优先级与拒绝面。

#include <gtest/gtest.h>

#include <limits>
#include <numbers>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Units/QuantityParser.h>

namespace ExpressionEngine::Units
{
    namespace
    {

        /**
         * @brief 钉住：数字与单位相邻即相乘，基准量纲为 mm
         */
        TEST(QuantityParserTest, NumberFollowedByUnitMultiplies)
        {
            EXPECT_DOUBLE_EQ(QuantityParser::parse("1.5 mm").getValue(), 1.5);
            EXPECT_DOUBLE_EQ(QuantityParser::parse("1 m").getValue(), 1000.0);
            EXPECT_DOUBLE_EQ(QuantityParser::parse("1 km").getValue(), 1.0e6);
            EXPECT_EQ(QuantityParser::parse("2 kg").getUnit(), Unit::Mass);
        }

        /**
         * @brief 钉住：逗号与点号都按小数点解释，与原文法的两条数字规则一致
         */
        TEST(QuantityParserTest, AcceptsCommaAsDecimalSeparator)
        {
            EXPECT_DOUBLE_EQ(QuantityParser::parse("1,5 mm").getValue(), 1.5);
            EXPECT_DOUBLE_EQ(QuantityParser::parse(".5 mm").getValue(), 0.5);
            EXPECT_DOUBLE_EQ(QuantityParser::parse("1.5e3 mm").getValue(), 1500.0);
        }

        /**
         * @brief 钉住：除号后紧跟单位时按「数值/单位」解释，形成倒数量纲
         */
        TEST(QuantityParserTest, DividesByBareUnit)
        {
            const Quantity inverseLength = QuantityParser::parse("1/mm");
            EXPECT_DOUBLE_EQ(inverseLength.getValue(), 1.0);
            EXPECT_EQ(inverseLength.getUnit(), Unit::InverseLength);

            // 除号后是数字时先算数值除法，再与单位相乘
            const Quantity halfMetre = QuantityParser::parse("1/2 m");
            EXPECT_DOUBLE_EQ(halfMetre.getValue(), 500.0);
            EXPECT_EQ(halfMetre.getUnit(), Unit::Length);
        }

        /**
         * @brief 钉住：相邻分段求和，英制写法 5' 6" 得到 5 英尺 6 英寸
         */
        TEST(QuantityParserTest, SumsAdjacentSegments)
        {
            const Quantity feetAndInches = QuantityParser::parse("5' 6\"");
            EXPECT_DOUBLE_EQ(feetAndInches.getValue(), 1524.0 + 152.4);

            const Quantity metric = QuantityParser::parse("1 m 20 cm");
            EXPECT_DOUBLE_EQ(metric.getValue(), 1200.0);
        }

        /**
         * @brief 钉住：一元负号低于乘方，因此 -2^2 是 -(2^2)；幂次右结合
         */
        TEST(QuantityParserTest, UnaryMinusBindsLooserThanPower)
        {
            EXPECT_DOUBLE_EQ(QuantityParser::parse("-2^2").getValue(), -4.0);
            EXPECT_DOUBLE_EQ(QuantityParser::parse("2^-3").getValue(), 0.125);
            EXPECT_DOUBLE_EQ(QuantityParser::parse("2^3^2").getValue(), 512.0);
        }

        /**
         * @brief 钉住：标量函数按原文法只作用于纯数值
         */
        TEST(QuantityParserTest, ScalarFunctionsApplyToNumbers)
        {
            EXPECT_NEAR(QuantityParser::parse("sin(pi/2)").getValue(), 1.0, 1e-12);
            EXPECT_DOUBLE_EQ(QuantityParser::parse("sqrt(4)").getValue(), 2.0);
            EXPECT_DOUBLE_EQ(QuantityParser::parse("abs(-3)").getValue(), 3.0);
            EXPECT_DOUBLE_EQ(QuantityParser::parse("log10(1000)").getValue(), 3.0);
        }

        /**
         * @brief 钉住：单位幂次作用在量纲上，数值保持 1
         */
        TEST(QuantityParserTest, UnitPowersApplyToDimension)
        {
            const Quantity squareMillimetre = QuantityParser::parse("mm^2");
            EXPECT_EQ(squareMillimetre.getUnit(), Unit::Area);
            EXPECT_DOUBLE_EQ(squareMillimetre.getValue(), 1.0);

            const Quantity squareMetre = QuantityParser::parse("2 m^2");
            EXPECT_EQ(squareMetre.getUnit(), Unit::Area);
            EXPECT_DOUBLE_EQ(squareMetre.getValue(), 2.0e6);
        }

        /**
         * @brief 钉住：常量与括号参与运算
         */
        TEST(QuantityParserTest, ConstantsAndParentheses)
        {
            EXPECT_NEAR(QuantityParser::parse("pi").getValue(), std::numbers::pi, 1e-12);
            EXPECT_DOUBLE_EQ(QuantityParser::parse("(1+2)*3").getValue(), 9.0);
        }

        /**
         * @brief 钉住：方括号注释整段跳过，不影响其余记号
         */
        TEST(QuantityParserTest, SkipsBracketComments)
        {
            EXPECT_DOUBLE_EQ(QuantityParser::parse("[这是注释] 1 mm").getValue(), 1.0);
        }

        /**
         * @brief 钉住：空输入返回最小正数，与原文法的「无有效内容」约定一致
         */
        TEST(QuantityParserTest, EmptyInputYieldsSmallestPositive)
        {
            EXPECT_DOUBLE_EQ(QuantityParser::parse("").getValue(), std::numeric_limits<double>::min());
        }

        /**
         * @brief 钉住拒绝面：单位不匹配、非法字符、缺单位分段、分段过多都要报错
         */
        TEST(QuantityParserTest, RejectsMalformedInput)
        {
            // 相邻分段单位不同，求和时报单位不匹配
            EXPECT_THROW(static_cast<void>(QuantityParser::parse("1 mm 1 s")), Base::UnitsMismatchError);

            // 无法识别的字符
            EXPECT_THROW(static_cast<void>(QuantityParser::parse("1 mm $")), Base::ParserError);

            // 第二段没有单位
            EXPECT_THROW(static_cast<void>(QuantityParser::parse("1 2")), Base::ParserError);

            // 超过三段相邻数量
            EXPECT_THROW(static_cast<void>(QuantityParser::parse("1mm 2mm 3mm 4mm")), Base::ParserError);

            // 括号未闭合
            EXPECT_THROW(static_cast<void>(QuantityParser::parse("(1+2 mm")), Base::ParserError);

            // 注释未闭合
            EXPECT_THROW(static_cast<void>(QuantityParser::parse("[1 mm")), Base::ParserError);

            // 单位取分数次幂无法表示
            EXPECT_THROW(static_cast<void>(QuantityParser::parse("mm^2.5")), Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：未知单位名不会被当成标识符吞掉，而是在字母处报错
         */
        TEST(QuantityParserTest, UnknownUnitNameIsRejected)
        {
            EXPECT_THROW(static_cast<void>(QuantityParser::parse("1 furlong")), Base::ParserError);
        }

    } // namespace
} // namespace ExpressionEngine::Units
