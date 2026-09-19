// 本文件覆盖值模型 Value 的类型判定、转换与拒绝面：类型不符、量纲不匹配、文本解析失败。

#include <gtest/gtest.h>

#include <string>
#include <variant>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Rotation.h>
#include <ExpressionEngine/Base/Vector3D.h>
#include <ExpressionEngine/Expression/Value.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Expression
{
    namespace
    {

        /// 取出数量；不是数量时让用例失败并给出实际类型
        Units::Quantity quantityOf(const Value &value)
        {
            const auto *quantity = std::get_if<Units::Quantity>(&value);
            if (quantity == nullptr)
            {
                ADD_FAILURE() << "期望数量，实际是 " << std::string(valueTypeName(value));
                return {};
            }
            return *quantity;
        }

        /**
         * @brief 钉住：数值型与几何型的判定口径，以及类型名的报错文案
         */
        TEST(ValueTest, TypePredicateAndNames)
        {
            EXPECT_TRUE(isNumeric(Value(Units::Quantity(1.5))));
            EXPECT_TRUE(isNumeric(Value(2.5)));
            EXPECT_FALSE(isNumeric(Value(true)));
            EXPECT_FALSE(isNumeric(Value(Base::Vector3d(1.0, 2.0, 3.0))));

            EXPECT_TRUE(isGeometric(Value(Base::Vector3d(1.0, 2.0, 3.0))));
            EXPECT_TRUE(isGeometric(Value(Base::Rotation())));
            EXPECT_TRUE(isGeometric(Value(Base::Matrix4D())));
            EXPECT_FALSE(isGeometric(Value(Units::Quantity(1.0))));
            EXPECT_FALSE(isGeometric(Value(std::string("abc"))));

            EXPECT_EQ(valueTypeName(Value(Units::Quantity(1.0))), "数量");
            EXPECT_EQ(valueTypeName(Value(1.0)), "纯数");
            EXPECT_EQ(valueTypeName(Value(true)), "布尔");
            EXPECT_EQ(valueTypeName(Value(std::string("abc"))), "文本");
            EXPECT_EQ(valueTypeName(Value(Base::Vector3d(1.0, 2.0, 3.0))), "向量");
            EXPECT_EQ(valueTypeName(Value(Base::Rotation())), "旋转");
            EXPECT_EQ(valueTypeName(Value(Base::Matrix4D())), "矩阵");
        }

        /**
         * @brief 钉住：数量、纯数与布尔都能按数量取出，布尔按 0/1 处理
         */
        TEST(ValueTest, QuantityConversionAcceptsNumbersAndBoolean)
        {
            EXPECT_DOUBLE_EQ(quantityOf(toQuantity(Value(Units::Quantity(2.0, Units::Unit::Length)), "测试")).getValue(), 2.0);
            EXPECT_EQ(quantityOf(toQuantity(Value(Units::Quantity(2.0, Units::Unit::Length)), "测试")).getUnit(), Units::Unit::Length);
            EXPECT_DOUBLE_EQ(quantityOf(toQuantity(Value(2.5), "测试")).getValue(), 2.5);
            EXPECT_TRUE(quantityOf(toQuantity(Value(2.5), "测试")).isDimensionless());
            EXPECT_DOUBLE_EQ(quantityOf(toQuantity(Value(true), "测试")).getValue(), 1.0);
            EXPECT_DOUBLE_EQ(quantityOf(toQuantity(Value(false), "测试")).getValue(), 0.0);
        }

        /**
         * @brief 钉住：文本能解析成数量，解析失败时报解析错并带上原文
         */
        TEST(ValueTest, QuantityConversionParsesText)
        {
            const Units::Quantity parsed = toQuantity(Value(std::string("1.5 mm")), "属性 Length");
            EXPECT_DOUBLE_EQ(parsed.getValue(), 1.5);
            EXPECT_EQ(parsed.getUnit(), Units::Unit::Length);

            const Units::Quantity angle = toQuantity(Value(std::string("90 deg")), "属性 Angle");
            EXPECT_EQ(angle.getUnit(), Units::Unit::Angle);

            EXPECT_THROW(static_cast<void>(toQuantity(Value(std::string("abc")), "属性 Length")), Base::ParserError);
            EXPECT_THROW(static_cast<void>(toQuantity(Value(std::string("12 个")), "属性 Length")), Base::ParserError);
        }

        /**
         * @brief 钉住：几何值不能当数量用，拒绝而不是静默取某个分量
         */
        TEST(ValueTest, QuantityConversionRejectsGeometry)
        {
            EXPECT_THROW(static_cast<void>(toQuantity(Value(Base::Vector3d(1.0, 2.0, 3.0)), "加法左操作数")), Base::TypeError);
            EXPECT_THROW(static_cast<void>(toQuantity(Value(Base::Matrix4D()), "加法左操作数")), Base::TypeError);
            EXPECT_THROW(static_cast<void>(toQuantity(Value(Base::Rotation()), "加法左操作数")), Base::TypeError);
        }

        /**
         * @brief 钉住：纯数通道只接受无量纲取值，带量纲的量要调用方显式按量处理
         */
        TEST(ValueTest, DoubleConversionRejectsDimensionedAndText)
        {
            EXPECT_DOUBLE_EQ(toDouble(Value(3.0), "测试"), 3.0);
            EXPECT_DOUBLE_EQ(toDouble(Value(true), "测试"), 1.0);
            EXPECT_DOUBLE_EQ(toDouble(Value(Units::Quantity(3.0)), "测试"), 3.0);

            EXPECT_THROW(static_cast<void>(toDouble(Value(Units::Quantity(3.0, Units::Unit::Length)), "测试")), Base::TypeError);
            EXPECT_THROW(static_cast<void>(toDouble(Value(std::string("3")), "测试")), Base::TypeError);
            EXPECT_THROW(static_cast<void>(toDouble(Value(Base::Vector3d(1.0, 2.0, 3.0)), "测试")), Base::TypeError);
        }

        /**
         * @brief 钉住：真值判定接受布尔与数值，文本与几何值拒绝
         */
        TEST(ValueTest, BooleanConversion)
        {
            EXPECT_TRUE(toBool(Value(true), "测试"));
            EXPECT_FALSE(toBool(Value(false), "测试"));
            EXPECT_TRUE(toBool(Value(Units::Quantity(2.0)), "测试"));
            EXPECT_FALSE(toBool(Value(Units::Quantity(0.0)), "测试"));
            EXPECT_TRUE(toBool(Value(0.5), "测试"));
            EXPECT_FALSE(toBool(Value(0.0), "测试"));

            EXPECT_THROW(static_cast<void>(toBool(Value(std::string("true")), "条件表达式")), Base::TypeError);
            EXPECT_THROW(static_cast<void>(toBool(Value(Base::Vector3d(1.0, 2.0, 3.0)), "条件表达式")), Base::TypeError);
        }

        /**
         * @brief 钉住：文本排版口径——布尔用中文、文本原样、向量紧凑、数量走单位方案
         */
        TEST(ValueTest, TextFormatting)
        {
            EXPECT_EQ(toString(Value(true)), "真");
            EXPECT_EQ(toString(Value(false)), "假");
            EXPECT_EQ(toString(Value(std::string("零件 A"))), "零件 A");
            EXPECT_EQ(toString(Value(Base::Vector3d(1.0, 2.0, 3.0))), "(1, 2, 3)");

            const std::string lengthText = toString(Value(Units::Quantity(1.5, Units::Unit::Length)));
            EXPECT_NE(lengthText.find("1.5"), std::string::npos);
        }

        /**
         * @brief 钉住：相等判定只在同类型内比较，数量按数值与量纲同时比较，几何值按容差比较
         */
        TEST(ValueTest, EqualityRequiresSameType)
        {
            EXPECT_TRUE(valuesEqual(Value(Units::Quantity(1.0, Units::Unit::Length)), Value(Units::Quantity(1.0, Units::Unit::Length))));
            // 量纲不同时返回 false 而不是抛错，供 == 运算符复用
            EXPECT_FALSE(valuesEqual(Value(Units::Quantity(1.0, Units::Unit::Length)), Value(Units::Quantity(1.0, Units::Unit::TimeSpan))));
            // 类型不同一律不相等，不做跨类型换算
            EXPECT_FALSE(valuesEqual(Value(Units::Quantity(1.0)), Value(1.0)));
            EXPECT_FALSE(valuesEqual(Value(1.0), Value(true)));
            EXPECT_TRUE(valuesEqual(Value(true), Value(true)));
            EXPECT_TRUE(valuesEqual(Value(std::string("a")), Value(std::string("a"))));
            EXPECT_FALSE(valuesEqual(Value(std::string("a")), Value(std::string("b"))));

            // 几何值按容差比较：亚微米级差异算相等，明显差异算不等
            EXPECT_TRUE(valuesEqual(Value(Base::Vector3d(0.0, 0.0, 0.0)), Value(Base::Vector3d(1e-9, 0.0, 0.0))));
            EXPECT_FALSE(valuesEqual(Value(Base::Vector3d(0.0, 0.0, 0.0)), Value(Base::Vector3d(1e-3, 0.0, 0.0))));
            EXPECT_TRUE(valuesEqual(Value(Base::Rotation()), Value(Base::Rotation(Base::Vector3d(0.0, 0.0, 1.0), 0.0))));
        }

        /**
         * @brief 钉住：大小比较要求可比较且量纲一致，量纲不符报错而不是按数值硬比
         */
        TEST(ValueTest, LessThanRequiresComparableValues)
        {
            EXPECT_TRUE(valueLessThan(Value(Units::Quantity(1.0, Units::Unit::Length)), Value(Units::Quantity(2.0, Units::Unit::Length))));
            EXPECT_FALSE(valueLessThan(Value(Units::Quantity(2.0, Units::Unit::Length)), Value(Units::Quantity(1.0, Units::Unit::Length))));
            EXPECT_TRUE(valueLessThan(Value(1.0), Value(2.0)));
            EXPECT_TRUE(valueLessThan(Value(false), Value(true)));
            EXPECT_TRUE(valueLessThan(Value(std::string("a")), Value(std::string("b"))));

            // 量纲不同：纯数按无量纲处理，与长度量不可比
            EXPECT_THROW(static_cast<void>(valueLessThan(Value(Units::Quantity(1.0, Units::Unit::Length)), Value(Units::Quantity(1.0, Units::Unit::TimeSpan)))),
                         Base::UnitsMismatchError);
            EXPECT_THROW(static_cast<void>(valueLessThan(Value(Units::Quantity(1.0, Units::Unit::Length)), Value(1.0))), Base::UnitsMismatchError);
            // 几何值之间没有大小关系
            EXPECT_THROW(static_cast<void>(valueLessThan(Value(Base::Vector3d(1.0, 0.0, 0.0)), Value(Base::Vector3d(2.0, 0.0, 0.0)))), Base::TypeError);
            EXPECT_THROW(static_cast<void>(valueLessThan(Value(std::string("a")), Value(1.0))), Base::TypeError);
        }

    } // namespace
}     // namespace ExpressionEngine::Expression
