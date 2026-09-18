/**
 * @file Placement.h
 * @brief 刚体位姿（位置 + 旋转），可与矩阵、对偶四元数互转
 * @author Gyanis
 * @date 2026-09-18
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <string>

#include <ExpressionEngine/Base/Rotation.h>

namespace ExpressionEngine::Base
{
    class DualQuat;
    class Matrix4D;

    /**
     * @brief 刚体位姿：平移加旋转
     * @details 语义与 FreeCAD Base::Placement 完全一致。multVec() 先施加旋转再加上位置；
     *          复合按「右操作数先作用」的约定，零位置加单位旋转即恒等位姿。
     */
    class Placement
    {
    public:
        /**
         * @brief 构造恒等位姿（零位置 + 单位旋转）
         */
        Placement();

        Placement(const Placement &) = default;

        Placement(Placement &&) = default;

        /**
         * @brief 从 4x4 矩阵构造：旋转取矩阵的旋转部分，位置取第四列
         * @param matrix 源矩阵
         */
        Placement(const Matrix4D &matrix);

        /**
         * @brief 以位置与旋转构造
         * @param position 平移量
         * @param rotation 旋转
         */
        Placement(const Vector3d &position, const Rotation &rotation);

        /**
         * @brief 以旋转中心构造：绕局部点 center 旋转后再平移
         * @param position 平移量
         * @param rotation 旋转
         * @param center 旋转中心，位于未平移的局部坐标系中
         */
        Placement(const Vector3d &position, const Rotation &rotation, const Vector3d &center);

        /**
         * @brief 由对偶四元数构造
         * @param qq 对偶四元数，实部为旋转、对偶部按 0.5·t·r 编码平移
         * @return 对应位姿
         */
        static Placement fromDualQuaternion(DualQuat qq);

        ~Placement() = default;

        /**
         * @brief 输出等价的 4x4 齐次变换矩阵
         * @return 旋转部分取四元数展开，第四列为位置
         */
        Matrix4D toMatrix() const;

        /**
         * @brief 从 4x4 矩阵设置平移与旋转
         * @param matrix 源矩阵，旋转取自其分解结果
         */
        void fromMatrix(const Matrix4D &matrix);

        /**
         * @brief 输出对偶四元数形式
         * @return 实部为旋转四元数，对偶部为 0.5·t·r
         */
        DualQuat toDualQuaternion() const;

        /**
         * @brief 取位置
         * @return 位置的常量引用，随对象生命周期有效
         */
        const Vector3d &getPosition() const
        {
            return m_position;
        }

        /**
         * @brief 取旋转
         * @return 旋转的常量引用，随对象生命周期有效
         */
        const Rotation &getRotation() const
        {
            return m_rotation;
        }

        /**
         * @brief 设置位置
         * @param position 新的平移量
         */
        void setPosition(const Vector3d &position)
        {
            m_position = position;
        }

        /**
         * @brief 设置旋转
         * @param rotation 新的旋转
         */
        void setRotation(const Rotation &rotation)
        {
            m_rotation = rotation;
        }

        /**
         * @brief 判断是否恰为恒等位姿（位置精确为零且旋转精确为单位旋转）
         * @return true 是恒等位姿
         */
        bool isIdentity() const;

        /**
         * @brief 判断在容差内是否为恒等位姿
         * @param tol 容差，位置按欧氏距离、旋转按四元数点积判定
         * @return true 在容差内是恒等位姿
         */
        bool isIdentity(double tol) const;

        /**
         * @brief 就地取逆
         */
        void invert();

        /**
         * @brief 取逆位姿
         * @return 新的位姿对象
         */
        Placement inverse() const;

        /**
         * @brief 就地累加平移量，旋转不变
         * @param movVector 在全局系中追加的平移量
         */
        void move(const Vector3d &movVector);

        /**
         * @brief 判断是否恰为同一位姿
         * @param other 待比较的位姿
         * @return true 位置精确相等且旋转精确相同
         */
        bool isSame(const Placement &other) const;

        /**
         * @brief 判断在容差内是否为同一位姿
         * @param other 待比较的位姿
         * @param tol 容差，位置按欧氏距离、旋转按四元数点积判定
         * @return true 位置与旋转都在容差内一致
         */
        bool isSame(const Placement &other, double tol) const;

        /**
         * @brief 就地右乘另一个位姿
         * @param other 右操作数，先施加它
         * @return 自身引用
         */
        Placement &operator*=(const Placement &other);

        /**
         * @brief 右乘另一个位姿
         * @param other 右操作数，先施加它
         * @return 复合后的新位姿
         */
        Placement operator*(const Placement &other) const;

        /**
         * @brief 精确比较
         * @param other 待比较的位姿
         * @return true 位置与旋转都精确相等
         */
        bool operator==(const Placement &other) const;

        /**
         * @brief 精确比较的取反
         * @param other 待比较的位姿
         * @return true 位置或旋转不相等
         */
        bool operator!=(const Placement &other) const;

        Placement &operator=(const Placement &) = default;

        Placement &operator=(Placement &&) = default;

        /**
         * @brief 沿螺旋运动插值该位姿
         * @details 以对偶四元数的 ScLERP 实现：t=0 得到恒等位姿，t=1 得到原位姿；无旋转时
         *          退化为平移的线性插值。t 允许取 [0, 1] 之外的值以做外插。
         * @param t 插值参数
         * @param shorten 旋转角超过 180° 时是否取短弧，默认取短弧
         * @return 插值结果
         */
        Placement pow(double t, bool shorten = true) const;

        /**
         * @brief 就地右乘另一个位姿
         * @param other 右操作数
         * @return 自身引用
         */
        Placement &multRight(const Placement &other);

        /**
         * @brief 就地左乘另一个位姿
         * @param other 左操作数
         * @return 自身引用
         */
        Placement &multLeft(const Placement &other);

        /**
         * @brief 用本位姿变换向量
         * @param src 输入向量
         * @param dst 输出向量，可与 src 为同一对象
         */
        void multVec(const Vector3d &src, Vector3d &dst) const;

        /**
         * @brief 用本位姿变换单精度向量
         * @param src 输入向量
         * @param dst 输出向量，可与 src 为同一对象
         */
        void multVec(const Vector3f &src, Vector3f &dst) const;

        /**
         * @brief 线性插值位姿：旋转走球面插值，位置走线性插值
         * @param p0 起点位姿，t=0 时返回它
         * @param p1 终点位姿，t=1 时返回它
         * @param t 插值参数
         * @return 插值结果
         */
        static Placement slerp(const Placement &p0, const Placement &p1, double t);

        /**
         * @brief 螺旋插值位姿（对偶四元数 ScLERP）
         * @param p0 起点位姿，t=0 时返回它
         * @param p1 终点位姿，t=1 时返回它
         * @param t 插值参数
         * @param shorten 旋转角超过 180° 时是否取短弧，默认取短弧
         * @return 插值结果
         */
        static Placement sclerp(const Placement &p0, const Placement &p1, double t, bool shorten = true);

        /**
         * @brief 输出便于调试阅读的文本表示
         * @return 形如 "position (...), axis (...), angle ..." 的单行文本（含结尾换行）
         */
        std::string toString() const;

    private:
        Vector3d m_position; ///< 平移量（全局坐标系下）
        Rotation m_rotation; ///< 旋转部分
    };
} // namespace ExpressionEngine::Base
