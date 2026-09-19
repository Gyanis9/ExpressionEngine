/**
 * @file Matrix.h
 * @brief 4x4 齐次变换矩阵 Matrix4D 与缩放类型枚举
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <array>
#include <cmath>
#include <string>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Vector3D.h>

namespace ExpressionEngine::Base
{
    /**
     * @brief 矩阵缩放类型的判定结果
     */
    enum class ScaleType
    {
        Other           = -1, ///< 投影、剪切等不属于单纯缩放的矩阵
        NoScaling       = 0,  ///< 无缩放
        NonUniformRight = 1,  ///< 从右侧作用的非均匀缩放
        NonUniformLeft  = 2,  ///< 从左侧作用的非均匀缩放
        Uniform         = 3   ///< 均匀缩放
    };

    /**
     * @brief 4x4 齐次变换矩阵
     * @details 元素按行优先存储，第 4 行固定为 (0,0,0,1) 表示仿射变换；默认构造为单位阵。
     *          所有变换方法都是把变换左乘到当前矩阵，与 FreeCAD 的语义保持一致。
     */
    class Matrix4D
    {
        /** @brief 双精度数值特征类型 */
        using TraitsType = FloatTraits<double>;

    public:
        /**
         * @brief 默认构造为单位阵
         */
        Matrix4D();

        /**
         * @brief 按 16 个单精度元素构造（行优先）
         * @param a11 第 1 行第 1 列
         * @param a12 第 1 行第 2 列
         * @param a13 第 1 行第 3 列
         * @param a14 第 1 行第 4 列
         * @param a21 第 2 行第 1 列
         * @param a22 第 2 行第 2 列
         * @param a23 第 2 行第 3 列
         * @param a24 第 2 行第 4 列
         * @param a31 第 3 行第 1 列
         * @param a32 第 3 行第 2 列
         * @param a33 第 3 行第 3 列
         * @param a34 第 3 行第 4 列
         * @param a41 第 4 行第 1 列
         * @param a42 第 4 行第 2 列
         * @param a43 第 4 行第 3 列
         * @param a44 第 4 行第 4 列
         */
        // clang-format off
    Matrix4D(float a11, float a12, float a13, float a14,
             float a21, float a22, float a23, float a24,
             float a31, float a32, float a33, float a34,
             float a41, float a42, float a43, float a44);

    /**
     * @brief 按 16 个双精度元素构造（行优先）
     * @param a11 第 1 行第 1 列
     * @param a12 第 1 行第 2 列
     * @param a13 第 1 行第 3 列
     * @param a14 第 1 行第 4 列
     * @param a21 第 2 行第 1 列
     * @param a22 第 2 行第 2 列
     * @param a23 第 2 行第 3 列
     * @param a24 第 2 行第 4 列
     * @param a31 第 3 行第 1 列
     * @param a32 第 3 行第 2 列
     * @param a33 第 3 行第 3 列
     * @param a34 第 3 行第 4 列
     * @param a41 第 4 行第 1 列
     * @param a42 第 4 行第 2 列
     * @param a43 第 4 行第 3 列
     * @param a44 第 4 行第 4 列
     */
    Matrix4D(double a11, double a12, double a13, double a14,
             double a21, double a22, double a23, double a24,
             double a31, double a32, double a33, double a34,
             double a41, double a42, double a43, double a44);
        // clang-format on

        /**
         * @brief 拷贝构造
         * @param other 被拷贝的矩阵
         */
        Matrix4D(const Matrix4D &other);

        /**
         * @brief 按任意轴与转角构造旋转矩阵（单精度）
         * @param base 轴上一点
         * @param direction 轴方向，内部会归一化
         * @param angle 旋转角（弧度）
         * @throws ValueError 轴方向为零向量时抛出
         */
        Matrix4D(const Vector3f &base, const Vector3f &direction, float angle);

        /**
         * @brief 按任意轴与转角构造旋转矩阵（双精度）
         * @param base 轴上一点
         * @param direction 轴方向，内部会归一化
         * @param angle 旋转角（弧度）
         * @throws ValueError 轴方向为零向量时抛出
         */
        Matrix4D(const Vector3d &base, const Vector3d &direction, double angle);

        /**
         * @brief 析构函数
         */
        ~Matrix4D() = default;

        /**
         * @brief 矩阵加法
         * @param other 加数
         * @return 逐元素相加的新矩阵
         */
        inline Matrix4D operator+(const Matrix4D &other) const;

        /**
         * @brief 就地矩阵加法
         * @param other 加数
         * @return 自身引用
         */
        inline Matrix4D &operator+=(const Matrix4D &other);

        /**
         * @brief 矩阵减法
         * @param other 减数
         * @return 逐元素相减的新矩阵
         */
        inline Matrix4D operator-(const Matrix4D &other) const;

        /**
         * @brief 就地矩阵减法
         * @param other 减数
         * @return 自身引用
         */
        inline Matrix4D &operator-=(const Matrix4D &other);

        /**
         * @brief 就地矩阵乘法
         * @param other 右乘矩阵
         * @return 自身引用
         */
        inline Matrix4D &operator*=(const Matrix4D &other);

        /**
         * @brief 拷贝赋值
         * @param other 被赋值的矩阵
         * @return 自身引用
         */
        inline Matrix4D &operator=(const Matrix4D &other);

        /**
         * @brief 矩阵乘法
         * @param other 右乘矩阵
         * @return 乘积矩阵
         */
        inline Matrix4D operator*(const Matrix4D &other) const;

        /**
         * @brief 用矩阵变换向量（含平移分量）
         * @param vector 待变换向量
         * @return 变换后的向量
         */
        inline Vector3f operator*(const Vector3f &vector) const;

        /**
         * @brief 用矩阵变换向量（含平移分量）
         * @param vector 待变换向量
         * @return 变换后的向量
         */
        inline Vector3d operator*(const Vector3d &vector) const;

        /**
         * @brief 就地变换向量（含平移分量）
         * @param source 待变换向量
         * @param destination 输出：变换结果，可与 source 为同一对象
         */
        inline void multiplyVector(const Vector3d &source, Vector3d &destination) const;

        /**
         * @brief 就地变换向量（含平移分量）
         * @param source 待变换向量
         * @param destination 输出：变换结果，可与 source 为同一对象
         */
        inline void multiplyVector(const Vector3f &source, Vector3f &destination) const;

        /**
         * @brief 矩阵数乘
         * @param scalar 缩放因子
         * @return 缩放后的新矩阵
         */
        inline Matrix4D operator*(double scalar) const;

        /**
         * @brief 就地数乘
         * @param scalar 缩放因子
         * @return 自身引用
         */
        inline Matrix4D &operator*=(double scalar);

        /**
         * @brief 按容差比较不等
         * @param other 另一矩阵
         * @return 任一元素差异超过机器精度时返回 true
         */
        inline bool operator!=(const Matrix4D &other) const;

        /**
         * @brief 按容差比较相等
         * @param other 另一矩阵
         * @return 所有元素差异都不超过机器精度时返回 true
         */
        inline bool operator==(const Matrix4D &other) const;

        /**
         * @brief 取可写行引用
         * @param index 行下标，取值 0..3
         * @return 该行 4 个元素的数组引用
         * @throws IndexError 行下标超出 [0,3] 时抛出
         */
        inline std::array<double, 4> &operator[](unsigned int index);

        /**
         * @brief 取只读行引用
         * @param index 行下标，取值 0..3
         * @return 该行 4 个元素的常量数组引用
         * @throws IndexError 行下标超出 [0,3] 时抛出
         */
        inline const std::array<double, 4> &operator[](unsigned int index) const;

        /**
         * @brief 取一行并截去第 4 个元素
         * @param index 行下标，取值 0..3
         * @return 该行前三分量组成的向量
         */
        inline Vector3d getRow(unsigned int index) const;

        /**
         * @brief 取一列并截去第 4 个元素
         * @param index 列下标，取值 0..3
         * @return 该列前三分量组成的向量
         */
        inline Vector3d getCol(unsigned int index) const;

        /**
         * @brief 取 3x3 子矩阵的对角线
         * @return (m00, m11, m22)
         */
        inline Vector3d diagonal() const;

        /**
         * @brief 取 3x3 子矩阵的迹
         * @return m00 + m11 + m22
         */
        inline double trace3() const;

        /**
         * @brief 取 4x4 矩阵的迹
         * @return m00 + m11 + m22 + m33
         */
        inline double trace() const;

        /**
         * @brief 用向量写入一行
         * @param index 行下标，取值 0..3
         * @param vector 写入的前三分量，第 4 个元素保持不变
         */
        inline void setRow(unsigned int index, const Vector3d &vector);

        /**
         * @brief 用向量写入一列
         * @param index 列下标，取值 0..3
         * @param vector 写入的前三分量，第 4 个元素保持不变
         */
        inline void setCol(unsigned int index, const Vector3d &vector);

        /**
         * @brief 写入 3x3 子矩阵的对角线
         * @param vector (m00, m11, m22)
         */
        inline void setDiagonal(const Vector3d &vector);

        /**
         * @brief 求 4x4 行列式
         * @return 行列式值
         */
        double determinant() const;

        /**
         * @brief 求 3x3 子矩阵行列式
         * @return 子矩阵行列式值
         */
        double determinant3() const;

        /**
         * @brief 分析矩阵描述的变换并用文本描述
         * @return 如 Unity Matrix、Scale [...]、Rotation Matrix 等的英文描述
         */
        std::string analyse() const;

        /**
         * @brief 计算外积（并矢）矩阵
         * @param firstVector 左向量
         * @param secondVector 右向量
         * @return 自身引用
         */
        Matrix4D &outer(const Vector3f &firstVector, const Vector3f &secondVector);

        /**
         * @brief 计算外积（并矢）矩阵
         * @param firstVector 左向量
         * @param secondVector 右向量
         * @return 自身引用
         */
        Matrix4D &outer(const Vector3d &firstVector, const Vector3d &secondVector);

        /**
         * @brief 计算反对称矩阵（帽算子）
         * @param vector 生成反对称矩阵的向量
         * @return 自身引用
         */
        Matrix4D &hat(const Vector3f &vector);

        /**
         * @brief 计算反对称矩阵（帽算子）
         * @param vector 生成反对称矩阵的向量
         * @return 自身引用
         */
        Matrix4D &hat(const Vector3d &vector);

        /**
         * @brief 按行优先导出 16 个元素
         * @param values 输出：长度 16 的数组
         */
        void getMatrix(double values[16]) const;

        /**
         * @brief 按行优先导入 16 个元素
         * @param values 长度 16 的数组
         */
        void setMatrix(const double values[16]);

        /**
         * @brief 按 OpenGL 列主序导出 16 个元素
         * @param values 输出：长度 16 的数组
         */
        void getOpenGlMatrix(double values[16]) const;

        /**
         * @brief 按 OpenGL 列主序导入 16 个元素
         * @param values 长度 16 的数组
         */
        void setOpenGlMatrix(const double values[16]);

        /**
         * @brief 取对象占用的字节数
         * @return sizeof(Matrix4D)
         */
        unsigned long getMemSpace() const;

        /**
         * @brief 重置为单位阵
         */
        void setToUnity();

        /**
         * @brief 判断是否为单位阵（零容差）
         * @return 与单位阵逐位相等时返回 true
         */
        bool isUnity() const;

        /**
         * @brief 按容差判断是否为单位阵
         * @param tolerance 元素容差
         * @return 对角元素与 1、非对角元素与 0 的偏差都不超过容差时返回 true
         */
        bool isUnity(double tolerance) const;

        /**
         * @brief 重置为零矩阵
         */
        void nullify();

        /**
         * @brief 判断是否为零矩阵
         * @return 16 个元素全为 0 时返回 true
         */
        bool isNull() const;

        /**
         * @brief 平移坐标系
         * @param x X 方向平移量
         * @param y Y 方向平移量
         * @param z Z 方向平移量
         */
        void move(float x, float y, float z)
        {
            move(Vector3f(x, y, z));
        }

        /**
         * @brief 平移坐标系
         * @param x X 方向平移量
         * @param y Y 方向平移量
         * @param z Z 方向平移量
         */
        void move(double x, double y, double z)
        {
            move(Vector3d(x, y, z));
        }

        /**
         * @brief 平移坐标系
         * @param vector 平移量
         */
        void move(const Vector3f &vector);

        /**
         * @brief 平移坐标系
         * @param vector 平移量
         */
        void move(const Vector3d &vector);

        /**
         * @brief 三分量分别缩放
         * @param x X 方向缩放因子
         * @param y Y 方向缩放因子
         * @param z Z 方向缩放因子
         */
        void scale(float x, float y, float z)
        {
            scale(Vector3f(x, y, z));
        }

        /**
         * @brief 三分量分别缩放
         * @param x X 方向缩放因子
         * @param y Y 方向缩放因子
         * @param z Z 方向缩放因子
         */
        void scale(double x, double y, double z)
        {
            scale(Vector3d(x, y, z));
        }

        /**
         * @brief 三分量分别缩放
         * @param vector 缩放因子向量
         */
        void scale(const Vector3f &vector);

        /**
         * @brief 三分量分别缩放
         * @param vector 缩放因子向量
         */
        void scale(const Vector3d &vector);

        /**
         * @brief 三轴同倍率缩放
         * @param uniformScale 统一缩放因子
         */
        void scale(float uniformScale)
        {
            scale(Vector3f(uniformScale, uniformScale, uniformScale));
        }

        /**
         * @brief 三轴同倍率缩放
         * @param uniformScale 统一缩放因子
         */
        void scale(double uniformScale)
        {
            scale(Vector3d(uniformScale, uniformScale, uniformScale));
        }

        /**
         * @brief 判定矩阵携带的缩放类型
         * @param tolerance 判定容差，传 0 时使用内部默认容差 1e-9
         * @return 缩放类型
         */
        ScaleType hasScale(double tolerance = 0.0) const;

        /**
         * @brief 把矩阵分解为剪切、缩放、旋转与平移四部分
         * @details 满足 matrix = move * rotation * scale * shear 的乘积关系。
         * @return 依次为剪切、缩放、旋转、平移的四个矩阵
         */
        std::array<Matrix4D, 4> decompose() const;

        /**
         * @brief 绕 X 轴旋转（作用于已变换空间）
         * @param angle 旋转角（弧度）
         */
        void rotateX(double angle);

        /**
         * @brief 绕 Y 轴旋转（作用于已变换空间）
         * @param angle 旋转角（弧度）
         */
        void rotateY(double angle);

        /**
         * @brief 绕 Z 轴旋转（作用于已变换空间）
         * @param angle 旋转角（弧度）
         */
        void rotateZ(double angle);

        /**
         * @brief 绕过原点的任意轴旋转（单精度）
         * @param vector 轴方向
         * @param angle 旋转角（弧度）
         * @throws ValueError 轴方向为零向量时抛出
         */
        void rotateLine(const Vector3f &vector, float angle);

        /**
         * @brief 绕过原点的任意轴旋转（双精度）
         * @param vector 轴方向
         * @param angle 旋转角（弧度）
         * @throws ValueError 轴方向为零向量时抛出
         */
        void rotateLine(const Vector3d &vector, double angle);

        /**
         * @brief 绕任意轴（不必过原点）旋转（单精度）
         * @param base 轴上一点
         * @param direction 轴方向
         * @param angle 旋转角（弧度）
         * @throws ValueError 轴方向为零向量时抛出
         */
        void rotateLine(const Vector3f &base, const Vector3f &direction, float angle);

        /**
         * @brief 绕任意轴（不必过原点）旋转（双精度）
         * @param base 轴上一点
         * @param direction 轴方向
         * @param angle 旋转角（弧度）
         * @throws ValueError 轴方向为零向量时抛出
         */
        void rotateLine(const Vector3d &base, const Vector3d &direction, double angle);

        /**
         * @brief 从矩阵反解旋转轴、转角与沿轴平移量（单精度）
         * @param base 输出：轴上一点
         * @param direction 输出：归一化的轴方向
         * @param angle 输出：旋转角（弧度）
         * @param translation 输出：沿轴方向的平移量
         * @return 3x3 子矩阵正交时返回 true，否则原样返回 false
         */
        bool toAxisAngle(Vector3f &base, Vector3f &direction, float &angle, float &translation) const;

        /**
         * @brief 从矩阵反解旋转轴、转角与沿轴平移量（双精度）
         * @param base 输出：轴上一点
         * @param direction 输出：归一化的轴方向
         * @param angle 输出：旋转角（弧度）
         * @param translation 输出：沿轴方向的平移量
         * @return 3x3 子矩阵正交时返回 true，否则原样返回 false
         */
        bool toAxisAngle(Vector3d &base, Vector3d &direction, double &angle, double &translation) const;

        /**
         * @brief 围绕给定点施加变换：先平移到该点、施加变换、再平移回去
         * @param vector 参考点
         * @param matrix 施加的变换
         */
        void transform(const Vector3f &vector, const Matrix4D &matrix);

        /**
         * @brief 围绕给定点施加变换：先平移到该点、施加变换、再平移回去
         * @param vector 参考点
         * @param matrix 施加的变换
         */
        void transform(const Vector3d &vector, const Matrix4D &matrix);

        /**
         * @brief 求逆（要求 3x3 子矩阵为正交旋转）
         * @details 逆 = 转置的旋转部分 × 反向平移，不做高斯消元。
         */
        void inverse();

        /**
         * @brief 正交矩阵求逆：转置后同步修正平移分量
         * @details 只对 3x3 子矩阵为正交矩阵的情况成立（矩阵仅含旋转与平移），比 inverseGauss() 快且无奇异风险。
         */
        void inverseOrthogonal();

        /**
         * @brief 任意非奇异矩阵求逆（高斯-约当消元）
         * @throws ValueError 矩阵奇异或求逆结果不可信时抛出
         */
        void inverseGauss();

        /**
         * @brief 就地转置
         */
        void transpose();

        /**
         * @brief 打印 4x4 元素到标准输出
         * @details 仅用于调试：直接写 stdout，不经由日志框架，也不随区域设置改变小数格式。
         */
        void print() const;

        /**
         * @brief 把 16 个元素序列化为空格分隔的文本
         * @return 可直接交给 fromString() 还原的文本
         */
        std::string toString() const;

        /**
         * @brief 从空格分隔文本读入 16 个元素
         * @param text 由 toString() 生成的文本
         * @throws ValueError 文本中的元素无法全部解析为浮点数时抛出
         */
        void fromString(const std::string &text);

    private:
        using Array2d = std::array<std::array<double, 4>, 4>; ///< 行优先 4x4 存储类型
        Array2d m_matrix;                                     ///< 行优先存储的矩阵元素
    };

    inline Matrix4D Matrix4D::operator+(const Matrix4D &other) const
    {
        Matrix4D result(*this);
        return result += other;
    }

    inline Matrix4D &Matrix4D::operator+=(const Matrix4D &other)
    {
        // 逐元素相加：两个操作数布局相同，编译器可向量化
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                m_matrix[row][column] += other.m_matrix[row][column];
            }
        }

        return *this;
    }

    inline Matrix4D Matrix4D::operator-(const Matrix4D &other) const
    {
        Matrix4D result(*this);
        return result -= other;
    }

    inline Matrix4D &Matrix4D::operator-=(const Matrix4D &other)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                m_matrix[row][column] -= other.m_matrix[row][column];
            }
        }

        return *this;
    }

    inline Matrix4D &Matrix4D::operator*=(const Matrix4D &other)
    {
        (*this) = (*this) * other;
        return *this;
    }

    inline Matrix4D Matrix4D::operator*(const Matrix4D &other) const
    {
        Matrix4D result;

        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                // 先清零再累加内积，运算次序与 FreeCAD 保持一致，保证结果可复现
                result.m_matrix[row][column] = 0.0;
                for (int inner = 0; inner < 4; ++inner)
                {
                    result.m_matrix[row][column] += m_matrix[row][inner] * other.m_matrix[inner][column];
                }
            }
        }

        return result;
    }

    inline Matrix4D &Matrix4D::operator=(const Matrix4D &other)
    {
        // 自赋值直接返回，省去 16 次无意义拷贝
        if (this == &other)
        {
            return *this;
        }

        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                m_matrix[row][column] = other.m_matrix[row][column];
            }
        }

        return *this;
    }

    inline Vector3f Matrix4D::operator*(const Vector3f &vector) const
    {
        Vector3f destination;
        multiplyVector(vector, destination);
        return destination;
    }

    inline Vector3d Matrix4D::operator*(const Vector3d &vector) const
    {
        Vector3d destination;
        multiplyVector(vector, destination);
        return destination;
    }

    inline void Matrix4D::multiplyVector(const Vector3d &source, Vector3d &destination) const
    {
        // 齐次坐标隐含 w = 1，所以平移分量（第 4 列）直接参与累加
        const double x = (m_matrix[0][0] * source.x + m_matrix[0][1] * source.y + m_matrix[0][2] * source.z + m_matrix[0][3]);
        const double y = (m_matrix[1][0] * source.x + m_matrix[1][1] * source.y + m_matrix[1][2] * source.z + m_matrix[1][3]);
        const double z = (m_matrix[2][0] * source.x + m_matrix[2][1] * source.y + m_matrix[2][2] * source.z + m_matrix[2][3]);
        destination.set(x, y, z);
    }

    inline void Matrix4D::multiplyVector(const Vector3f &source, Vector3f &destination) const
    {
        // 累加在 double 下进行，只有最终结果落回 float，减少单精度累积误差
        const double x = static_cast<double>(source.x);
        const double y = static_cast<double>(source.y);
        const double z = static_cast<double>(source.z);

        const double resultX = (m_matrix[0][0] * x + m_matrix[0][1] * y + m_matrix[0][2] * z + m_matrix[0][3]);
        const double resultY = (m_matrix[1][0] * x + m_matrix[1][1] * y + m_matrix[1][2] * z + m_matrix[1][3]);
        const double resultZ = (m_matrix[2][0] * x + m_matrix[2][1] * y + m_matrix[2][2] * z + m_matrix[2][3]);
        destination.set(static_cast<float>(resultX), static_cast<float>(resultY), static_cast<float>(resultZ));
    }

    inline Matrix4D Matrix4D::operator*(double scalar) const
    {
        Matrix4D result(*this);
        return result *= scalar;
    }

    inline Matrix4D &Matrix4D::operator*=(double scalar)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                m_matrix[row][column] *= scalar;
            }
        }

        return *this;
    }

    inline bool Matrix4D::operator==(const Matrix4D &other) const
    {
        // 按机器精度容差比较，逐位相等会把舍入差异误判为不等
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                if (std::abs(m_matrix[row][column] - other.m_matrix[row][column]) > TraitsType::epsilon())
                {
                    return false;
                }
            }
        }

        return true;
    }

    inline bool Matrix4D::operator!=(const Matrix4D &other) const
    {
        return !((*this) == other);
    }

    /**
     * @brief 用矩阵就地变换向量
     * @param vector 被变换的向量，结果就地写回
     * @param matrix 变换矩阵
     * @return 变换后的向量引用
     */
    inline Vector3f &operator*=(Vector3f &vector, const Matrix4D &matrix)
    {
        matrix.multiplyVector(vector, vector);
        return vector;
    }

    inline std::array<double, 4> &Matrix4D::operator[](unsigned int index)
    {
        if (index > 3)
        {
            // 越界行若静默返回，会把调用方的下标错误变成隐蔽的数据损坏
            throw IndexError("矩阵行下标越界（只允许 0/1/2/3）；请改行下标，或用 getMatrix() 取全部元素");
        }
        return m_matrix[index];
    }

    inline const std::array<double, 4> &Matrix4D::operator[](unsigned int index) const
    {
        if (index > 3)
        {
            throw IndexError("矩阵行下标越界（只允许 0/1/2/3）；请改行下标，或用 getMatrix() 取全部元素");
        }
        return m_matrix[index];
    }

    inline Vector3d Matrix4D::getRow(unsigned int index) const
    {
        return Vector3d(m_matrix[index][0], m_matrix[index][1], m_matrix[index][2]);
    }

    inline Vector3d Matrix4D::getCol(unsigned int index) const
    {
        return Vector3d(m_matrix[0][index], m_matrix[1][index], m_matrix[2][index]);
    }

    inline Vector3d Matrix4D::diagonal() const
    {
        return Vector3d(m_matrix[0][0], m_matrix[1][1], m_matrix[2][2]);
    }

    inline double Matrix4D::trace3() const
    {
        return m_matrix[0][0] + m_matrix[1][1] + m_matrix[2][2];
    }

    inline double Matrix4D::trace() const
    {
        return m_matrix[0][0] + m_matrix[1][1] + m_matrix[2][2] + m_matrix[3][3];
    }

    inline void Matrix4D::setRow(unsigned int index, const Vector3d &vector)
    {
        m_matrix[index][0] = vector.x;
        m_matrix[index][1] = vector.y;
        m_matrix[index][2] = vector.z;
    }

    inline void Matrix4D::setCol(unsigned int index, const Vector3d &vector)
    {
        m_matrix[0][index] = vector.x;
        m_matrix[1][index] = vector.y;
        m_matrix[2][index] = vector.z;
    }

    inline void Matrix4D::setDiagonal(const Vector3d &vector)
    {
        m_matrix[0][0] = vector.x;
        m_matrix[1][1] = vector.y;
        m_matrix[2][2] = vector.z;
    }
} // namespace ExpressionEngine::Base
