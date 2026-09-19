/**
 * @file DualQuaternion.h
 * @brief 对偶四元数：旋转与平移的统一表示，用于位姿插值
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cmath>

#include <ExpressionEngine/Base/DualNumber.h>

namespace ExpressionEngine::Base
{
    /**
     * @brief 分量取对偶数的四元数，用于位姿插值（见 pow()）
     * @details 旋转存放在实部，平移编码进对偶部：dual() = 0.5·t·r，其中 t 为平移量对应的纯四元数、
     *          r 为旋转四元数。四个分量 x、y、z、w 是构成值语义的公开数据，按数学惯例不加前缀。
     */
    class DualQuaternion
    {
    public:
        DualNumber x; ///< x 分量
        DualNumber y; ///< y 分量
        DualNumber z; ///< z 分量
        DualNumber w; ///< w（标量）分量

        /**
         * @brief 构造零四元数（所有分量为零）
         */
        DualQuaternion() = default;

        /**
         * @brief 按对偶分量构造
         * @param x x 分量
         * @param y y 分量
         * @param z z 分量
         * @param w w 分量
         */
        DualQuaternion(DualNumber x, DualNumber y, DualNumber z, DualNumber w) : x(x), y(y), z(z), w(w)
        {
        }

        /**
         * @brief 逐分量构造，各分量的实部与对偶部依次给出
         * @param x x 分量实部
         * @param y y 分量实部
         * @param z z 分量实部
         * @param w w 分量实部
         * @param dualX x 分量对偶部
         * @param dualY y 分量对偶部
         * @param dualZ z 分量对偶部
         * @param dualW w 分量对偶部
         */
        DualQuaternion(double x, double y, double z, double w, double dualX, double dualY, double dualZ, double dualW) : x(x, dualX), y(y, dualY), z(z, dualZ), w(w, dualW)
        {
        }

        /**
         * @brief 按纯实四元数构造（对偶部全为零）
         * @param x x 分量实部
         * @param y y 分量实部
         * @param z z 分量实部
         * @param w w 分量实部
         */
        DualQuaternion(const double x, double y, double z, double w) : x(x), y(y), z(z), w(w)
        {
        }

        /**
         * @brief 由实部与对偶部两个纯实四元数构造
         * @details 只读取两个参数各自的实部-实部；若参数带有非零对偶分量，说明调用方传错了对象
         *          （例如把整个对偶四元数当实部），此时抛 ValueError 而不是静默丢数据。
         * @param realPart 实部，即旋转四元数
         * @param dualPart 对偶部，即平移编码
         * @throws ValueError 任一参数含非零对偶分量
         */
        DualQuaternion(const DualQuaternion &realPart, const DualQuaternion &dualPart);

        /**
         * @brief 取恒等位姿对应的对偶四元数
         * @return 四元数 (0, 0, 0, 1)
         */
        static DualQuaternion identity()
        {
            return {0.0, 0.0, 0.0, 1.0};
        }

        /**
         * @brief 取实部（对偶部清零）
         * @return 纯实四元数
         */
        [[nodiscard]] DualQuaternion real() const
        {
            return {x.real, y.real, z.real, w.real};
        }

        /**
         * @brief 取对偶部（作为纯实四元数返回）
         * @return 由各分量对偶部组成的纯实四元数
         */
        [[nodiscard]] DualQuaternion dual() const
        {
            return {x.dual, y.dual, z.dual, w.dual};
        }

        /**
         * @brief 取共轭
         * @return 向量部分取反的结果
         */
        [[nodiscard]] DualQuaternion conjugate() const
        {
            return {-x, -y, -z, w};
        }

        /**
         * @brief 取向量部分（标量分量 w 清零）
         * @return w 为零的四元数
         */
        [[nodiscard]] DualQuaternion vector() const
        {
            return {x, y, z, 0.0};
        }

        /**
         * @brief 取实部四元数的模长
         * @return 旋转部分的模，单位旋转时为 1
         */
        [[nodiscard]] double length() const
        {
            return std::sqrt(x.real * x.real + y.real * y.real + z.real * z.real + w.real * w.real);
        }

        /**
         * @brief 取所表示旋转的转角
         * @return 转角，单位弧度，取值范围 [0, 2π)
         */
        [[nodiscard]] double rotationAngle() const
        {
            return 2.0 * std::atan2(vector().length(), w.real);
        }

        /**
         * @brief 取两个对偶四元数实部的点积
         * @details 用于判断两者的旋转方向是否相反，从而决定插值前是否取反一个操作数。
         * @param left 左操作数
         * @param right 右操作数
         * @return 实部四元数的点积
         */
        static double dot(const DualQuaternion &left, const DualQuaternion &right);

        /**
         * @brief 螺旋插值（ScLERP）
         * @details t=0 返回恒等位姿、t=1 返回自身，t 可超出 [0, 1] 做外插；本身无旋转时退化为
         *          平移的线性插值。shorten 为真且旋转角超过 180° 时取短弧。
         * @param t 插值参数
         * @param shorten 是否取短弧，默认取
         * @return 插值结果
         */
        [[nodiscard]] DualQuaternion pow(double t, bool shorten = true) const;

        /**
         * @brief 取相反数
         * @return 各分量的实部与对偶部都取反的结果
         */
        DualQuaternion operator-() const
        {
            return {-x, -y, -z, -w};
        }
    };

    /**
     * @brief 对偶四元数相加
     * @param left 左操作数
     * @param right 右操作数
     * @return 逐分量之和
     */
    DualQuaternion operator+(const DualQuaternion &left, const DualQuaternion &right);

    /**
     * @brief 对偶四元数相减
     * @param left 左操作数
     * @param right 右操作数
     * @return 逐分量之差
     */
    DualQuaternion operator-(const DualQuaternion &left, const DualQuaternion &right);

    /**
     * @brief 对偶四元数相乘
     * @param left 左操作数
     * @param right 右操作数
     * @return 乘积，对应两次位姿变换的复合
     */
    DualQuaternion operator*(const DualQuaternion &left, const DualQuaternion &right);

    /**
     * @brief 对偶四元数乘实数
     * @param left 被缩放的对偶四元数
     * @param right 缩放因子
     * @return 逐分量缩放的结果
     */
    DualQuaternion operator*(const DualQuaternion &left, double right);

    /**
     * @brief 实数乘对偶四元数
     * @param left 缩放因子
     * @param right 被缩放的对偶四元数
     * @return 逐分量缩放的结果
     */
    DualQuaternion operator*(double left, const DualQuaternion &right);

    /**
     * @brief 对偶四元数乘对偶数
     * @param left 被缩放的对偶四元数
     * @param right 对偶数缩放因子
     * @return 逐分量缩放的结果
     */
    DualQuaternion operator*(const DualQuaternion &left, DualNumber right);

    /**
     * @brief 对偶数乘对偶四元数
     * @param left 对偶数缩放因子
     * @param right 被缩放的对偶四元数
     * @return 逐分量缩放的结果
     */
    DualQuaternion operator*(DualNumber left, const DualQuaternion &right);
} // namespace ExpressionEngine::Base
