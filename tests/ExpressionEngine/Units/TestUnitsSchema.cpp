// 本文件覆盖单位方案的枚举、切换与换算排版。

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/UnitsApi.h>
#include <ExpressionEngine/Units/UnitsSchema.h>
#include <ExpressionEngine/Units/UnitsSchemasData.h>

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

        /**
         * @brief 钉住：建筑英制把长度写成「英尺' 英寸" + 分数"」，分数约到最简
         */
        TEST(UnitsApiTest, FractionalLengthShowsFeetInchesAndRemainder)
        {
            UnitsApi::setSchema("ImperialBuilding");
            UnitsApi::setDenominator(16);

            // 100 mm = 3.937 in，按 1/16 英寸落进 3" + 15/16"
            EXPECT_EQ(UnitsApi::schemaTranslate(Quantity(100.0, Unit::Length)), "3\" + 15/16\"");
            // 4/16 要约成 1/4
            EXPECT_EQ(UnitsApi::schemaTranslate(Quantity(25.4 * 1.25, Unit::Length)), "1\" + 1/4\"");
            // 整英尺只剩英尺段，英寸与分数都省略
            EXPECT_EQ(UnitsApi::schemaTranslate(Quantity(304.8, Unit::Length)), "1'");
            // 不足一英寸时只写分数
            UnitsApi::setDenominator(8);
            EXPECT_EQ(UnitsApi::schemaTranslate(Quantity(25.4 * 0.375, Unit::Length)), "3/8\"");
            // 负值把符号放在最前，余量用减号连接
            UnitsApi::setDenominator(16);
            EXPECT_EQ(UnitsApi::schemaTranslate(Quantity(-100.0, Unit::Length)), "-3\" - 15/16\"");
            // 零就是零
            EXPECT_EQ(UnitsApi::schemaTranslate(Quantity(0.0, Unit::Length)), "0");
        }

        /**
         * @brief 钉住：分母为 0 会把任何长度都排成 "0"，必须报错而不是静默出零
         */
        TEST(UnitsApiTest, ZeroFractionDenominatorIsRejected)
        {
            UnitsApi::setSchema("ImperialBuilding");
            UnitsApi::setDenominator(0);

            const Quantity value{100.0, Unit::Length};
            EXPECT_THROW(static_cast<void>(UnitsApi::schemaTranslate(value)), Base::ValueError);

            // 传负数恢复方案默认分母（建筑英制默认 1/8 英寸），排版照常用
            UnitsApi::setDenominator(-1);
            EXPECT_GT(UnitsApi::getDenominator(), 1);
            EXPECT_EQ(UnitsApi::schemaTranslate(value), "3\" + 7/8\"");
        }

        /**
         * @brief 钉住：精度与分母未显式设置时回落到方案默认，方案清单三张表同序
         */
        TEST(UnitsApiTest, FormatSettingsFallBackToSchemaDefaults)
        {
            UnitsApi::setSchema("Internal");
            UnitsApi::setDecimals(-1);
            UnitsApi::setDenominator(-1);

            // 默认格式自己不存精度与分母，读到的就是当前方案的默认值
            EXPECT_EQ(QuantityFormat().getPrecision(), UnitsApi::getDecimals());
            EXPECT_EQ(QuantityFormat().getDenominator(), UnitsApi::getDenominator());

            UnitsApi::setDecimals(4);
            EXPECT_EQ(UnitsApi::getDecimals(), 4);
            EXPECT_EQ(QuantityFormat().getPrecision(), 4);
            UnitsApi::setDecimals(-1);

            const auto names        = UnitsApi::getNames();
            const auto descriptions = UnitsApi::getDescriptions();
            ASSERT_EQ(descriptions.size(), names.size());
            EXPECT_FALSE(descriptions.front().empty());

            // 单个方案的元数据可以按编号取出来，供宿主自己列表展示
            const std::unique_ptr<UnitsSchema> schema = UnitsApi::createSchema(0);
            ASSERT_NE(schema, nullptr);
            EXPECT_EQ(schema->getNumber(), 0U);
            EXPECT_EQ(schema->getName(), "Internal");
            EXPECT_FALSE(schema->getDescription().empty());
        }

        /**
         * @brief 钉住：宿主自带的方案数据包能整体装进门面，默认精度与分母随之生效
         * @details 门面没有替换入口时，「宿主可整体替换方案数据」这句承诺只能靠绕开门面自己构造
         *          UnitsSchema 兑现，而排版的默认精度与分母仍然读门面上的全局状态。
         */
        TEST(UnitsApiTest, HostPackReplacesTheGlobalSchemaCollection)
        {
            UnitsSchemaSpecification custom;
            custom.number                 = 42;
            custom.name                   = "Custom";
            custom.basicLengthUnitString  = "in";
            custom.description            = "host pack";
            custom.translationSpecifications["Length"] = {{0, "in", 1.0}};

            const UnitsSchemasDataPack pack{.specifications = {custom}, .defaultDecimals = 5, .defaultDenominator = 32};

            UnitsApi::setDecimals(-1);
            UnitsApi::setDenominator(-1);
            UnitsApi::applyPack(pack);

            EXPECT_EQ(UnitsApi::count(), 1U);
            EXPECT_EQ(UnitsApi::getNames(), (std::vector<std::string>{"Custom"}));
            // 数据包没标记 isDefault，门面的当前方案就是列表第一个
            EXPECT_EQ(UnitsApi::getDefaultSchemaNumber(), 42U);
            EXPECT_EQ(UnitsApi::getBasicLengthUnit(), "in");

            // 未显式设置时，精度与分母都取自新数据包
            EXPECT_EQ(UnitsApi::getDecimals(), 5);
            EXPECT_EQ(UnitsApi::getDenominator(), 32);
            EXPECT_EQ(UnitsApi::schemaTranslate(Quantity(2.0, Unit::Length)), "2.00000 in");

            // 显式设置过的精度跨数据包保留，不被替换悄悄改掉
            UnitsApi::setDecimals(1);
            UnitsApi::applyPack(pack);
            EXPECT_EQ(UnitsApi::getDecimals(), 1);

            UnitsApi::applyPack(UnitsSchemasData::unitSchemasDataPack);
            UnitsApi::setDecimals(-1);
            UnitsApi::setDenominator(-1);
            EXPECT_EQ(UnitsApi::count(), 10U);
        }

        /**
         * @brief 钉住：空数据包不能装进门面
         */
        TEST(UnitsApiTest, EmptyHostPackIsRejected)
        {
            const UnitsSchemasDataPack emptyPack{.specifications = {}, .defaultDecimals = 2, .defaultDenominator = 8};

            EXPECT_THROW(UnitsApi::applyPack(emptyPack), Base::NameError);
            // 抛错后门面上的方案集合原样可用
            EXPECT_EQ(UnitsApi::count(), 10U);
        }

    }
} // namespace ExpressionEngine::Units
