// 本文件直测内置单位方案数据层：度分秒与分数的排版辅助函数、特殊函数登记表，
// 以及内置数据表自身的完整性（排版路径依赖的不变量一旦破了，这里先红）。

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <set>
#include <string>

#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/Unit.h>
#include <ExpressionEngine/Units/UnitsSchema.h>
#include <ExpressionEngine/Units/UnitsSchemasData.h>

namespace ExpressionEngine::Units::UnitsSchemasData
{
    namespace
    {

        /**
         * @brief 钉住：度分秒按绝对值逐级拆分再补符号
         * @details 分秒为零时省略对应部分；负角若直接取整，-1.5° 会被拆成 -2° 加 30′。
         */
        TEST(UnitsSchemasDataTest, DegreesMinutesSecondsUseAbsoluteMagnitude)
        {
            EXPECT_EQ(toDegreesMinutesSeconds(0.0), "0°");
            EXPECT_EQ(toDegreesMinutesSeconds(1.5), "1°30′");
            EXPECT_EQ(toDegreesMinutesSeconds(12.5125), "12°30′45″");
            EXPECT_EQ(toDegreesMinutesSeconds(-1.5), "-1°30′");
            EXPECT_EQ(toDegreesMinutesSeconds(-12.5125), "-12°30′45″");
        }

        /**
         * @brief 钉住：求最大公约数在含 0 的入参上也给出可用值
         * @details 分数排版拿它约分，分母一侧落到 0 时不能返回 0（随后要按它做除法）。
         */
        TEST(UnitsSchemasDataTest, GreatestCommonDenominatorCoversZeroArguments)
        {
            EXPECT_EQ(greatestCommonDenominator(0U, 0U), 0U);
            EXPECT_EQ(greatestCommonDenominator(0U, 5U), 5U);
            EXPECT_EQ(greatestCommonDenominator(5U, 0U), 5U);
            EXPECT_EQ(greatestCommonDenominator(4U, 8U), 4U);
            EXPECT_EQ(greatestCommonDenominator(15U, 4U), 1U);
            EXPECT_EQ(greatestCommonDenominator(16U, 12U), 4U);
        }

        /**
         * @brief 钉住：登记表按名字派发，未登记的名字返回空串交由调用方降级
         */
        TEST(UnitsSchemasDataTest, RunSpecialDispatchesByRegisteredName)
        {
            double      factor     = 0.0;
            std::string unitString;

            EXPECT_EQ(runSpecial("toDMS", 12.5125, 2, 8, factor, unitString), "12°30′45″");
            EXPECT_DOUBLE_EQ(factor, 1.0);
            EXPECT_EQ(unitString, "deg");

            // 换算因子与单位串由被调用的函数写回，分数那一支报的是英寸刻度
            EXPECT_EQ(runSpecial("toFractional", 25.4, 2, 8, factor, unitString), "1\"");
            EXPECT_DOUBLE_EQ(factor, 25.4);
            EXPECT_EQ(unitString, "in");

            EXPECT_TRUE(runSpecial("toNoSuchThing", 1.0, 2, 8, factor, unitString).empty());
        }

        /**
         * @brief 钉住：内置数据包只有一个默认方案，编号与名字都不重
         */
        TEST(UnitsSchemasDataTest, BuiltInPackHasUniqueKeysAndExactlyOneDefault)
        {
            std::set<std::size_t> numbers;
            std::set<std::string> names;
            std::size_t           defaultCount = 0;

            for (const auto &schema : unitSchemasDataPack.specifications)
            {
                EXPECT_TRUE(numbers.insert(schema.number).second) << "方案编号重复：" << schema.number;
                EXPECT_FALSE(schema.name.empty());
                EXPECT_TRUE(names.insert(schema.name).second) << "方案名重复：" << schema.name;
                EXPECT_FALSE(schema.basicLengthUnitString.empty()) << schema.name << " 没有基准长度单位";

                if (schema.isDefault)
                {
                    ++defaultCount;
                }
            }

            EXPECT_EQ(defaultCount, 1U);
        }

        /**
         * @brief 钉住：每条换算表都以阈值 0 的兜底条目结尾，且特殊条目都能被解析
         * @details 排版取第一个「阈值大于待换算值」的条目，末尾没有兜底条目时大数值会直接报错；
         *          换算因子为 0 的条目靠登记表里的函数名或自带回调取值，两者都没有就排不出东西。
         */
        TEST(UnitsSchemasDataTest, BuiltInRowsFallBackAndResolveSpecials)
        {
            for (const auto &schema: unitSchemasDataPack.specifications)
            {
                for (const auto &[unitTypeName, rows]: schema.translationSpecifications)
                {
                    ASSERT_FALSE(rows.empty()) << schema.name << " 的 " << unitTypeName << " 换算是空的";

                    EXPECT_EQ(rows.back().threshold, 0.0) << schema.name << " 的 " << unitTypeName << " 末尾缺阈值 0 的兜底条目";

                    for (const auto &row: rows)
                    {
                        EXPECT_FALSE(row.unitString.empty()) << schema.name << " 的 " << unitTypeName << " 有条目没写单位串";

                        if (row.factor == 0.0)
                        {
                            EXPECT_TRUE(specials.contains(row.unitString) || row.callback != nullptr)
                                    << schema.name << " 的 " << unitTypeName << " 引用了未登记的特殊函数 " << row.unitString;
                        }
                    }
                }
            }
        }

        /**
         * @brief 造一个只含角度条目的方案，用于比较特殊通道与常规通道的取值
         * @param row 角度换算条目
         * @return 方案定义
         */
        UnitsSchemaSpecification makeAngleSchema(const UnitTranslationSpecification &row)
        {
            UnitsSchemaSpecification specification;
            specification.number                             = 0;
            specification.name                               = "AngleOnly";
            specification.basicLengthUnitString              = "mm";
            specification.translationSpecifications["Angle"] = {row};

            return specification;
        }

        /**
         * @brief 钉住：非有限值不进特殊通道，退回常规排版
         * @details 度分秒与分数都要把数值落成整数刻度，非有限值会撞上有未定义行为的整型转换。
         */
        TEST(UnitsSchemasDataTest, NonFiniteValuesSkipTheSpecialChannel)
        {
            const UnitsSchema special{makeAngleSchema({0, "toDMS", 0.0})};
            // 兜底路径按量自身的单位串排版，因此对照方案写 "deg" 而不是 "°"
            const UnitsSchema notConverted{makeAngleSchema({0, "deg", 1.0})};
            const UnitsSchema plain{makeAngleSchema({0, "°", 1.0})};

            for (const double value: {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                                      -std::numeric_limits<double>::infinity()})
            {
                Quantity quantity{value, Unit::Angle};
                quantity.setFormat(QuantityFormat{QuantityFormat::NumberFormat::Fixed, 2});

                EXPECT_EQ(special.translate(quantity), notConverted.translate(quantity)) << "非有限值没有退回常规排版";
            }

            // 有限值照旧走度分秒，别把整条通道一起关掉
            Quantity finite{1.5, Unit::Angle};
            finite.setFormat(QuantityFormat{QuantityFormat::NumberFormat::Fixed, 2});
            EXPECT_EQ(special.translate(finite), "1°30′");
            EXPECT_EQ(plain.translate(finite), "1.50°");
        }

    }
} // namespace ExpressionEngine::Units::UnitsSchemasData
