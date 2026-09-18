// 本文件覆盖单位方案的枚举、切换与换算排版。

#include <gtest/gtest.h>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Units/UnitsApi.h>

namespace ExpressionEngine::Units
{
    namespace
    {

        /**
         * @brief 钉住：内置方案齐全，默认方案是 Internal
         */
        TEST(UnitsApiTest, BuiltInSchemasAreListedByNumber)
        {
            const auto names = UnitsApi::getNames();
            ASSERT_EQ(names.size(), 10U);
            // 列表按方案编号排序，编号 0 的 Internal 排在最前
            EXPECT_EQ(names.front(), "Internal");
            EXPECT_EQ(UnitsApi::getDefaultSchemaNumber(), 0U);
            EXPECT_EQ(UnitsApi::count(), 10U);

            UnitsApi::setSchema(UnitsApi::getDefaultSchemaNumber());
            EXPECT_EQ(UnitsApi::getBasicLengthUnit(), "mm");
        }

        /**
         * @brief 钉住：按名与按编号都能切换方案，找不到时报错
         */
        TEST(UnitsApiTest, SchemaSelectionRejectsUnknownNames)
        {
            UnitsApi::setSchema("MKS");
            EXPECT_EQ(UnitsApi::getBasicLengthUnit(), "m");

            UnitsApi::setSchema(0U);
            EXPECT_EQ(UnitsApi::getBasicLengthUnit(), "mm");

            EXPECT_THROW(UnitsApi::setSchema("NoSuchSchema"), Base::NameError);
            EXPECT_THROW(UnitsApi::setSchema(99U), Base::NameError);
        }

        /**
         * @brief 钉住：换算结果由方案的目标单位与数值共同决定
         */
        TEST(UnitsApiTest, SchemaTranslateConvertsToPreferredUnit)
        {
            UnitsApi::setSchema("Internal");
            UnitsApi::setDecimals(2);

            double            factor = 0.0;
            std::string       unitString;
            const std::string translated = UnitsApi::schemaTranslate(Quantity::Metre, factor, unitString);

            EXPECT_DOUBLE_EQ(factor, 1.0);
            EXPECT_EQ(unitString, "mm");
            EXPECT_EQ(translated, "1000.00 mm");

            // 英制方案用英寸符号 " 作为长度单位，排版时不加空格
            UnitsApi::setSchema("Imperial");
            const std::string imperial = UnitsApi::schemaTranslate(Quantity::Inch, factor, unitString);
            EXPECT_EQ(unitString, "\"");
            EXPECT_EQ(imperial, "1.00\"");
        }

        /**
         * @brief 钉住：角度的单位符号与数值之间不留空格
         */
        TEST(UnitsApiTest, AngleSymbolIsNotSeparatedBySpace)
        {
            UnitsApi::setSchema("Internal");
            UnitsApi::setDecimals(2);

            const std::string translated = UnitsApi::schemaTranslate(Quantity::Degree);
            EXPECT_EQ(translated, "1.00°");
        }

        /**
         * @brief 钉住：精度设置会体现在排版结果上
         */
        TEST(UnitsApiTest, PrecisionAffectsFormatting)
        {
            UnitsApi::setSchema("Internal");
            UnitsApi::setDecimals(4);
            EXPECT_EQ(UnitsApi::schemaTranslate(Quantity::Metre), "1000.0000 mm");

            // 负数表示恢复方案默认精度
            UnitsApi::setDecimals(-1);
            EXPECT_EQ(UnitsApi::schemaTranslate(Quantity::Metre), "1000.00 mm");
        }

        /**
         * @brief 钉住：多单位方案的标记能被查询到
         */
        TEST(UnitsApiTest, MultiUnitFlagsFollowSelectedSchema)
        {
            UnitsApi::setSchema("ImperialBuilding");
            EXPECT_TRUE(UnitsApi::isMultiUnitLength());
            EXPECT_FALSE(UnitsApi::isMultiUnitAngle());

            UnitsApi::setSchema("ImperialCivil");
            EXPECT_TRUE(UnitsApi::isMultiUnitAngle());
            EXPECT_FALSE(UnitsApi::isMultiUnitLength());

            UnitsApi::setSchema(UnitsApi::getDefaultSchemaNumber());
        }

    } // namespace
} // namespace ExpressionEngine::Units
