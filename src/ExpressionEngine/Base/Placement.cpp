#include <ExpressionEngine/Base/Placement.h>

#include <format>

#include <ExpressionEngine/Base/DualQuaternion.h>
#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Rotation.h>

namespace ExpressionEngine::Base
{
    Placement::Placement() = default;

    Placement::Placement(const Matrix4D &matrix)
    {
        fromMatrix(matrix);
    }

    Placement::Placement(const Vector3d &position, const Rotation &rotation) : m_position(position), m_rotation(rotation)
    {
    }

    Placement::Placement(const Vector3d &position, const Rotation &rotation, const Vector3d &center) : m_rotation(rotation)
    {
        // 绕局部点 center 旋转：把 center 的像从位置中扣除，使其在变换后落在 center + position
        Vector3d rotatedCenter = center;
        rotation.multVec(rotatedCenter, rotatedCenter);
        m_position = position + center - rotatedCenter;
    }

    Placement Placement::fromDualQuaternion(DualQuat dualQuaternion)
    {
        // 实部就是旋转四元数，分量顺序为 x, y, z, w
        const Rotation rotation(dualQuaternion.x.re, dualQuaternion.y.re, dualQuaternion.z.re, dualQuaternion.w.re);
        // 平移按 t = 2·d·r* 还原：d 为对偶部、r* 为旋转共轭
        const DualQuat moveQuaternion = 2 * dualQuaternion.dual() * dualQuaternion.real().conj();
        return Placement(Vector3d(moveQuaternion.x.re, moveQuaternion.y.re, moveQuaternion.z.re), rotation);
    }

    Matrix4D Placement::toMatrix() const
    {
        Matrix4D matrix;
        m_rotation.getValue(matrix);
        // 第四列写入平移量，其余齐次分量已由旋转矩阵部分填好
        matrix[0][3] = m_position.x;
        matrix[1][3] = m_position.y;
        matrix[2][3] = m_position.z;
        return matrix;
    }

    void Placement::fromMatrix(const Matrix4D &matrix)
    {
        // 旋转部分由 Rotation 自行分解，这里只搬运平移列
        m_rotation.setValue(matrix);
        m_position.x = matrix[0][3];
        m_position.y = matrix[1][3];
        m_position.z = matrix[2][3];
    }

    DualQuat Placement::toDualQuaternion() const
    {
        // 平移向量以 w = 0 的纯四元数参与运算
        const DualQuat positionQuaternion(m_position.x, m_position.y, m_position.z, 0.0);
        DualQuat       rotationQuaternion;
        m_rotation.getValue(rotationQuaternion.x.re, rotationQuaternion.y.re, rotationQuaternion.z.re, rotationQuaternion.w.re);
        // 对偶部按 0.5·t·r 编码
        const DualQuat result(rotationQuaternion, 0.5 * positionQuaternion * rotationQuaternion);
        return result;
    }

    bool Placement::isIdentity() const
    {
        const Vector3d nullVector(0.0, 0.0, 0.0);
        return (m_position == nullVector) && m_rotation.isIdentity();
    }

    bool Placement::isIdentity(double tolerance) const
    {
        return isSame(Placement(), tolerance);
    }

    bool Placement::isSame(const Placement &other) const
    {
        // 位置用零容差比较，即要求精确相等
        return m_rotation.isSame(other.m_rotation) && m_position.IsEqual(other.m_position, 0);
    }

    bool Placement::isSame(const Placement &other, double tolerance) const
    {
        return m_rotation.isSame(other.m_rotation, tolerance) && m_position.IsEqual(other.m_position, tolerance);
    }

    void Placement::invert()
    {
        // 取逆：旋转取共轭，位置反向旋转后取负，即 T(-R⁻¹·p)·R⁻¹
        m_rotation = m_rotation.inverse();
        m_rotation.multVec(m_position, m_position);
        m_position = -m_position;
    }

    Placement Placement::inverse() const
    {
        Placement result(*this);
        result.invert();
        return result;
    }

    void Placement::move(const Vector3d &moveVector)
    {
        m_position += moveVector;
    }

    bool Placement::operator==(const Placement &other) const
    {
        return (m_position == other.m_position) && (m_rotation == other.m_rotation);
    }

    bool Placement::operator!=(const Placement &other) const
    {
        return !(*this == other);
    }

    Placement &Placement::operator*=(const Placement &other)
    {
        return multRight(other);
    }

    Placement Placement::operator*(const Placement &other) const
    {
        Placement result(*this);
        result *= other;
        return result;
    }

    Placement Placement::pow(double t, bool shorten) const
    {
        // 走对偶四元数的 ScLERP：平移与旋转沿同一条螺旋路径同步插值
        return Placement::fromDualQuaternion(this->toDualQuaternion().pow(t, shorten));
    }

    Placement &Placement::multRight(const Placement &other)
    {
        // 右乘：other 先作用，其平移量要先经本体旋转变换，再与本体平移相加
        Vector3d temporary(other.m_position);
        m_rotation.multVec(temporary, temporary);
        m_position += temporary;
        m_rotation.multRight(other.m_rotation);
        return *this;
    }

    Placement &Placement::multLeft(const Placement &other)
    {
        // 左乘：other 后作用，本体平移先被 other 变换，旋转部分左乘
        other.multVec(m_position, m_position);
        m_rotation.multLeft(other.m_rotation);
        return *this;
    }

    void Placement::multVec(const Vector3d &source, Vector3d &destination) const
    {
        m_rotation.multVec(source, destination);
        destination += m_position;
    }

    void Placement::multVec(const Vector3f &source, Vector3f &destination) const
    {
        m_rotation.multVec(source, destination);
        // 单精度路径同样把位置降为单精度，避免混算引入额外的精度分支
        destination += toVector<float>(m_position);
    }

    Placement Placement::slerp(const Placement &p0, const Placement &p1, double t)
    {
        // 旋转走球面插值、位置走线性插值：两条轨迹各自独立，不构成螺旋运动
        const Rotation rotation = Rotation::slerp(p0.getRotation(), p1.getRotation(), t);
        const Vector3d position = p0.getPosition() * (1.0 - t) + p1.getPosition() * t;
        return Placement(position, rotation);
    }

    Placement Placement::sclerp(const Placement &p0, const Placement &p1, double t, bool shorten)
    {
        // 先把 p1 换算到 p0 的局部系，对相对位姿做螺旋插值，再变回全局系
        const Placement transformation = p0.inverse() * p1;
        return p0 * transformation.pow(t, shorten);
    }

    std::string Placement::toString() const
    {
        const Vector3d position = getPosition();
        const Rotation rotation = getRotation();

        Vector3d axis;
        double   angle{};
        // 用原始轴角：保持用户设定的轴长与角度，不做归一化改写
        rotation.getRawValue(axis, angle);

        return std::format("position ({:.1f}, {:.1f}, {:.1f}), axis ({:.1f}, {:.1f}, {:.1f}), angle {:.1f}\n", position.x, position.y, position.z, axis.x, axis.y, axis.z, angle);
    }
} // namespace ExpressionEngine::Base
