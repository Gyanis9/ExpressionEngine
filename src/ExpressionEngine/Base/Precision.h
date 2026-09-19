/**
 * @file Precision.h
 * @brief 几何算法通用的推荐精度常量（口径与 OCC 的 Precision 类一致）
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cmath>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Base
{
    /**
     * @brief 几何算法推荐精度
     * @details 角度比较用 1e-12，点重合判定用 1e-7，视为无穷大的临界量是 2e100；
     *          参数空间精度按「实空间精度 / 参数区间长度」换算，默认曲线取长度 100。
     */
    class Precision
    {
    public:
        /**
         * @brief 角度相等判定的推荐精度
         * @return 1e-12（弧度）
         */
        static double angular()
        {
            return s_angular;
        }

        /**
         * @brief 两点重合判定的推荐精度
         * @return 1e-7
         */
        static double confusion()
        {
            return s_confusion;
        }

        /**
         * @brief 重合精度的平方
         * @return confusion() 的平方，比较距离平方时用它可省去开方
         */
        static double squareConfusion()
        {
            return confusion() * confusion();
        }

        /**
         * @brief 求交算法的推荐精度
         * @return confusion() * 0.01，比重合判定更严格
         */
        static double intersection()
        {
            return confusion() * s_intersectionScale;
        }

        /**
         * @brief 逼近算法的推荐精度
         * @return confusion() * 10，比重合判定更宽松
         */
        static double approximation()
        {
            return confusion() * s_approximationScale;
        }

        /**
         * @brief 把实空间精度换算到参数空间
         * @param realPrecision 实空间精度
         * @param parameterRange 参数区间长度，必须非零
         * @return realPrecision / parameterRange
         * @throws ValueError 参数区间长度为零时抛出，避免返回 inf 掩盖退化曲线
         */
        static double parametric(const double realPrecision, const double parameterRange)
        {
            if (parameterRange == 0.0)
            {
                // 除以零会静默产出 inf，让后续比较全部失真，因此在入口拒绝
                throw ValueError("参数区间长度为零，无法把实空间精度换算到参数空间；"
                                 "请传入非零的参数区间长度（例如曲线的参数范围）");
            }
            return realPrecision / parameterRange;
        }

        /**
         * @brief 取指定参数区间长度下的参数空间重合精度
         * @param parameterRange 参数区间长度，必须非零
         * @return 按 parameterRange 换算的 confusion()
         */
        static double parametricConfusion(const double parameterRange)
        {
            return parametric(confusion(), parameterRange);
        }

        /**
         * @brief 取默认曲线上的参数空间重合精度
         * @return 以默认参数区间长度换算的 confusion()
         */
        static double parametricConfusion()
        {
            return parametric(confusion());
        }

        /**
         * @brief 参数空间重合精度的平方
         * @return parametricConfusion() 的平方
         */
        static double squareParametricConfusion()
        {
            return parametricConfusion() * parametricConfusion();
        }

        /**
         * @brief 取指定参数区间长度下的参数空间求交精度
         * @param parameterRange 参数区间长度，必须非零
         * @return 按 parameterRange 换算的 intersection()
         */
        static double parametricIntersection(const double parameterRange)
        {
            return parametric(intersection(), parameterRange);
        }

        /**
         * @brief 取指定参数区间长度下的参数空间逼近精度
         * @param parameterRange 参数区间长度，必须非零
         * @return 按 parameterRange 换算的 approximation()
         */
        static double parametricApproximation(const double parameterRange)
        {
            return parametric(approximation(), parameterRange);
        }

        /**
         * @brief 按默认曲线把实空间精度换算到参数空间
         * @param realPrecision 实空间精度
         * @return 以默认参数区间长度换算的结果
         */
        static double parametric(const double realPrecision)
        {
            return parametric(realPrecision, s_defaultParameterRange);
        }

        /**
         * @brief 取默认曲线上的参数空间求交精度
         * @return 以默认参数区间长度换算的 intersection()
         */
        static double parametricIntersection()
        {
            return parametric(intersection());
        }

        /**
         * @brief 取默认曲线上的参数空间逼近精度
         * @return 以默认参数区间长度换算的 approximation()
         */
        static double parametricApproximation()
        {
            return parametric(approximation());
        }

        /**
         * @brief 判断数值是否可视为无穷大
         * @param value 待判定数值
         * @return 绝对值不小于 infinite() 的一半时返回 true
         */
        static bool isInfinite(const double value)
        {
            return std::abs(value) >= (0.5 * infinite());
        }

        /**
         * @brief 判断数值是否可视为正无穷大
         * @param value 待判定数值
         * @return 不小于 infinite() 的一半时返回 true
         */
        static bool isPositiveInfinite(const double value)
        {
            return value >= (0.5 * infinite());
        }

        /**
         * @brief 判断数值是否可视为负无穷大
         * @param value 待判定数值
         * @return 不大于 -infinite() 的一半时返回 true
         */
        static bool isNegativeInfinite(const double value)
        {
            return value <= -(0.5 * infinite());
        }

        /**
         * @brief 取可视为无穷大的临界量
         * @return 2e100，取负即负无穷大的临界量
         */
        static double infinite()
        {
            return s_infinite;
        }

    private:
        static constexpr double s_angular               = 1.0e-12;  ///< 角度相等推荐精度（弧度）
        static constexpr double s_confusion             = 1.0e-7;   ///< 点重合推荐精度
        static constexpr double s_intersectionScale     = 0.01;     ///< 求交精度相对重合精度的比例
        static constexpr double s_approximationScale    = 10.0;     ///< 逼近精度相对重合精度的倍数
        static constexpr double s_infinite              = 2.0e+100; ///< 视为无穷大的临界量
        static constexpr double s_defaultParameterRange = 100.0;    ///< 默认曲线的参数区间长度
    };
} // namespace ExpressionEngine::Base
