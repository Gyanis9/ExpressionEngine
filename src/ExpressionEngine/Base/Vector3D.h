/**
 * @file Vector3D.h
 * @brief 三维向量模板 Vector3 及配套的浮点特征与自由函数
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cmath>
#include <limits>
#include <numbers>

namespace ExpressionEngine::Base
{
    /**
     * @brief 浮点类型数值特征的通用模板
     * @details 为 Vector3 提供 pi/epsilon/maximum 三个编译期常量；未特化的类型无法实例化 Vector3。
     * @tparam FloatingType 浮点类型
     */
    template<class FloatingType>
    struct FloatTraits
    {
    };

    /**
     * @brief float 的数值特征
     */
    template<>
    struct FloatTraits<float>
    {
        /** @brief 分量类型 */
        using FloatingType = float;

        /**
         * @brief 取圆周率
         * @return 分量类型的 π 值
         */
        [[nodiscard]] static consteval FloatingType pi()
        {
            return std::numbers::pi_v<FloatingType>;
        }

        /**
         * @brief 取机器精度
         * @return 分量类型的 epsilon，即相邻可表示浮点数之间的最小相对差异
         */
        [[nodiscard]] static consteval FloatingType epsilon()
        {
            return std::numeric_limits<FloatingType>::epsilon();
        }

        /**
         * @brief 取可表示的最大有限值
         * @return 分量类型的最大有限值
         */
        [[nodiscard]] static consteval FloatingType maximum()
        {
            return std::numeric_limits<FloatingType>::max();
        }
    };

    /**
     * @brief double 的数值特征
     */
    template<>
    struct FloatTraits<double>
    {
        /** @brief 分量类型 */
        using FloatingType = double;

        /**
         * @brief 取圆周率
         * @return 分量类型的 π 值
         */
        [[nodiscard]] static consteval FloatingType pi()
        {
            return std::numbers::pi_v<FloatingType>;
        }

        /**
         * @brief 取机器精度
         * @return 分量类型的 epsilon，即相邻可表示浮点数之间的最小相对差异
         */
        [[nodiscard]] static consteval FloatingType epsilon()
        {
            return std::numeric_limits<FloatingType>::epsilon();
        }

        /**
         * @brief 取可表示的最大有限值
         * @return 分量类型的最大有限值
         */
        [[nodiscard]] static consteval FloatingType maximum()
        {
            return std::numeric_limits<FloatingType>::max();
        }
    };

    /**
     * @brief 三维向量模板
     * @details 值语义的数学类型：x/y/z 按数学惯例公开，便于结构化访问与就地运算。
     *          operator==/operator!= 按机器精度做分量容差比较，并非逐位相等。
     * @tparam FloatingType 分量类型，支持 float 与 double
     */
    template<class FloatingType>
    class Vector3
    {
    public:
        static const Vector3 UnitX; ///< X 轴单位向量
        static const Vector3 UnitY; ///< Y 轴单位向量
        static const Vector3 UnitZ; ///< Z 轴单位向量

        using NumberType = FloatingType;            ///< 分量类型
        using TraitsType = FloatTraits<NumberType>; ///< 数值特征类型

        /**
         * @brief 取分量类型的机器精度
         * @return 分量类型 epsilon 的最小可分辨差异
         */
        [[nodiscard]] static constexpr NumberType epsilon()
        {
            return TraitsType::epsilon();
        }

        FloatingType x; ///< x 分量
        FloatingType y; ///< y 分量
        FloatingType z; ///< z 分量

        /**
         * @brief 构造向量
         * @param xValue x 分量，默认 0
         * @param yValue y 分量，默认 0
         * @param zValue z 分量，默认 0
         */
        explicit Vector3(FloatingType xValue = 0.0, FloatingType yValue = 0.0, FloatingType zValue = 0.0);

        /**
         * @brief 拷贝构造
         * @param other 被拷贝的向量
         */
        Vector3(const Vector3 &other) = default;

        /**
         * @brief 移动构造
         * @param other 被移动的向量，移动后处于有效但未指定状态
         */
        Vector3(Vector3 &&other) noexcept = default;

        /**
         * @brief 析构函数
         */
        ~Vector3() = default;

        /**
         * @brief 取可写分量引用
         * @param index 分量下标，取值 0/1/2
         * @return 对应分量的引用
         * @throws IndexError 下标超出 [0,2] 时抛出，避免越界写入静默破坏相邻内存
         */
        [[nodiscard]] FloatingType &operator[](unsigned short index);

        /**
         * @brief 取只读分量引用
         * @param index 分量下标，取值 0/1/2
         * @return 对应分量的常量引用
         * @throws IndexError 下标超出 [0,2] 时抛出
         */
        [[nodiscard]] const FloatingType &operator[](unsigned short index) const;

        /**
         * @brief 向量加法
         * @param other 加数
         * @return 逐分量相加的新向量
         */
        [[nodiscard]] Vector3 operator+(const Vector3 &other) const;

        /**
         * @brief 逐分量乘以另一向量各分量的绝对值
         * @param other 提供绝对值因子的向量
         * @return 结果向量
         */
        [[nodiscard]] Vector3 operator&(const Vector3 &other) const;

        /**
         * @brief 向量减法
         * @param other 减数
         * @return 逐分量相减的新向量
         */
        [[nodiscard]] Vector3 operator-(const Vector3 &other) const;

        /**
         * @brief 取反向量
         * @return 三分量都取反的新向量
         */
        [[nodiscard]] Vector3 operator-() const;

        /**
         * @brief 就地累加
         * @param other 加数
         * @return 自身引用
         */
        Vector3 &operator+=(const Vector3 &other);

        /**
         * @brief 就地累减
         * @param other 减数
         * @return 自身引用
         */
        Vector3 &operator-=(const Vector3 &other);

        /**
         * @brief 向量缩放
         * @param scale 缩放因子
         * @return 缩放后的新向量
         */
        [[nodiscard]] Vector3 operator*(FloatingType scale) const;

        /**
         * @brief 向量除以标量
         * @param divisor 除数
         * @return 相除后的新向量
         */
        [[nodiscard]] Vector3 operator/(FloatingType divisor) const;

        /**
         * @brief 就地缩放
         * @param scale 缩放因子
         * @return 自身引用
         */
        Vector3 &operator*=(FloatingType scale);

        /**
         * @brief 就地除以标量
         * @param divisor 除数
         * @return 自身引用
         */
        Vector3 &operator/=(FloatingType divisor);

        /**
         * @brief 拷贝赋值
         * @param other 被赋值的向量
         * @return 自身引用
         */
        Vector3 &operator=(const Vector3 &other) = default;

        /**
         * @brief 移动赋值
         * @param other 被移动的向量，移动后处于有效但未指定状态
         * @return 自身引用
         */
        Vector3 &operator=(Vector3 &&other) noexcept = default;

        /**
         * @brief 点积（标量积）
         * @param other 另一向量
         * @return 点积结果
         */
        [[nodiscard]] FloatingType operator*(const Vector3 &other) const;

        /**
         * @brief 点积（标量积）
         * @param other 另一向量
         * @return 点积结果
         */
        [[nodiscard]] FloatingType dot(const Vector3 &other) const;

        /**
         * @brief 叉积（向量积）
         * @param other 另一向量
         * @return 同时垂直于两向量的新向量
         */
        [[nodiscard]] Vector3 operator%(const Vector3 &other) const;

        /**
         * @brief 叉积（向量积）
         * @param other 另一向量
         * @return 同时垂直于两向量的新向量
         */
        [[nodiscard]] Vector3 cross(const Vector3 &other) const;

        /**
         * @brief 按容差比较不等
         * @param other 另一向量
         * @return 任一分量差异超过机器精度时返回 true
         */
        [[nodiscard]] bool operator!=(const Vector3 &other) const;

        /**
         * @brief 按容差比较相等
         * @param other 另一向量
         * @return 三分量差异都不超过机器精度时返回 true
         */
        [[nodiscard]] bool operator==(const Vector3 &other) const;

        /**
         * @brief 判断本点是否落在线段上
         * @param startPoint 线段起点
         * @param endPoint 线段终点
         * @return 与线段共线且投影落在线段范围内时返回 true
         */
        [[nodiscard]] bool isOnLineSegment(const Vector3 &startPoint, const Vector3 &endPoint) const;

        /**
         * @brief 就地缩放 X 分量
         * @param factor 缩放因子
         */
        void scaleX(FloatingType factor);

        /**
         * @brief 就地缩放 Y 分量
         * @param factor 缩放因子
         */
        void scaleY(FloatingType factor);

        /**
         * @brief 就地缩放 Z 分量
         * @param factor 缩放因子
         */
        void scaleZ(FloatingType factor);

        /**
         * @brief 三分量分别就地缩放
         * @param xFactor X 分量缩放因子
         * @param yFactor Y 分量缩放因子
         * @param zFactor Z 分量缩放因子
         */
        void scale(FloatingType xFactor, FloatingType yFactor, FloatingType zFactor);

        /**
         * @brief 就地平移 X 分量
         * @param offset 偏移量
         */
        void moveX(FloatingType offset);

        /**
         * @brief 就地平移 Y 分量
         * @param offset 偏移量
         */
        void moveY(FloatingType offset);

        /**
         * @brief 就地平移 Z 分量
         * @param offset 偏移量
         */
        void moveZ(FloatingType offset);

        /**
         * @brief 三分量分别就地平移
         * @param xOffset X 分量偏移
         * @param yOffset Y 分量偏移
         * @param zOffset Z 分量偏移
         */
        void move(FloatingType xOffset, FloatingType yOffset, FloatingType zOffset);

        /**
         * @brief 绕 X 轴就地旋转
         * @param angle 旋转角（弧度）
         */
        void rotateX(FloatingType angle);

        /**
         * @brief 绕 Y 轴就地旋转
         * @param angle 旋转角（弧度）
         */
        void rotateY(FloatingType angle);

        /**
         * @brief 绕 Z 轴就地旋转
         * @param angle 旋转角（弧度）
         */
        void rotateZ(FloatingType angle);

        /**
         * @brief 一次性重设三分量
         * @param xValue x 分量
         * @param yValue y 分量
         * @param zValue z 分量
         */
        void set(FloatingType xValue, FloatingType yValue, FloatingType zValue);

        /**
         * @brief 取向量长度
         * @return 欧几里得范数
         */
        [[nodiscard]] FloatingType length() const;

        /**
         * @brief 取长度平方
         * @return 三分量平方和，比 length() 少一次开方
         */
        [[nodiscard]] FloatingType squaredLength() const;

        /**
         * @brief 就地归一化
         * @return 自身引用
         * @throws ValueError 向量长度为零时抛出，因为零向量没有方向，静默返回会掩盖调用错误
         */
        Vector3 &normalize();

        /**
         * @brief 取归一化后的副本
         * @return 单位向量副本
         * @throws ValueError 向量长度为零时抛出
         */
        Vector3 normalized() const;

        /**
         * @brief 判断是否为零向量
         * @return 三分量都精确等于 0 时返回 true
         */
        [[nodiscard]] bool isNull() const;

        /**
         * @brief 取两向量夹角
         * @param other 另一向量
         * @return 夹角弧度，落在 [0, pi]；任一方为零向量时返回 NaN（无法定义夹角）
         */
        [[nodiscard]] FloatingType getAngle(const Vector3 &other) const;

        /**
         * @brief 取带符号夹角
         * @param other 另一向量
         * @param normal 用于确定旋向的参考法向
         * @return 夹角弧度，落在 [0, 2*pi]
         */
        [[nodiscard]] FloatingType getAngleOriented(const Vector3 &other, const Vector3 &normal) const;

        /**
         * @brief 把本点变换到给定坐标系
         * @param base 目标坐标系原点
         * @param xDirection 目标坐标系 X 方向
         * @param yDirection 目标坐标系 Y 方向，必须与 xDirection 垂直
         * @throws ValueError xDirection 与 yDirection 平行导致叉积为零向量时抛出
         */
        void transformToCoordinateSystem(const Vector3 &base, const Vector3 &xDirection, const Vector3 &yDirection);

        /**
         * @brief 按距离容差判断两点是否重合
         * @param point 另一坐标点
         * @param tolerance 容差
         * @return 两点距离不超过容差时返回 true
         */
        [[nodiscard]] bool isEqual(const Vector3 &point, FloatingType tolerance) const;

        /**
         * @brief 判断两向量是否平行
         * @param direction 方向向量
         * @param tolerance 角度容差（弧度）
         * @return 夹角接近 0 或 pi 时返回 true；任一方为零向量时返回 false
         */
        [[nodiscard]] bool isParallel(const Vector3 &direction, FloatingType tolerance) const;

        /**
         * @brief 判断两向量是否垂直
         * @param direction 方向向量
         * @param tolerance 角度容差（弧度）
         * @return 夹角接近 pi/2 时返回 true；任一方为零向量时返回 false
         */
        [[nodiscard]] bool isNormal(const Vector3 &direction, FloatingType tolerance) const;

        /**
         * @brief 就地把点投影到平面
         * @param base 平面上一点
         * @param normal 平面法向
         * @return 自身引用
         */
        Vector3 &projectToPlane(const Vector3 &base, const Vector3 &normal);

        /**
         * @brief 把点投影到平面并存入输出参数
         * @param base 平面上一点
         * @param normal 平面法向
         * @param projection 输出：投影结果
         */
        void projectToPlane(const Vector3 &base, const Vector3 &normal, Vector3 &projection) const;

        /**
         * @brief 就地把点投影到直线
         * @details 结果是「本点到直线上垂足」的向量；本方法并不依赖当前向量的内容。
         * @param point 直线所过的点
         * @param line 直线方向
         * @return 自身引用
         */
        Vector3 &projectToLine(const Vector3 &point, const Vector3 &line);

        /**
         * @brief 求本点到直线的垂足
         * @param base 直线所过的点
         * @param direction 直线方向
         * @return 垂足坐标
         */
        [[nodiscard]] Vector3 perpendicular(const Vector3 &base, const Vector3 &direction) const;

        /**
         * @brief 求点到平面的带符号距离
         * @param base 平面上一点
         * @param normal 平面法向
         * @return 与法向同侧为正、异侧为负的距离
         */
        [[nodiscard]] FloatingType distanceToPlane(const Vector3 &base, const Vector3 &normal) const;

        /**
         * @brief 求点到直线的距离
         * @param base 直线所过的点
         * @param direction 直线方向
         * @return 垂直距离
         */
        [[nodiscard]] FloatingType distanceToLine(const Vector3 &base, const Vector3 &direction) const;

        /**
         * @brief 求点到线段的最短位移向量
         * @details 垂足落在线段外时取到端点的位移。
         * @param firstPoint 线段起点
         * @param secondPoint 线段终点
         * @return 从本点指向线段最近点的向量
         */
        [[nodiscard]] Vector3 distanceToLineSegment(const Vector3 &firstPoint, const Vector3 &secondPoint) const;
    };

    template<class FloatingType>
    Vector3<FloatingType> const Vector3<FloatingType>::UnitX(1.0, 0.0, 0.0);
    template<class FloatingType>
    Vector3<FloatingType> const Vector3<FloatingType>::UnitY(0.0, 1.0, 0.0);
    template<class FloatingType>
    Vector3<FloatingType> const Vector3<FloatingType>::UnitZ(0.0, 0.0, 1.0);

    /**
     * @brief 求两点距离
     * @param first 第一个点
     * @param second 第二个点
     * @return 欧几里得距离
     */
    template<class FloatingType>
    [[nodiscard]] inline FloatingType distance(const Vector3<FloatingType> &first, const Vector3<FloatingType> &second)
    {
        const FloatingType deltaX = first.x - second.x;
        const FloatingType deltaY = first.y - second.y;
        const FloatingType deltaZ = first.z - second.z;
        return static_cast<FloatingType>(std::sqrt((deltaX * deltaX) + (deltaY * deltaY) + (deltaZ * deltaZ)));
    }

    /**
     * @brief 求两点距离平方
     * @param first 第一个点
     * @param second 第二个点
     * @return 距离的平方，比较远近时用它可省去开方
     */
    template<class FloatingType>
    [[nodiscard]] FloatingType squaredDistance(const Vector3<FloatingType> &first, const Vector3<FloatingType> &second)
    {
        const FloatingType deltaX = first.x - second.x;
        const FloatingType deltaY = first.y - second.y;
        const FloatingType deltaZ = first.z - second.z;
        return deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
    }

    /**
     * @brief 标量左乘向量
     * @param factor 缩放因子
     * @param vector 被缩放的向量
     * @return 缩放后的新向量
     */
    template<class FloatingType>
    [[nodiscard]] Vector3<FloatingType> operator*(FloatingType factor, const Vector3<FloatingType> &vector)
    {
        return Vector3<FloatingType>(vector.x * factor, vector.y * factor, vector.z * factor);
    }

    /**
     * @brief 按分量精度转换向量类型
     * @tparam TargetType 目标分量类型
     * @tparam SourceType 源分量类型
     * @param vector 源向量
     * @return 转换后的新向量
     */
    template<class TargetType, class SourceType>
    [[nodiscard]] Vector3<TargetType> toVector(const Vector3<SourceType> &vector)
    {
        return Vector3<TargetType>(static_cast<TargetType>(vector.x), static_cast<TargetType>(vector.y), static_cast<TargetType>(vector.z));
    }

    /**
     * @brief 单精度三维向量
     */
    using Vector3f = Vector3<float>;

    /**
     * @brief 双精度三维向量
     */
    using Vector3d = Vector3<double>;
} // namespace ExpressionEngine::Base
