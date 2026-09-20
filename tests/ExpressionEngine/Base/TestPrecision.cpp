// 本文件钉住几何精度常量的取值与派生关系：角度、重合、求交、逼近、参数空间换算与无穷大判定。

#include <gtest/gtest.h>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Precision.h>

namespace ExpressionEngine::Base
{
    namespace
    {

        /**
         * @brief 钉住：基准精度与头文件承诺的数值一致（口径同 OCC 的 Precision）
         */
        TEST(PrecisionTest, BaseThresholdsMatchTheDocumentedValues)
        {
            EXPECT_DOUBLE_EQ(Precision::angular(), 1.0e-12);
            EXPECT_DOUBLE_EQ(Precision::confusion(), 1.0e-7);
            EXPECT_DOUBLE_EQ(Precision::infinite(), 2.0e+100);
        }

        /**
         * @brief 钉住：派生精度都由重合精度换算，改基准值时不会彼此失联
         */
        TEST(PrecisionTest, DerivedThresholdsFollowConfusion)
        {
            EXPECT_DOUBLE_EQ(Precision::squareConfusion(), Precision::confusion() * Precision::confusion());
            // 求交比重合更严格，逼近更宽松
            EXPECT_DOUBLE_EQ(Precision::intersection(), Precision::confusion() * 0.01);
            EXPECT_DOUBLE_EQ(Precision::approximation(), Precision::confusion() * 10.0);
            EXPECT_LT(Precision::intersection(), Precision::confusion());
            EXPECT_GT(Precision::approximation(), Precision::confusion());
        }

        /**
         * @brief 钉住：实空间精度按参数区间长度换算，默认区间取 100
         */
        TEST(PrecisionTest, ParametricConversionUsesTheDefaultRange)
        {
            EXPECT_DOUBLE_EQ(Precision::parametric(1.0e-7, 100.0), 1.0e-9);
            EXPECT_DOUBLE_EQ(Precision::parametricConfusion(), Precision::parametricConfusion(100.0));
            EXPECT_DOUBLE_EQ(Precision::squareParametricConfusion(), Precision::parametricConfusion() * Precision::parametricConfusion());
            EXPECT_DOUBLE_EQ(Precision::parametricIntersection(), Precision::parametricIntersection(100.0));
            EXPECT_DOUBLE_EQ(Precision::parametricApproximation(), Precision::parametricApproximation(100.0));

            // 区间长度为零会让换算变成 inf，把后续比较全部失真，因此在入口就拒绝
            EXPECT_THROW(static_cast<void>(Precision::parametric(1.0e-7, 0.0)), ValueError);
            EXPECT_THROW(static_cast<void>(Precision::parametricConfusion(0.0)), ValueError);
        }

        /**
         * @brief 钉住：无穷大判定取临界量的一半，正负分开判
         */
        TEST(PrecisionTest, InfiniteFlagsUseHalfTheThreshold)
        {
            const double half = Precision::infinite() * 0.5;
            EXPECT_TRUE(Precision::isInfinite(half));
            EXPECT_TRUE(Precision::isInfinite(-half));
            EXPECT_TRUE(Precision::isInfinite(2.0 * Precision::infinite()));
            EXPECT_FALSE(Precision::isInfinite(half * 0.99));

            EXPECT_TRUE(Precision::isPositiveInfinite(half));
            EXPECT_FALSE(Precision::isPositiveInfinite(-half));
            EXPECT_TRUE(Precision::isNegativeInfinite(-half));
            EXPECT_FALSE(Precision::isNegativeInfinite(half));

            // 普通数值与零都不算无穷大
            EXPECT_FALSE(Precision::isInfinite(0.0));
            EXPECT_FALSE(Precision::isInfinite(1.0e+10));
        }

    } // namespace
}     // namespace ExpressionEngine::Base
