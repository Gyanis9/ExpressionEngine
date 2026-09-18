// 本文件覆盖 Unit 的量纲运算、文本表示与越界拒绝面。

#include <gtest/gtest.h>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Units
{
    namespace
    {

        /**
         * @brief 钉住：单位相等只看量纲指数，与书写形式无关
         */
        TEST(UnitTest, EqualityComparesExponentsOnly)
        {
            EXPECT_EQ(Unit::Length, Unit::Length);
            EXPECT_NE(Unit::Length, Unit::Area);
            EXPECT_EQ(Unit(1, 0, 0), Unit::Length);
            EXPECT_EQ(Unit(), Unit::One);
        }

        /**
         * @brief 钉住：乘除运算按分量合并指数
         */
        TEST(UnitTest, MultiplicationAndDivisionCombineExponents)
        {
            const Unit area = Unit::Length * Unit::Length;
            EXPECT_EQ(area, Unit::Area);
            EXPECT_EQ(area.exponents().at(0), 2);

            const Unit velocity = Unit::Length / Unit::TimeSpan;
            EXPECT_EQ(velocity, Unit::Velocity);
            EXPECT_EQ(velocity.exponents().at(0), 1);
            EXPECT_EQ(velocity.exponents().at(2), -1);

            // 全量纲相除回到无量纲
            EXPECT_EQ(Unit::Length / Unit::Length, Unit::One);
        }

        /**
         * @brief 钉住：复合写法按分子分母拼接，分母多分量要加括号
         */
        TEST(UnitTest, StringRepresentation)
        {
            EXPECT_EQ(Unit::Length.getString(), "mm");
            EXPECT_EQ(Unit::Area.getString(), "mm^2");
            EXPECT_EQ((Unit::Length / Unit::TimeSpan).getString(), "mm/s");
            EXPECT_EQ(Unit::Velocity.getTypeString(), "Velocity");
            // 无量纲单位的紧凑写法是空串，量纲信息全在数值里（与 FreeCAD 一致）
            EXPECT_EQ(Unit::One.getString(), "");
            EXPECT_EQ(Unit::One.getTypeString(), "1");
        }

        /**
         * @brief 钉住：开方要求每个指数都能整除，否则报错而不是给出近似结果
         */
        TEST(UnitTest, RootRejectsNonDivisibleExponent)
        {
            EXPECT_EQ(Unit::Area.root(2), Unit::Length);
            EXPECT_EQ(Unit::Volume.root(3), Unit::Length);

            // 长度指数是 1，开二次方无法表示
            EXPECT_THROW(static_cast<void>(Unit::Length.root(2)), Base::UnitsMismatchError);
            // 开方次数为 0 直接拒绝
            EXPECT_THROW(static_cast<void>(Unit::Length.root(0)), Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：幂次必须让每个指数落在整数格点上，分数次幂报错
         */
        TEST(UnitTest, PowRejectsFractionalExponent)
        {
            EXPECT_EQ(Unit::Length.pow(2), Unit::Area);

            // 0.5 次幂会让长度为 1 的指数变成 0.5
            EXPECT_THROW(static_cast<void>(Unit::Length.pow(0.5)), Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：指数越界时报错而不是静默截断
         */
        TEST(UnitTest, ExponentOverflowIsRejected)
        {
            // 上限判定沿用原实现：正方向「达到上限」即越界，负方向「低于下限」才越界，因此 -8 仍可表示
            EXPECT_THROW(static_cast<void>(Unit::Length.pow(8)), Base::OverflowError);
            EXPECT_THROW(static_cast<void>(Unit::Length.pow(-9)), Base::UnderflowError);
            EXPECT_THROW(static_cast<void>(Unit(9, 0, 0)), Base::OverflowError);

            // 边界内的幂次正常返回
            EXPECT_EQ(Unit::Length.pow(-8).exponents().at(0), -8);
            EXPECT_EQ(Unit::Length.pow(7).exponents().at(0), 7);
        }

        /**
         * @brief 钉住：带类型名构造时反查不再生效，类型名按构造时给出的值返回
         */
        TEST(UnitTest, ExplicitTypeNameOverridesLookup)
        {
            const Unit custom{UnitExponents{1, 0, 0, 0, 0, 0, 0, 0}, "CustomLength"};
            EXPECT_EQ(custom.getTypeString(), "CustomLength");
        }

    } // namespace
} // namespace ExpressionEngine::Units
