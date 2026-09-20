// 本文件的用例一律用手工构造的方案数据，不读内置表：覆盖「宿主自带方案」这条通路。

#include <gtest/gtest.h>

#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/Unit.h>
#include <ExpressionEngine/Units/UnitsSchema.h>
#include <ExpressionEngine/Units/UnitsSchemas.h>

namespace ExpressionEngine::Units
{
    namespace
    {

        /**
         * @brief 造一个只含长度条目的最小方案，免得用例被无关字段淹没
         * @param number 方案编号
         * @param name 方案名
         * @param rows 长度换算条目
         * @param isDefault 是否标记为默认方案
         * @return 方案定义
         */
        UnitsSchemaSpecification makeLengthSchema(const std::size_t number, const std::string &name, std::vector<UnitTranslationSpecification> rows,
                                                  const bool isDefault = false)
        {
            UnitsSchemaSpecification specification{.number = number, .name = name, .basicLengthUnitString = "mm", .isDefault = isDefault};
            specification.translationSpecifications["Length"] = std::move(rows);

            return specification;
        }

        /**
         * @brief 造一条「换算因子为 0，由特殊函数名或回调接管排版」的兜底条目
         * @param name 特殊函数名
         * @param callback 名字未登记时使用的回调，默认为空
         * @return 换算条目
         */
        UnitTranslationSpecification makeSpecialRow(std::string name, std::function<std::string(double)> callback = {})
        {
            return UnitTranslationSpecification{0, std::move(name), 0.0, std::move(callback)};
        }

        /**
         * @brief 执行动作并取回它抛出的 Base::Exception 消息；没抛就算用例失败
         */
        template <typename Action>
        std::string thrownMessage(const Action &action)
        {
            try
            {
                action();
            }
            catch (const Base::Exception &error)
            {
                return error.message();
            }

            ADD_FAILURE() << "预期抛出 Base::Exception，实际正常返回";
            return {};
        }

        /// 只含一个方案的描述用作多个用例的共用数据包内容
        const UnitsSchemaSpecification markedSchema = makeLengthSchema(2, "Marked", {{0, "mm", 1.0}}, true);

        /**
         * @brief 钉住：数据包没标记 isDefault 时取列表第一个，而不是抛错
         * @details 构造函数就要取默认方案，所以这条回落不成立的话只提供单个方案的宿主连对象都建不出来。
         */
        TEST(UnitsSchemasTest, PackWithoutDefaultFlagFallsBackToFirstEntry)
        {
            const UnitsSchemasDataPack pack{
                    .specifications     = {makeLengthSchema(3, "Third", {{0, "mm", 1.0}}), makeLengthSchema(1, "First", {{0, "m", 1.0}})},
                    .defaultDecimals    = 3,
                    .defaultDenominator = 16,
            };

            UnitsSchemas schemas{pack};

            // 按列表顺序取第一个，不是按编号取最小
            EXPECT_EQ(schemas.specification().name, "Third");
            EXPECT_EQ(schemas.currentSchema()->getName(), "Third");

            schemas.select("First");
            EXPECT_EQ(schemas.currentSchema()->getName(), "First");
        }

        /**
         * @brief 钉住：标记了 isDefault 时按标记取，不受列表顺序影响
         */
        TEST(UnitsSchemasTest, MarkedDefaultWinsOverListOrder)
        {
            const UnitsSchemasDataPack pack{
                    .specifications     = {makeLengthSchema(3, "Third", {{0, "mm", 1.0}}), markedSchema},
                    .defaultDecimals    = 2,
                    .defaultDenominator = 8,
            };

            UnitsSchemas schemas{pack};

            EXPECT_EQ(schemas.specification().name, "Marked");
            EXPECT_EQ(schemas.currentSchema()->getName(), "Marked");
        }

        /**
         * @brief 钉住：空数据包在构造阶段就报错，而不是回落到不存在的第一个方案
         */
        TEST(UnitsSchemasTest, EmptyPackIsRejected)
        {
            const UnitsSchemasDataPack emptyPack{.specifications = {}, .defaultDecimals = 2, .defaultDenominator = 8};

            const std::string message = thrownMessage([&emptyPack]
            {
                static_cast<void>(UnitsSchemas{emptyPack});
            });

            EXPECT_NE(message.find("没有任何方案"), std::string::npos) << "报错文案要指明原因，实际：" << message;
        }

        /**
         * @brief 钉住：按名与按编号查不到时，报错文案带上查的是哪个名字或编号
         */
        TEST(UnitsSchemasTest, LookupFailureNamesTheKey)
        {
            const UnitsSchemasDataPack pack{
                    .specifications     = {markedSchema},
                    .defaultDecimals    = 2,
                    .defaultDenominator = 8,
            };

            UnitsSchemas schemas{pack};

            const std::string byName = thrownMessage([&schemas]
            {
                static_cast<void>(schemas.specification("NoSuchSchema"));
            });
            EXPECT_NE(byName.find("NoSuchSchema"), std::string::npos) << "实际：" << byName;
            EXPECT_THROW(schemas.select("NoSuchSchema"), Base::NameError);

            const std::string byNumber = thrownMessage([&schemas]
            {
                static_cast<void>(schemas.specification(99U));
            });
            EXPECT_NE(byNumber.find("99"), std::string::npos) << "实际：" << byNumber;
            EXPECT_THROW(schemas.select(99U), Base::NameError);
        }

        /**
         * @brief 钉住：名字与描述按方案编号排序，没有描述的返回空串
         */
        TEST(UnitsSchemasTest, NamesAndDescriptionsFollowSchemaNumber)
        {
            UnitsSchemaSpecification withoutDescription = makeLengthSchema(1, "One", {{0, "mm", 1.0}});
            withoutDescription.description              = nullptr;

            UnitsSchemaSpecification thirdSchema = makeLengthSchema(3, "Three", {{0, "mm", 1.0}});
            thirdSchema.description              = "third";

            UnitsSchemaSpecification fifthSchema = makeLengthSchema(5, "Five", {{0, "mm", 1.0}});
            fifthSchema.description              = "fifth";

            const UnitsSchemasDataPack pack{
                    .specifications     = {fifthSchema, withoutDescription, thirdSchema},
                    .defaultDecimals    = 2,
                    .defaultDenominator = 8,
            };

            const UnitsSchemas schemas{pack};

            EXPECT_EQ(schemas.count(), 3U);
            EXPECT_EQ(schemas.names(), (std::vector<std::string>{"One", "Three", "Five"}));

            const std::vector<std::string> descriptions = schemas.descriptions();
            ASSERT_EQ(descriptions.size(), 3U);
            EXPECT_TRUE(descriptions.front().empty());
            EXPECT_FALSE(descriptions.back().empty());
        }

        /**
         * @brief 钉住：数据包自带的精度与分母能被读出，分母可改
         */
        TEST(UnitsSchemasTest, PackDefaultsAreReadableAndOverridable)
        {
            const UnitsSchemasDataPack pack{
                    .specifications     = {markedSchema},
                    .defaultDecimals    = 4,
                    .defaultDenominator = 32,
            };

            UnitsSchemas schemas{pack};

            EXPECT_EQ(schemas.getDecimals(), 4U);
            EXPECT_EQ(schemas.defaultFractionDenominator(), 32U);

            schemas.setDefaultFractionDenominator(64);
            EXPECT_EQ(schemas.defaultFractionDenominator(), 64U);
        }

        /**
         * @brief 钉住：条目自带的 callback 接管排版，内置登记表优先于它
         * @details callback 是方案数据里的公开字段，不接上就等于宿主填了也不生效。
         */
        TEST(UnitsSchemaCustomPackTest, CallbackFormatsWhenNameIsNotRegistered)
        {
            const UnitsSchemaSpecification specification =
                    makeLengthSchema(0, "Custom", {makeSpecialRow("myFormat", [](const double value) { return std::format("<{}>", value); })});

            const UnitsSchema schema{specification};
            double            factor    = 0.0;
            std::string       unitString;
            const Quantity    quantity{2.5, Unit::Length};

            EXPECT_EQ(schema.translate(quantity, factor, unitString), "<2.5>");
            // 回调负责整段文本，换算因子与单位串保持未换算的默认值
            EXPECT_DOUBLE_EQ(factor, 1.0);
            EXPECT_EQ(unitString, quantity.getUnit().getString());
        }

        /**
         * @brief 钉住：函数名已在内置登记表里时走内置实现，回调不生效
         */
        TEST(UnitsSchemaCustomPackTest, RegisteredSpecialTakesPrecedenceOverCallback)
        {
            const UnitsSchemaSpecification specification =
                    makeLengthSchema(0, "Custom", {makeSpecialRow("toFractional", [](const double) { return std::string{"callback"}; })});

            Quantity       quantity{100.0, Unit::Length};
            QuantityFormat format;
            format.setPrecision(2);
            format.setDenominator(16);
            quantity.setFormat(format);

            const UnitsSchema schema{specification};
            double            factor    = 0.0;
            std::string       unitString;

            // 100 mm = 3.937 in，按 1/16 英寸落进 3" + 15/16"；因子与单位串由内置函数写回
            EXPECT_EQ(schema.translate(quantity, factor, unitString), "3\" + 15/16\"");
            EXPECT_DOUBLE_EQ(factor, 25.4);
            EXPECT_EQ(unitString, "in");
        }

        /**
         * @brief 钉住：名字不在登记表里、条目也没有回调时报错，而不是排出一段空文本
         */
        TEST(UnitsSchemaCustomPackTest, UnknownSpecialWithoutCallbackIsRejected)
        {
            const UnitsSchemaSpecification specification = makeLengthSchema(0, "Custom", {makeSpecialRow("toNope")});

            const UnitsSchema schema{specification};
            const Quantity    quantity{100.0, Unit::Length};

            const std::string message = thrownMessage([&schema, &quantity]
            {
                static_cast<void>(schema.translate(quantity));
            });

            EXPECT_NE(message.find("toNope"), std::string::npos) << "报错文案要指出未登记的名字，实际：" << message;
        }

        /**
         * @brief 钉住：换算表没有阈值 0 的兜底条目时报错
         */
        TEST(UnitsSchemaCustomPackTest, MissingFallbackRowIsRejected)
        {
            const UnitsSchemaSpecification specification = makeLengthSchema(0, "Custom", {{1e-9, "mm", 1.0}});

            const UnitsSchema schema{specification};
            const Quantity    quantity{1.0, Unit::Length};

            const std::string message = thrownMessage([&schema, &quantity]
            {
                static_cast<void>(schema.translate(quantity));
            });

            EXPECT_NE(message.find("兜底条目"), std::string::npos) << "实际：" << message;
        }

        /**
         * @brief 钉住：方案里没有该单位类型时按原单位不换算排版
         */
        TEST(UnitsSchemaCustomPackTest, UnknownUnitTypeFallsBackToOwnUnit)
        {
            const UnitsSchemaSpecification specification = makeLengthSchema(0, "Custom", {{0, "cm", 10.0}});

            Quantity       volume{1.0, Unit::Volume};
            QuantityFormat format;
            format.setPrecision(2);
            volume.setFormat(format);

            const UnitsSchema schema{specification};
            double            factor    = 0.0;
            std::string       unitString;
            const std::string ownUnit   = volume.getUnit().getString();

            // 换算表只有 Length，体积照自身单位写出来，数值不被换算
            EXPECT_EQ(schema.translate(volume, factor, unitString), "1.00 " + ownUnit);
            EXPECT_EQ(unitString, ownUnit);
            EXPECT_DOUBLE_EQ(factor, 1.0);
        }

    }
} // namespace ExpressionEngine::Units
