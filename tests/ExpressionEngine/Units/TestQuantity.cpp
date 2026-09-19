// 本文件覆盖 Quantity 的运算契约、格式排版与单位不匹配拒绝面。

#include <gtest/gtest.h>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/UnitsApi.h>

namespace ExpressionEngine::Units
{
    namespace
    {

        /**
         * @brief 钉住：数值以基准量纲表示，同一物理量在不同单位下的数值不同
         */
        TEST(QuantityTest, PredefinedQuantitiesUseBaseUnits)
        {
            EXPECT_DOUBLE_EQ(Quantity::MilliMetre.getValue(), 1.0);
            EXPECT_DOUBLE_EQ(Quantity::CentiMetre.getValue(), 10.0);
            EXPECT_DOUBLE_EQ(Quantity::Metre.getValue(), 1000.0);
            EXPECT_DOUBLE_EQ(Quantity::Inch.getValue(), 25.4);
            EXPECT_DOUBLE_EQ(Quantity::Foot.getValue(), 304.8);
            EXPECT_EQ(Quantity::Metre.getUnit(), Unit::Length);
        }

        /**
         * @brief 钉住：加减与比较要求单位一致，不一致时报错而不是按数值硬算
         */
        TEST(QuantityTest, AdditionRequiresMatchingUnits)
        {
            const Quantity oneMetre      = Quantity::Metre;
            const Quantity oneMillimetre = Quantity::MilliMetre;
            EXPECT_DOUBLE_EQ((oneMetre + oneMillimetre).getValue(), 1001.0);

            const Quantity oneSecond = Quantity::Second;
            EXPECT_THROW(static_cast<void>(oneMetre + oneSecond), Base::UnitsMismatchError);
            EXPECT_THROW(static_cast<void>(oneMetre - oneSecond), Base::UnitsMismatchError);
            EXPECT_THROW(static_cast<void>(oneMetre < oneSecond), Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：乘除只合并量纲，幂次同时作用于数值与量纲
         */
        TEST(QuantityTest, MultiplicationDividesUnitsAndPowers)
        {
            const Quantity area = Quantity::Metre * Quantity::Metre;
            EXPECT_EQ(area.getUnit(), Unit::Area);
            EXPECT_DOUBLE_EQ(area.getValue(), 1.0e6);

            const Quantity squared = Quantity(2.0, Unit::Length).pow(2.0);
            EXPECT_EQ(squared.getUnit(), Unit::Area);
            EXPECT_DOUBLE_EQ(squared.getValue(), 4.0);

            // 幂次带单位属于用法错误
            EXPECT_THROW(static_cast<void>(Quantity(2.0).pow(Quantity(1.0, Unit::Length))), Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：无效值用 NaN 表示，可被 isValid() 识别
         */
        TEST(QuantityTest, InvalidValueRoundTrip)
        {
            Quantity value{1.0};
            EXPECT_TRUE(value.isValid());

            value.setInvalid();
            EXPECT_FALSE(value.isValid());
            EXPECT_TRUE(std::isnan(value.getValue()));
        }

        /**
         * @brief 钉住：toString 带引号便于回填，toNumber 只给数值
         */
        TEST(QuantityTest, FormattingKeepsUnitAndQuotes)
        {
            const Quantity value{25.4, Unit::Length};
            EXPECT_EQ(value.toNumber(QuantityFormat(QuantityFormat::NumberFormat::Fixed, 1)), "25.4");
            EXPECT_EQ(value.toString(QuantityFormat(QuantityFormat::NumberFormat::Fixed, 1)), "'25.4 mm'");
        }

        /**
         * @brief 钉住：单位文本构造走解析器，无法解析时退化为无量纲且数值归零
         */
        TEST(QuantityTest, ConstructFromUnitText)
        {
            const Quantity value{2.0, std::string("in")};
            EXPECT_DOUBLE_EQ(value.getValue(), 50.8);
            EXPECT_EQ(value.getUnit(), Unit::Length);

            // 非法单位文本不抛异常，但会退化成无量纲的零值
            const Quantity broken{2.0, std::string("not-a-unit")};
            EXPECT_DOUBLE_EQ(broken.getValue(), 0.0);
            EXPECT_EQ(broken.getUnit(), Unit::One);
        }

        /**
         * @brief 钉住：无量纲判定只看量纲是否为单位一
         */
        TEST(QuantityTest, DimensionlessDetection)
        {
            EXPECT_TRUE(Quantity(2.0).isDimensionless());
            EXPECT_FALSE(Quantity(2.0, Unit::Length).isDimensionless());
            EXPECT_TRUE(Quantity(2.0, Unit::Length).isDimensionlessOrUnit(Unit::Length));
        }

    } // namespace
}     // namespace ExpressionEngine::Units
