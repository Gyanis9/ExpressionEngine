/**
 * @file DualNumber.h
 * @brief 对偶数 a + b·ε（ε² = 0），对偶四元数的分量类型
 * @author Gyanis
 * @date 2026-09-19
 * @version 0.0.1
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <cmath>

namespace ExpressionEngine::Base
{
    /**
     * @brief 对偶数，形如 a + b·ε，其中 ε² = 0
     * @details 对偶数把函数值与其导数打包进一个数：把 f 作用在 a + b·ε 上，结果的 ε 分量即
     *          f'(a)·b。DualQuaternion 用它同时承载旋转与平移两部分。
     */
    class DualNumber
    {
    public:
        double real = 0.0; ///< 实部（函数值）
        double dual = 0.0; ///< 对偶部（ε 分量，承载导数值）

        /**
         * @brief 默认构造为零对偶数
         */
        DualNumber() = default;

        /**
         * @brief 构造对偶数
         * @param reValue 实部
         * @param dualValue 对偶部，默认 0 表示纯实数
         */
        DualNumber(const double reValue, const double dualValue = 0.0) :
            real(reValue), dual(dualValue)
        {
        }

        /**
         * @brief 取相反数
         * @return 两个分量都取反的对偶数
         */
        DualNumber operator-() const
        {
            return {-real, -dual};
        }
    };

    /**
     * @brief 对偶数相加
     * @param left 左操作数
     * @param right 右操作数
     * @return 逐分量相加的结果
     */
    inline DualNumber operator+(const DualNumber left, const DualNumber right)
    {
        return {left.real + right.real, left.dual + right.dual};
    }

    /**
     * @brief 对偶数加实数
     * @param left 左操作数
     * @param right 加到实部上的实数
     * @return 实部相加、对偶部保持的结果
     */
    inline DualNumber operator+(DualNumber left, const double right)
    {
        return {left.real + right, left.dual};
    }

    /**
     * @brief 实数加对偶数
     * @param left 加到实部上的实数
     * @param right 右操作数
     * @return 实部相加、对偶部保持的结果
     */
    inline DualNumber operator+(const double left, DualNumber right)
    {
        return {left + right.real, right.dual};
    }

    /**
     * @brief 对偶数相减
     * @param left 左操作数
     * @param right 右操作数
     * @return 逐分量相减的结果
     */
    inline DualNumber operator-(const DualNumber left, const DualNumber right)
    {
        return {left.real - right.real, left.dual - right.dual};
    }

    /**
     * @brief 对偶数减实数
     * @param left 左操作数
     * @param right 从实部减去的实数
     * @return 实部相减、对偶部保持的结果
     */
    inline DualNumber operator-(DualNumber left, double right)
    {
        return {left.real - right, left.dual};
    }

    /**
     * @brief 实数减对偶数
     * @param left 被减的实数
     * @param right 右操作数
     * @return 逐分量相减的结果
     */
    inline DualNumber operator-(const double left, const DualNumber right)
    {
        return {left - right.real, -right.dual};
    }

    /**
     * @brief 对偶数相乘
     * @param left 左操作数
     * @param right 右操作数
     * @return 乘积；ε² = 0 使结果只保留两项
     */
    inline DualNumber operator*(const DualNumber left, const DualNumber right)
    {
        return {left.real * right.real, left.real * right.dual + left.dual * right.real};
    }

    /**
     * @brief 实数乘对偶数
     * @param left 缩放因子
     * @param right 被缩放的对偶数
     * @return 逐分量缩放的结果
     */
    inline DualNumber operator*(const double left, const DualNumber right)
    {
        return {left * right.real, left * right.dual};
    }

    /**
     * @brief 对偶数乘实数
     * @param left 被缩放的对偶数
     * @param right 缩放因子
     * @return 逐分量缩放的结果
     */
    inline DualNumber operator*(const DualNumber left, const double right)
    {
        return {left.real * right, left.dual * right};
    }

    /**
     * @brief 对偶数相除
     * @param left 被除数
     * @param right 除数，实部为零时结果为 NaN/inf，调用方须自行保证非零
     * @return 商，按商的求导法则同时更新对偶部
     */
    inline DualNumber operator/(const DualNumber left, const DualNumber right)
    {
        return {left.real / right.real, (left.dual * right.real - left.real * right.dual) / (right.real * right.real)};
    }

    /**
     * @brief 对偶数除以实数
     * @param left 被除数
     * @param right 除数
     * @return 逐分量相除的结果
     */
    inline DualNumber operator/(const DualNumber left, const double right)
    {
        return {left.real / right, left.dual / right};
    }

    /**
     * @brief 对偶数求幂
     * @param base 底数
     * @param power 指数
     * @return base^power；对偶部由幂函数导数得出
     */
    inline DualNumber pow(const DualNumber base, const double power)
    {
        return {std::pow(base.real, power), power * std::pow(base.real, power - 1.0) * base.dual};
    }
} // namespace ExpressionEngine::Base
