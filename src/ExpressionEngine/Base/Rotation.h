/**
 * @file Rotation.h
 * @brief 三维旋转（内部为四元数，附轴角与欧拉角转换）
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <array>

#include <ExpressionEngine/Base/Vector3D.h>

namespace ExpressionEngine::Base
{
    class Matrix4D;

    /**
     * @brief 三维旋转，内部以单位四元数表示
     * @details 四元数分量按 (x, y, z, w) 排列，公开 API 与 FreeCAD Base::Rotation 完全一致。
     *          所有 setValue() 入口都会归一化四元数；轴角另存一份，使旋转角为 0 时仍能取回用户给定的
     *          转轴方向。零四元数不会被自动纠正，调用方应先查 isNull() 再使用其轴角结果。
     */
    class Rotation
    {
    public:
        /**
         * @brief 构造单位旋转
         */
        Rotation();

        /**
         * @brief 以转轴与转角构造
         * @param axis 转轴，可为非单位向量；零向量时沿用 Z 轴
         * @param angle 转角，单位弧度
         */
        Rotation(const Vector3d &axis, double angle);

        /**
         * @brief 取 4x4 矩阵的旋转部分构造
         * @param matrix 源矩阵，仅使用其分解出的旋转分量，允许带缩放或剪切
         */
        Rotation(const Matrix4D &matrix);

        /**
         * @brief 以四元数分量数组构造
         * @param q 四元数分量，顺序为 x, y, z, w
         */
        Rotation(const double q[4]);

        /**
         * @brief 以四元数分量构造
         * @param q0 x 分量
         * @param q1 y 分量
         * @param q2 z 分量
         * @param q3 w 分量
         */
        Rotation(double q0, double q1, double q2, double q3);

        /**
         * @brief 构造把 rotateFrom 方向转到 rotateTo 方向的旋转
         * @param rotateFrom 起始方向，零向量时方向未定义
         * @param rotateTo 目标方向
         */
        Rotation(const Vector3d &rotateFrom, const Vector3d &rotateTo);

        /**
         * @brief 拷贝构造
         * @param other 被拷贝的旋转
         */
        Rotation(const Rotation &other) = default;

        /**
         * @brief 移动构造
         * @param other 被移动的旋转，移动后处于有效但未指定状态
         */
        Rotation(Rotation &&other) = default;

        /**
         * @brief 析构函数
         */
        ~Rotation() = default;

        /**
         * @brief 欧拉角序列标识
         * @details 命名与 FreeCAD 一致：Extrinsic_ 为绕固定轴转动（外旋），Intrinsic_ 为绕自身新轴转动
         *          （内旋），后缀三个字母是绕轴顺序。
         */
        enum EulerSequence
        {
            Invalid, ///< 非法序列，名字解析失败时返回

            EulerAngles, ///< 经典欧拉角，等价于 Intrinsic_ZXZ

            YawPitchRoll, ///< 偏航-俯仰-滚转（航海角），等价于 Intrinsic_ZYX

            // 泰特-布莱恩角（三个轴互不相同）
            Extrinsic_XYZ, ///< 外旋，轴序 XYZ
            Extrinsic_XZY, ///< 外旋，轴序 XZY
            Extrinsic_YZX, ///< 外旋，轴序 YZX
            Extrinsic_YXZ, ///< 外旋，轴序 YXZ
            Extrinsic_ZXY, ///< 外旋，轴序 ZXY
            Extrinsic_ZYX, ///< 外旋，轴序 ZYX

            Intrinsic_XYZ, ///< 内旋，轴序 XYZ
            Intrinsic_XZY, ///< 内旋，轴序 XZY
            Intrinsic_YZX, ///< 内旋，轴序 YZX
            Intrinsic_YXZ, ///< 内旋，轴序 YXZ
            Intrinsic_ZXY, ///< 内旋，轴序 ZXY
            Intrinsic_ZYX, ///< 内旋，轴序 ZYX

            // 真欧拉角（第一、第三轴相同）
            Extrinsic_XYX, ///< 外旋，轴序 XYX
            Extrinsic_XZX, ///< 外旋，轴序 XZX
            Extrinsic_YZY, ///< 外旋，轴序 YZY
            Extrinsic_YXY, ///< 外旋，轴序 YXY
            Extrinsic_ZYZ, ///< 外旋，轴序 ZYZ
            Extrinsic_ZXZ, ///< 外旋，轴序 ZXZ

            Intrinsic_XYX, ///< 内旋，轴序 XYX
            Intrinsic_XZX, ///< 内旋，轴序 XZX
            Intrinsic_YZY, ///< 内旋，轴序 YZY
            Intrinsic_YXY, ///< 内旋，轴序 YXY
            Intrinsic_ZXZ, ///< 内旋，轴序 ZXZ
            Intrinsic_ZYZ, ///< 内旋，轴序 ZYZ

            EulerSequenceLast, ///< 序列计数哨兵，不是合法序列
        };

        /**
         * @brief 构造把 Z 轴转到 normal 方向的旋转
         * @param normal 目标法向
         * @return 旋转；normal 为零向量时方向未定义
         */
        static Rotation fromNormalVector(const Vector3d &normal);

        /**
         * @brief 以欧拉角构造旋转
         * @param order 欧拉角序列
         * @param alpha 第一转角，单位度
         * @param beta 第二转角，单位度
         * @param gamma 第三转角，单位度
         * @return 旋转
         * @throws ValueError order 为 Invalid 或越界
         */
        static Rotation fromEulerAngles(EulerSequence order, double alpha, double beta, double gamma);

        /**
         * @brief 取四元数分量的裸指针
         * @return 指向内部 4 个 double 的指针，顺序 x, y, z, w
         * @warning 指针指向对象内部存储，对象被修改或销毁后即失效
         */
        const double *getValue() const;

        /**
         * @brief 按分量取出四元数
         * @param q0 输出 x 分量
         * @param q1 输出 y 分量
         * @param q2 输出 z 分量
         * @param q3 输出 w 分量
         */
        void getValue(double &q0, double &q1, double &q2, double &q3) const;

        /**
         * @brief 按分量设置四元数，设置后归一化
         * @param q0 x 分量
         * @param q1 y 分量
         * @param q2 z 分量
         * @param q3 w 分量
         */
        void setValue(double q0, double q1, double q2, double q3);

        /**
         * @brief 取归一化的转轴与转角
         * @param axis 输出转轴（已归一化）
         * @param angle 输出转角，单位弧度
         * @note 零四元数时 axis 为零向量、angle 为 acos(0) 的兜底值，调用方应先查 isNull()
         */
        void getValue(Vector3d &axis, double &angle) const;

        /**
         * @brief 取原始转轴与转角，不归一化转轴
         * @param axis 输出转轴，保持 setValue(axis, angle) 传入的原样长度
         * @param angle 输出转角，单位弧度
         */
        void getRawValue(Vector3d &axis, double &angle) const;

        /**
         * @brief 输出等价的旋转矩阵
         * @param matrix 输出矩阵，第四行第四列被写为齐次分量
         * @warning 零四元数会使结果出现 NaN，调用方应先查 isNull()
         */
        void getValue(Matrix4D &matrix) const;

        /**
         * @brief 以四元数分量数组设置，设置后归一化
         * @param q 四元数分量，顺序 x, y, z, w
         */
        void setValue(const double q[4]);

        /**
         * @brief 以矩阵的旋转部分设置
         * @param matrix 源矩阵，仅使用其分解出的旋转分量
         */
        void setValue(const Matrix4D &matrix);

        /**
         * @brief 以转轴与转角设置
         * @param axis 转轴，可为非单位向量；零向量时沿用当前轴
         * @param angle 转角，单位弧度，内部会折算到 [0, 2π)
         */
        void setValue(const Vector3d &axis, double angle);

        /**
         * @brief 设置把 rotateFrom 方向转到 rotateTo 方向的最短旋转
         * @param rotateFrom 起始方向
         * @param rotateTo 目标方向
         */
        void setValue(const Vector3d &rotateFrom, const Vector3d &rotateTo);

        /**
         * @brief 以偏航-俯仰-滚转角设置
         * @param yaw 偏航角，单位度
         * @param pitch 俯仰角，单位度
         * @param roll 滚转角，单位度
         */
        void setYawPitchRoll(double yaw, double pitch, double roll);

        /**
         * @brief 取偏航-俯仰-滚转角
         * @param yaw 输出偏航角，单位度
         * @param pitch 输出俯仰角，单位度
         * @param roll 输出滚转角，单位度
         */
        void getYawPitchRoll(double &yaw, double &pitch, double &roll) const;

        /**
         * @brief 取序列对应的名字
         * @param sequence 欧拉角序列
         * @return 名字字面量；Invalid 或越界时返回 nullptr
         */
        static const char *eulerSequenceName(EulerSequence sequence);

        /**
         * @brief 按名字解析欧拉角序列，大小写不敏感
         * @param name 序列名字，如 "YawPitchRoll"、"ZXY"、"IXYZ"
         * @return 对应序列；name 为空或不能识别时返回 Invalid
         */
        static EulerSequence eulerSequenceFromName(const char *name);

        /**
         * @brief 取欧拉角
         * @param order 欧拉角序列
         * @param alpha 输出第一转角，单位度
         * @param beta 输出第二转角，单位度
         * @param gamma 输出第三转角，单位度
         * @throws ValueError order 为 Invalid 或越界
         */
        void getEulerAngles(EulerSequence order, double &alpha, double &beta, double &gamma) const;

        /**
         * @brief 设置欧拉角
         * @param order 欧拉角序列
         * @param alpha 第一转角，单位度
         * @param beta 第二转角，单位度
         * @param gamma 第三转角，单位度
         * @throws ValueError order 为 Invalid 或越界
         */
        void setEulerAngles(EulerSequence order, double alpha, double beta, double gamma);

        /**
         * @brief 判断是否恰为单位旋转（四元数精确等于 ±(0,0,0,1)）
         * @return true 是单位旋转
         */
        bool isIdentity() const;

        /**
         * @brief 判断在容差内是否为单位旋转
         * @param tolerance 容差，作用在四元数点积上
         * @return true 是单位旋转
         */
        bool isIdentity(double tolerance) const;

        /**
         * @brief 判断四元数是否全零
         * @return true 四元数四个分量全为 0，此时旋转无定义
         */
        bool isNull() const;

        /**
         * @brief 判断是否为同一个旋转（四元数精确相等，允许整体取反）
         * @param other 待比较的旋转
         * @return true 表示两个四元数互为同一旋转的两种表示
         */
        bool isSame(const Rotation &other) const;

        /**
         * @brief 判断在容差内是否为同一个旋转
         * @param other 待比较的旋转
         * @param tolerance 容差，作用在两个四元数单位化后的点积上
         * @return true 表示两个旋转在容差内一致
         * @note 容差判据假定双方四元数均为单位长
         */
        bool isSame(const Rotation &other, double tolerance) const;

        /**
         * @brief 就地取逆（四元数共轭）
         * @return 自身引用
         */
        Rotation &invert();

        /**
         * @brief 取逆（四元数共轭）
         * @return 新的旋转对象
         */
        Rotation inverse() const;

        /**
         * @brief 就地右乘另一个旋转
         * @param other 右操作数，先施加它
         * @return 自身引用
         */
        Rotation &operator*=(const Rotation &other);

        /**
         * @brief 右乘另一个旋转
         * @param other 右操作数，先施加它
         * @return 复合后的新旋转
         */
        Rotation operator*(const Rotation &other) const;

        /**
         * @brief 精确比较，等价于 isSame(other)
         * @param other 待比较的旋转
         * @return true 表示表示同一个旋转
         */
        bool operator==(const Rotation &other) const;

        /**
         * @brief 精确比较的取反
         * @param other 待比较的旋转
         * @return true 表示不是同一个旋转
         */
        bool operator!=(const Rotation &other) const;

        /**
         * @brief 访问四元数分量
         * @param index 分量下标，0..3 依次为 x, y, z, w
         * @return 该分量的引用
         */
        double &operator[](unsigned short index)
        {
            return m_quaternion[index];
        }

        /**
         * @brief 只读访问四元数分量
         * @param index 分量下标，0..3 依次为 x, y, z, w
         * @return 该分量的常量引用
         */
        const double &operator[](unsigned short index) const
        {
            return m_quaternion[index];
        }

        /**
         * @brief 拷贝赋值
         * @return 自身引用
         */
        Rotation &operator=(const Rotation &) = default;

        /**
         * @brief 移动赋值
         * @return 自身引用
         */
        Rotation &operator=(Rotation &&) = default;

        /**
         * @brief 就地右乘
         * @param other 右操作数
         * @return 自身引用
         */
        Rotation &multRight(const Rotation &other);

        /**
         * @brief 就地左乘
         * @param other 左操作数
         * @return 自身引用
         */
        Rotation &multLeft(const Rotation &other);

        /**
         * @brief 用本旋转变换向量
         * @param source 输入向量
         * @param destination 输出向量，可与 source 为同一对象
         */
        void multVec(const Vector3d &source, Vector3d &destination) const;

        /**
         * @brief 用本旋转变换向量
         * @param source 输入向量
         * @return 变换后的向量
         */
        Vector3d multVec(const Vector3d &source) const;

        /**
         * @brief 用本旋转变换单精度向量
         * @param source 输入向量
         * @param destination 输出向量，可与 source 为同一对象
         */
        void multVec(const Vector3f &source, Vector3f &destination) const;

        /**
         * @brief 用本旋转变换单精度向量
         * @param source 输入向量
         * @return 变换后的向量
         */
        Vector3f multVec(const Vector3f &source) const;

        /**
         * @brief 按比例缩放旋转角，转轴保持
         * @param scaleFactor 缩放系数
         */
        void scaleAngle(double scaleFactor);

        /**
         * @brief 球面线性插值
         * @param q0 起点旋转，t=0 时返回它
         * @param q1 终点旋转，t=1 时返回它
         * @param t 插值参数，越界时被钳制到 [0, 1]
         * @return 插值结果
         */
        static Rotation slerp(const Rotation &q0, const Rotation &q1, double t);

        /**
         * @brief 取单位旋转
         * @return 四元数为 (0, 0, 0, 1) 的旋转
         */
        static Rotation identity();

        /**
         * @brief 由三个方向向量构造旋转
         * @details priorityOrder 中排在前面的方向优先被精确采用：第一个方向作为主轴；第二个方向
         *          只用于确定垂直于主轴的提示方向；第三个方向完全由前两者叉乘得出。若某个方向
         *          为零向量，则按优先级顺延到下一个方向，全部为零时抛 ValueError。
         * @param xDirection 局部 X 轴期望方向
         * @param yDirection 局部 Y 轴期望方向
         * @param zDirection 局部 Z 轴期望方向
         * @param priorityOrder 三个大写轴字母构成的优先级串，默认 "ZXY"
         * @return 把局部坐标轴映射到给定方向的旋转
         * @throws ValueError priorityOrder 不是三个互不相同的大写 X/Y/Z 字母，或三个方向全为零向量
         */
        static Rotation makeRotationByAxes(Vector3d xDirection, Vector3d yDirection, Vector3d zDirection, const char *priorityOrder = "ZXY");

    private:
        /**
         * @brief 把四元数缩放到单位长，零四元数保持为零
         */
        void normalize();

        /**
         * @brief 由四元数反解轴角缓存
         */
        void evaluateVector();

        std::array<double, 4> m_quaternion; ///< 单位四元数分量，顺序 x, y, z, w
        Vector3d              m_axis;       ///< 转轴缓存，用于旋转角为 0 时保留方向
        double                m_angle;      ///< 转角缓存，单位弧度，保留用户给定的角度
    };
} // namespace ExpressionEngine::Base
