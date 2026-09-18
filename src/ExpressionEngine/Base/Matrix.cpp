#include <ExpressionEngine/Base/Matrix.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <format>
#include <numbers>
#include <string>
#include <system_error>

#include <ExpressionEngine/Base/Converter.h>
#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Base
{
    namespace
    {
        /**
         * @brief 对 4x4 矩阵做高斯-约当消元求逆
         * @details 部分选主元以抑制舍入误差；原地改写传入的两个缓冲区，只在成功时结果可信。
         * @param matrix 列主序（OpenGL 风格）的 4x4 源矩阵，成功时被就地改写为消元中间量
         * @param inverse 初值为单位阵，成功时被就地改写为逆矩阵
         * @return 消元过程始终能找到非零主元时返回 true，否则返回 false
         */
        bool gaussInvert(double *matrix, double *inverse)
        {
            std::array<int, 4> pivotUsed{};          // 0 = 该列仍可作主元，1 = 已作主元，2 = 重复占用（奇异）
            std::array<int, 4> pivotRowHistory{};    // 每步选中的主元行，用于最后还原列序
            std::array<int, 4> pivotColumnHistory{}; // 每步选中的主元列
            int                pivotRow    = 0;
            int                pivotColumn = 0;

            for (int step = 0; step < 4; ++step)
            {
                double largest = 0.0;
                // 在尚未使用的列里挑绝对值最大的元素作主元，等价于部分选主元的数值稳定化
                for (int row = 0; row < 4; ++row)
                {
                    if (pivotUsed[row] != 1)
                    {
                        for (int column = 0; column < 4; ++column)
                        {
                            if (pivotUsed[column] == 0)
                            {
                                const double magnitude = std::abs(matrix[4 * row + column]);
                                if (magnitude >= largest)
                                {
                                    largest     = magnitude;
                                    pivotRow    = row;
                                    pivotColumn = column;
                                }
                            } else if (pivotUsed[column] > 1)
                            {
                                // 同一列被选作主元两次：剩余子矩阵秩亏
                                return false;
                            }
                        }
                    }
                }
                if (largest == 0.0)
                {
                    // 剩余候选元素全为零，矩阵奇异
                    return false;
                }

                ++pivotUsed[pivotColumn];
                if (pivotRow != pivotColumn)
                {
                    // 行交换让主元落到对角线上，右端单位阵必须同步交换
                    for (int column = 0; column < 4; ++column)
                    {
                        std::swap(matrix[4 * pivotRow + column], matrix[4 * pivotColumn + column]);
                    }
                    for (int column = 0; column < 4; ++column)
                    {
                        std::swap(inverse[4 * pivotRow + column], inverse[4 * pivotColumn + column]);
                    }
                }
                pivotRowHistory[step]    = pivotRow;
                pivotColumnHistory[step] = pivotColumn;

                if (matrix[4 * pivotColumn + pivotColumn] == 0.0)
                {
                    // 主元为零：无法继续消元
                    return false;
                }

                // 主元归一化，右端同步相除；两步除法合并成一次乘倒数以减少误差累积
                const double pivotInverse             = 1.0 / matrix[4 * pivotColumn + pivotColumn];
                matrix[4 * pivotColumn + pivotColumn] = 1.0;
                for (int column = 0; column < 4; ++column)
                {
                    matrix[4 * pivotColumn + column] *= pivotInverse;
                    inverse[4 * pivotColumn + column] *= pivotInverse;
                }

                // 高斯-约当消元：把主元列上其余各行清零，右端同步做行变换
                for (int row = 0; row < 4; ++row)
                {
                    if (row == pivotColumn)
                    {
                        continue;
                    }
                    const double factor = matrix[4 * row + pivotColumn];
                    for (int column = 0; column < 4; ++column)
                    {
                        matrix[4 * row + column] -= matrix[4 * pivotColumn + column] * factor;
                        inverse[4 * row + column] -= inverse[4 * pivotColumn + column] * factor;
                    }
                    // 该位置理论上已为零，显式写 0 避免留下舍入残差
                    matrix[4 * row + pivotColumn] = 0.0;
                }
            }

            // 按记录还原列交换，使消元后的矩阵回到原始列顺序
            for (int step = 3; step >= 0; --step)
            {
                if (pivotRowHistory[step] != pivotColumnHistory[step])
                {
                    for (int row = 0; row < 4; ++row)
                    {
                        std::swap(matrix[4 * row + pivotRowHistory[step]], matrix[4 * row + pivotColumnHistory[step]]);
                    }
                }
            }

            return true;
        }
    } // namespace

    Matrix4D::Matrix4D() : m_matrix{{{1.0, 0.0, 0.0, 0.0}, {0.0, 1.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {0.0, 0.0, 0.0, 1.0}}}
    {
    }

    // clang-format off
Matrix4D::Matrix4D(float a11, float a12, float a13, float a14,
                   float a21, float a22, float a23, float a24,
                   float a31, float a32, float a33, float a34,
                   float a41, float a42, float a43, float a44)
    : m_matrix {{{a11, a12, a13, a14},
                 {a21, a22, a23, a24},
                 {a31, a32, a33, a34},
                 {a41, a42, a43, a44}}}
{}

Matrix4D::Matrix4D(double a11, double a12, double a13, double a14,
                   double a21, double a22, double a23, double a24,
                   double a31, double a32, double a33, double a34,
                   double a41, double a42, double a43, double a44)
    : m_matrix {{{a11, a12, a13, a14},
                 {a21, a22, a23, a24},
                 {a31, a32, a33, a34},
                 {a41, a42, a43, a44}}}
{}
    // clang-format on

    Matrix4D::Matrix4D(const Matrix4D &other) : Matrix4D()
    {
        (*this) = other;
    }

    Matrix4D::Matrix4D(const Vector3f &base, const Vector3f &direction, float angle) : Matrix4D()
    {
        rotLine(base, direction, angle);
    }

    Matrix4D::Matrix4D(const Vector3d &base, const Vector3d &direction, double angle) : Matrix4D()
    {
        rotLine(base, direction, angle);
    }

    void Matrix4D::setToUnity()
    {
        // 逐元素写入而非整体赋值，避免依赖临时对象构造
        m_matrix = {{{1.0, 0.0, 0.0, 0.0}, {0.0, 1.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {0.0, 0.0, 0.0, 1.0}}};
    }

    bool Matrix4D::isUnity() const
    {
        return isUnity(0.0);
    }

    bool Matrix4D::isUnity(double tolerance) const
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                if (row == column)
                {
                    // 对角元素应等于 1
                    if (std::abs(m_matrix[row][column] - 1.0) > tolerance)
                    {
                        return false;
                    }
                } else if (std::abs(m_matrix[row][column]) > tolerance)
                {
                    // 非对角元素应等于 0
                    return false;
                }
            }
        }

        return true;
    }

    void Matrix4D::nullify()
    {
        m_matrix = {{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}}};
    }

    bool Matrix4D::isNull() const
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                // 零矩阵是精确语义，不做容差比较
                if (m_matrix[row][column] != 0.0)
                {
                    return false;
                }
            }
        }

        return true;
    }

    double Matrix4D::determinant() const
    {
        // 2x2 子式展开：把 4x4 行列式写成 6 对 2x2 子式的组合，避免递归
        const double upperBlockMinor0 = m_matrix[0][0] * m_matrix[1][1] - m_matrix[0][1] * m_matrix[1][0];
        const double upperBlockMinor1 = m_matrix[0][0] * m_matrix[1][2] - m_matrix[0][2] * m_matrix[1][0];
        const double upperBlockMinor2 = m_matrix[0][0] * m_matrix[1][3] - m_matrix[0][3] * m_matrix[1][0];
        const double upperBlockMinor3 = m_matrix[0][1] * m_matrix[1][2] - m_matrix[0][2] * m_matrix[1][1];
        const double upperBlockMinor4 = m_matrix[0][1] * m_matrix[1][3] - m_matrix[0][3] * m_matrix[1][1];
        const double upperBlockMinor5 = m_matrix[0][2] * m_matrix[1][3] - m_matrix[0][3] * m_matrix[1][2];
        const double lowerBlockMinor0 = m_matrix[2][0] * m_matrix[3][1] - m_matrix[2][1] * m_matrix[3][0];
        const double lowerBlockMinor1 = m_matrix[2][0] * m_matrix[3][2] - m_matrix[2][2] * m_matrix[3][0];
        const double lowerBlockMinor2 = m_matrix[2][0] * m_matrix[3][3] - m_matrix[2][3] * m_matrix[3][0];
        const double lowerBlockMinor3 = m_matrix[2][1] * m_matrix[3][2] - m_matrix[2][2] * m_matrix[3][1];
        const double lowerBlockMinor4 = m_matrix[2][1] * m_matrix[3][3] - m_matrix[2][3] * m_matrix[3][1];
        const double lowerBlockMinor5 = m_matrix[2][2] * m_matrix[3][3] - m_matrix[2][3] * m_matrix[3][2];

        return upperBlockMinor0 * lowerBlockMinor5 - upperBlockMinor1 * lowerBlockMinor4 + upperBlockMinor2 * lowerBlockMinor3 + upperBlockMinor3 * lowerBlockMinor2 -
               upperBlockMinor4 * lowerBlockMinor1 + upperBlockMinor5 * lowerBlockMinor0;
    }

    double Matrix4D::determinant3() const
    {
        // 3x3 子矩阵行列式：三正三负的六项展开
        const double positiveTerm1 = m_matrix[0][0] * m_matrix[1][1] * m_matrix[2][2];
        const double positiveTerm2 = m_matrix[0][1] * m_matrix[1][2] * m_matrix[2][0];
        const double positiveTerm3 = m_matrix[1][0] * m_matrix[2][1] * m_matrix[0][2];
        const double negativeTerm1 = m_matrix[0][2] * m_matrix[1][1] * m_matrix[2][0];
        const double negativeTerm2 = m_matrix[1][0] * m_matrix[0][1] * m_matrix[2][2];
        const double negativeTerm3 = m_matrix[0][0] * m_matrix[2][1] * m_matrix[1][2];

        return (positiveTerm1 + positiveTerm2 + positiveTerm3) - (negativeTerm1 + negativeTerm2 + negativeTerm3);
    }

    void Matrix4D::move(const Vector3f &vector)
    {
        // 先升到双精度再累加，避免单精度平移量在内部 double 存储上产生截断误差
        move(convertTo<Vector3d>(vector));
    }

    void Matrix4D::move(const Vector3d &vector)
    {
        m_matrix[0][3] += vector.x;
        m_matrix[1][3] += vector.y;
        m_matrix[2][3] += vector.z;
    }

    void Matrix4D::scale(const Vector3f &vector)
    {
        scale(convertTo<Vector3d>(vector));
    }

    void Matrix4D::scale(const Vector3d &vector)
    {
        Matrix4D scaleMatrix;

        scaleMatrix.m_matrix[0][0] = vector.x;
        scaleMatrix.m_matrix[1][1] = vector.y;
        scaleMatrix.m_matrix[2][2] = vector.z;
        // 缩放左乘到当前矩阵上，与 FreeCAD 的运算次序一致
        (*this) = scaleMatrix * (*this);
    }

    void Matrix4D::rotX(double angle)
    {
        const double sinAngle = std::sin(angle);
        const double cosAngle = std::cos(angle);

        Matrix4D rotationMatrix;
        rotationMatrix.m_matrix[1][1] = cosAngle;
        rotationMatrix.m_matrix[2][2] = cosAngle;
        rotationMatrix.m_matrix[1][2] = -sinAngle;
        rotationMatrix.m_matrix[2][1] = sinAngle;

        (*this) = rotationMatrix * (*this);
    }

    void Matrix4D::rotY(double angle)
    {
        const double sinAngle = std::sin(angle);
        const double cosAngle = std::cos(angle);

        Matrix4D rotationMatrix;
        rotationMatrix.m_matrix[0][0] = cosAngle;
        rotationMatrix.m_matrix[2][2] = cosAngle;
        rotationMatrix.m_matrix[2][0] = -sinAngle;
        rotationMatrix.m_matrix[0][2] = sinAngle;

        (*this) = rotationMatrix * (*this);
    }

    void Matrix4D::rotZ(double angle)
    {
        const double sinAngle = std::sin(angle);
        const double cosAngle = std::cos(angle);

        Matrix4D rotationMatrix;
        rotationMatrix.m_matrix[0][0] = cosAngle;
        rotationMatrix.m_matrix[1][1] = cosAngle;
        rotationMatrix.m_matrix[0][1] = -sinAngle;
        rotationMatrix.m_matrix[1][0] = sinAngle;

        (*this) = rotationMatrix * (*this);
    }

    void Matrix4D::rotLine(const Vector3d &vector, double angle)
    {
        // 罗德里格公式：R = (1-cos)*a*a^T + cos*I + sin*[a]_x
        Matrix4D outerTerm;
        Matrix4D diagonalTerm;
        Matrix4D skewTerm;
        Matrix4D rotationMatrix;
        Vector3d rotationAxis(vector);

        // 分项矩阵先全部清零，只填非零元素
        outerTerm.nullify();
        diagonalTerm.nullify();
        skewTerm.nullify();

        // 轴必须归一化，否则旋转矩阵会被轴长额外缩放
        rotationAxis.Normalize();

        const double cosAngle = std::cos(angle);
        const double sinAngle = std::sin(angle);

        // (1-cos) * a * a^T：不含旋转、只含轴向的对称项
        outerTerm.m_matrix[0][0] = (1 - cosAngle) * rotationAxis.x * rotationAxis.x;
        outerTerm.m_matrix[0][1] = (1 - cosAngle) * rotationAxis.x * rotationAxis.y;
        outerTerm.m_matrix[0][2] = (1 - cosAngle) * rotationAxis.x * rotationAxis.z;
        outerTerm.m_matrix[1][0] = (1 - cosAngle) * rotationAxis.x * rotationAxis.y;
        outerTerm.m_matrix[1][1] = (1 - cosAngle) * rotationAxis.y * rotationAxis.y;
        outerTerm.m_matrix[1][2] = (1 - cosAngle) * rotationAxis.y * rotationAxis.z;
        outerTerm.m_matrix[2][0] = (1 - cosAngle) * rotationAxis.x * rotationAxis.z;
        outerTerm.m_matrix[2][1] = (1 - cosAngle) * rotationAxis.y * rotationAxis.z;
        outerTerm.m_matrix[2][2] = (1 - cosAngle) * rotationAxis.z * rotationAxis.z;

        // 余弦项：cos * I
        diagonalTerm.m_matrix[0][0] = cosAngle;
        diagonalTerm.m_matrix[1][1] = cosAngle;
        diagonalTerm.m_matrix[2][2] = cosAngle;

        // sin * [a]_x：轴的反对称矩阵
        skewTerm.m_matrix[0][1] = -sinAngle * rotationAxis.z;
        skewTerm.m_matrix[0][2] = sinAngle * rotationAxis.y;
        skewTerm.m_matrix[1][0] = sinAngle * rotationAxis.z;
        skewTerm.m_matrix[1][2] = -sinAngle * rotationAxis.x;
        skewTerm.m_matrix[2][0] = -sinAngle * rotationAxis.y;
        skewTerm.m_matrix[2][1] = sinAngle * rotationAxis.x;

        // 只叠加 3x3 部分，第 4 行/列保持单位阵形式
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                rotationMatrix.m_matrix[row][column] = outerTerm.m_matrix[row][column] + diagonalTerm.m_matrix[row][column] + skewTerm.m_matrix[row][column];
            }
        }

        (*this) = rotationMatrix * (*this);
    }

    void Matrix4D::rotLine(const Vector3f &vector, float angle)
    {
        const auto axis = convertTo<Vector3d>(vector);
        rotLine(axis, static_cast<double>(angle));
    }

    void Matrix4D::rotLine(const Vector3d &base, const Vector3d &direction, double angle)
    {
        Matrix4D rotationMatrix;
        rotationMatrix.rotLine(direction, angle);
        // 轴不过原点时先平移到原点、旋转、再平移回去
        transform(base, rotationMatrix);
    }

    void Matrix4D::rotLine(const Vector3f &base, const Vector3f &direction, float angle)
    {
        const auto basePoint = convertTo<Vector3d>(base);
        const auto axis      = convertTo<Vector3d>(direction);
        rotLine(basePoint, axis, static_cast<double>(angle));
    }

    bool Matrix4D::toAxisAngle(Vector3f &base, Vector3f &direction, float &angle, float &translation) const
    {
        auto basePoint         = convertTo<Vector3d>(base);
        auto axis              = convertTo<Vector3d>(direction);
        auto doubleAngle       = static_cast<double>(angle);
        auto doubleTranslation = static_cast<double>(translation);

        const bool success = toAxisAngle(basePoint, axis, doubleAngle, doubleTranslation);
        if (success)
        {
            // 只在反解成功时回写单精度输出，失败时保持调用方原值
            base        = convertTo<Vector3f>(basePoint);
            direction   = convertTo<Vector3f>(axis);
            angle       = static_cast<float>(doubleAngle);
            translation = static_cast<float>(doubleTranslation);
        }

        return success;
    }

    bool Matrix4D::toAxisAngle(Vector3d &base, Vector3d &direction, double &angle, double &translation) const
    {
        // 先验证 3x3 子矩阵是否正交：列长为 1 且相邻列正交
        for (int index = 0; index < 3; ++index)
        {
            // 列长必须为 1
            if (std::abs(m_matrix[0][index] * m_matrix[0][index] + m_matrix[1][index] * m_matrix[1][index] + m_matrix[2][index] * m_matrix[2][index] - 1.0) > 0.01)
            {
                return false;
            }
            // 与下一列的内积必须为 0
            if (std::abs(m_matrix[0][index] * m_matrix[0][(index + 1) % 3] + m_matrix[1][index] * m_matrix[1][(index + 1) % 3] +
                         m_matrix[2][index] * m_matrix[2][(index + 1) % 3]) > 0.01)
            {
                return false;
            }
        }

        // 3x3 子矩阵正交，按 R = I + sin(A)*P + (1-cos(A))*P^2 反解轴与角
        const double traceValue = m_matrix[0][0] + m_matrix[1][1] + m_matrix[2][2];
        const double cosAngle   = 0.5 * (traceValue - 1.0);
        angle                   = std::acos(cosAngle); // 落在 [0, pi]

        if (angle > 0.0)
        {
            if (angle < std::numbers::pi)
            {
                // 一般情形：R - R^T = 2*sin(A)*P，可直接读出轴向
                direction.x = (m_matrix[2][1] - m_matrix[1][2]);
                direction.y = (m_matrix[0][2] - m_matrix[2][0]);
                direction.z = (m_matrix[1][0] - m_matrix[0][1]);
                direction.Normalize();
            } else
            {
                // 转角为 pi 时 R - R^T 为零，必须从对角线元素反解轴向
                double halfInverse{};
                if (m_matrix[0][0] >= m_matrix[1][1])
                {
                    if (m_matrix[0][0] >= m_matrix[2][2])
                    {
                        // r00 是最大对角元，用它开方最稳定
                        direction.x = (0.5 * std::sqrt(m_matrix[0][0] - m_matrix[1][1] - m_matrix[2][2] + 1.0));
                        halfInverse = 0.5 / direction.x;
                        direction.y = (halfInverse * m_matrix[0][1]);
                        direction.z = (halfInverse * m_matrix[0][2]);
                    } else
                    {
                        // r22 是最大对角元
                        direction.z = (0.5 * std::sqrt(m_matrix[2][2] - m_matrix[0][0] - m_matrix[1][1] + 1.0));
                        halfInverse = 0.5 / direction.z;
                        direction.x = (halfInverse * m_matrix[0][2]);
                        direction.y = (halfInverse * m_matrix[1][2]);
                    }
                } else if (m_matrix[1][1] >= m_matrix[2][2])
                {
                    // r11 是最大对角元
                    direction.y = (0.5 * std::sqrt(m_matrix[1][1] - m_matrix[0][0] - m_matrix[2][2] + 1.0));
                    halfInverse = 0.5 / direction.y;
                    direction.x = (halfInverse * m_matrix[0][1]);
                    direction.z = (halfInverse * m_matrix[1][2]);
                } else
                {
                    // r22 是最大对角元
                    direction.z = (0.5 * std::sqrt(m_matrix[2][2] - m_matrix[0][0] - m_matrix[1][1] + 1.0));
                    halfInverse = 0.5 / direction.z;
                    direction.x = (halfInverse * m_matrix[0][2]);
                    direction.y = (halfInverse * m_matrix[1][2]);
                }
            }
        } else
        {
            // 转角为 0，矩阵是单位阵：轴任取，约定用 X 轴，基点取原点
            direction.x = 1.0;
            direction.y = 0.0;
            direction.z = 0.0;
            base.x      = 0.0;
            base.y      = 0.0;
            base.z      = 0.0;
        }

        // 沿轴方向的平移量
        translation = (m_matrix[0][3] * direction.x + m_matrix[1][3] * direction.y + m_matrix[2][3] * direction.z);
        Vector3d translationPoint(m_matrix[0][3], m_matrix[1][3], m_matrix[2][3]);
        translationPoint = translationPoint - translation * direction;

        // 轴上的基点：由（1+tr(R)）/（2*sin A）与反对称部分组合得到
        if (angle > 0.0)
        {
            const double factor = 0.5 * (1.0 + traceValue) / std::sin(angle);
            base.x              = (0.5 * (translationPoint.x + factor * (direction.y * translationPoint.z - direction.z * translationPoint.y)));
            base.y              = (0.5 * (translationPoint.y + factor * (direction.z * translationPoint.x - direction.x * translationPoint.z)));
            base.z              = (0.5 * (translationPoint.z + factor * (direction.x * translationPoint.y - direction.y * translationPoint.x)));
        }

        return true;
    }

    void Matrix4D::transform(const Vector3f &vector, const Matrix4D &matrix)
    {
        move(-convertTo<Vector3d>(vector));
        (*this) = matrix * (*this);
        move(convertTo<Vector3d>(vector));
    }

    void Matrix4D::transform(const Vector3d &vector, const Matrix4D &matrix)
    {
        // 变换是围绕参考点施加的：先移到原点、应用变换、再移回去
        move(-vector);
        (*this) = matrix * (*this);
        move(vector);
    }

    void Matrix4D::inverse()
    {
        Matrix4D inverseTranslation;
        Matrix4D inverseRotation;

        // 平移部分取反
        for (int row = 0; row < 3; ++row)
        {
            inverseTranslation.m_matrix[row][3] = -m_matrix[row][3];
        }

        // 旋转部分转置（仅当 3x3 子矩阵为正交矩阵时才是逆）
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                inverseRotation.m_matrix[row][column] = m_matrix[column][row];
            }
        }

        // 逆的乘法次序要反转：inv(M_trl * M_rot) = inv(M_rot) * inv(M_trl)
        (*this) = inverseRotation * inverseTranslation;
    }

    void Matrix4D::inverseOrthogonal()
    {
        Vector3d translation(m_matrix[0][3], m_matrix[1][3], m_matrix[2][3]);
        transpose();
        // 用转置后的矩阵把平移分量变换到新坐标系，再取反
        multVec(translation, translation);
        m_matrix[0][3] = -translation.x;
        m_matrix[3][0] = 0.0;
        m_matrix[1][3] = -translation.y;
        m_matrix[3][1] = 0.0;
        m_matrix[2][3] = -translation.z;
        m_matrix[3][2] = 0.0;
    }

    void Matrix4D::inverseGauss()
    {
        // 在列主序缓冲上做消元：算法按 OpenGL 布局索引，因此先导出再写回
        std::array<double, 16> source{};
        std::array<double, 16> result{
                1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
        };
        getGLMatrix(source.data());

        if (!gaussInvert(source.data(), result.data()))
        {
            // 奇异矩阵没有逆：与其返回垃圾结果，不如让调用方先处理可逆性问题
            throw ValueError("矩阵奇异（高斯消元找不到非零主元），无法求逆；请先用 determinant() 确认矩阵可逆，"
                             "或改用 inverseOrthogonal() 处理只含旋转与平移的矩阵");
        }

        // 秩亏矩阵未必会在消元中撞上零主元，因此用 M * M^-1 近似单位阵来复核
        Matrix4D candidate;
        candidate.setGLMatrix(result.data());
        double largestElement = 0.0;
        for (const double value: source)
        {
            largestElement = std::max(largestElement, std::abs(value));
        }
        // 容差随矩阵元素量级放大，避免大数矩阵被误判为奇异
        const double tolerance = 1e-9 * std::max(1.0, largestElement);
        if (!((*this) * candidate).isUnity(tolerance))
        {
            throw ValueError("矩阵接近奇异，求逆结果不可信；请先检查 determinant() 是否远离 0，"
                             "或对坐标做缩放/归一化后再求逆");
        }

        setGLMatrix(result.data());
    }

    void Matrix4D::getMatrix(double values[16]) const
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                values[4 * row + column] = m_matrix[row][column];
            }
        }
    }

    void Matrix4D::setMatrix(const double values[16])
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                m_matrix[row][column] = values[4 * row + column];
            }
        }
    }

    void Matrix4D::getGLMatrix(double values[16]) const
    {
        // OpenGL 采用列主序，索引按「行 + 4 * 列」排布
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                values[row + 4 * column] = m_matrix[row][column];
            }
        }
    }

    void Matrix4D::setGLMatrix(const double values[16])
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                m_matrix[row][column] = values[row + 4 * column];
            }
        }
    }

    unsigned long Matrix4D::getMemSpace() const
    {
        return sizeof(Matrix4D);
    }

    void Matrix4D::Print() const
    {
        // 只用于调试：直接写标准输出，避免引入 iostream 的开销与全局状态
        for (int row = 0; row < 4; ++row)
        {
            const std::string line = std::format("{:9.3f} {:9.3f} {:9.3f} {:9.3f}\n", m_matrix[row][0], m_matrix[row][1], m_matrix[row][2], m_matrix[row][3]);
            std::fputs(line.c_str(), stdout);
        }
    }

    void Matrix4D::transpose()
    {
        // 只交换上三角与下三角，对角线不动
        std::swap(m_matrix[0][1], m_matrix[1][0]);
        std::swap(m_matrix[0][2], m_matrix[2][0]);
        std::swap(m_matrix[0][3], m_matrix[3][0]);
        std::swap(m_matrix[1][2], m_matrix[2][1]);
        std::swap(m_matrix[1][3], m_matrix[3][1]);
        std::swap(m_matrix[2][3], m_matrix[3][2]);
    }

    std::string Matrix4D::toString() const
    {
        std::string text;
        text.reserve(16 * 25);
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                // {} 使用最短往返表示，保证文本能逐位还原原始数值
                text += std::format("{} ", m_matrix[row][column]);
            }
        }

        return text;
    }

    void Matrix4D::fromString(const std::string &text)
    {
        const char       *cursor = text.data();
        const char *const end    = text.data() + text.size();

        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                // 跳过空白：允许空格、制表符与换行混用
                while (cursor != end && std::isspace(static_cast<unsigned char>(*cursor)) != 0)
                {
                    ++cursor;
                }

                double value             = 0.0;
                const auto [next, error] = std::from_chars(cursor, end, value);
                if (error != std::errc{})
                {
                    // 缺分量或含非法字符时直接报错，不静默保留旧值
                    throw ValueError(std::format("矩阵文本解析失败：第 {} 行第 {} 列的位置不是合法浮点数；"
                                                 "请传入由 toString() 生成的 16 个空格分隔数值",
                                                 row, column));
                }
                m_matrix[row][column] = value;
                cursor                = next;
            }
        }
    }

    std::string Matrix4D::analyse() const
    {
        // 判定 3x3 部分正交性的经验阈值
        constexpr double orthogonalityTolerance = 1.0e-06;
        const bool       hasTranslation         = (m_matrix[0][3] != 0.0 || m_matrix[1][3] != 0.0 || m_matrix[2][3] != 0.0);
        const Matrix4D   unityMatrix;

        std::string text;
        if (*this == unityMatrix)
        {
            text = "Unity Matrix";
        } else
        {
            // 第 4 行不是 (0,0,0,1) 说明不是仿射变换
            if (m_matrix[3][0] != 0.0 || m_matrix[3][1] != 0.0 || m_matrix[3][2] != 0.0 || m_matrix[3][3] != 1.0)
            {
                text = "Projection";
            } else if (m_matrix[0][1] == 0.0 && m_matrix[0][2] == 0.0 && m_matrix[1][0] == 0.0 && m_matrix[1][2] == 0.0 && m_matrix[2][0] == 0.0 && m_matrix[2][1] == 0.0)
            {
                // 非对角全零：纯缩放
                text = std::format("Scale [{}, {}, {}]", m_matrix[0][0], m_matrix[1][1], m_matrix[2][2]);
            } else
            {
                Matrix4D linearPart;
                linearPart[0][0] = m_matrix[0][0];
                linearPart[0][1] = m_matrix[0][1];
                linearPart[0][2] = m_matrix[0][2];
                linearPart[1][0] = m_matrix[1][0];
                linearPart[1][1] = m_matrix[1][1];
                linearPart[1][2] = m_matrix[1][2];
                linearPart[2][0] = m_matrix[2][0];
                linearPart[2][1] = m_matrix[2][1];
                linearPart[2][2] = m_matrix[2][2];

                // Gram 矩阵非对角项接近零即说明 3x3 部分是正交的
                Matrix4D gramMatrix = linearPart;
                gramMatrix.transpose();
                gramMatrix = gramMatrix * linearPart;

                bool orthogonal = true;
                for (unsigned int row = 0; row < 4 && orthogonal; ++row)
                {
                    for (unsigned int column = 0; column < 4 && orthogonal; ++column)
                    {
                        if (row != column && std::abs(gramMatrix[row][column]) > orthogonalityTolerance)
                        {
                            orthogonal = false;
                        }
                    }
                }

                const double subDeterminant = linearPart.determinant();
                if (orthogonal)
                {
                    if (std::abs(subDeterminant - 1.0) < orthogonalityTolerance)
                    {
                        text = "Rotation Matrix";
                    } else if (std::abs(subDeterminant + 1.0) < orthogonalityTolerance)
                    {
                        text = "Rotinversion Matrix";
                    } else
                    {
                        // 正交但行列式不是 ±1：带旋转的缩放，行列式为负说明含镜像
                        text = std::format("Scale and Rotate {}[ {}, {}, {}]", subDeterminant < 0.0 ? "and Invert " : "", std::sqrt(gramMatrix[0][0]), std::sqrt(gramMatrix[1][1]),
                                           std::sqrt(gramMatrix[2][2]));
                    }
                } else
                {
                    text = std::format("Affine with det= {}", subDeterminant);
                }
            }

            if (hasTranslation)
            {
                text += " with Translation";
            }
        }

        return text;
    }

    Matrix4D &Matrix4D::Outer(const Vector3f &firstVector, const Vector3f &secondVector)
    {
        setToUnity();
        Outer(convertTo<Vector3d>(firstVector), convertTo<Vector3d>(secondVector));
        return *this;
    }

    Matrix4D &Matrix4D::Outer(const Vector3d &firstVector, const Vector3d &secondVector)
    {
        // 并矢积的左上 3x3 为外积，其余位置保持单位阵
        setToUnity();

        m_matrix[0][0] = firstVector.x * secondVector.x;
        m_matrix[0][1] = firstVector.x * secondVector.y;
        m_matrix[0][2] = firstVector.x * secondVector.z;

        m_matrix[1][0] = firstVector.y * secondVector.x;
        m_matrix[1][1] = firstVector.y * secondVector.y;
        m_matrix[1][2] = firstVector.y * secondVector.z;

        m_matrix[2][0] = firstVector.z * secondVector.x;
        m_matrix[2][1] = firstVector.z * secondVector.y;
        m_matrix[2][2] = firstVector.z * secondVector.z;

        return *this;
    }

    Matrix4D &Matrix4D::Hat(const Vector3f &vector)
    {
        setToUnity();
        Hat(convertTo<Vector3d>(vector));
        return *this;
    }

    Matrix4D &Matrix4D::Hat(const Vector3d &vector)
    {
        // 反对称矩阵：左上 3x3 满足 [a]x * b = a x b，对角线与第 4 行/列保持单位阵
        setToUnity();

        m_matrix[0][0] = 0.0;
        m_matrix[0][1] = -vector.z;
        m_matrix[0][2] = vector.y;

        m_matrix[1][0] = vector.z;
        m_matrix[1][1] = 0.0;
        m_matrix[1][2] = -vector.x;

        m_matrix[2][0] = -vector.y;
        m_matrix[2][1] = vector.x;
        m_matrix[2][2] = 0.0;

        return *this;
    }

    ScaleType Matrix4D::hasScale(double tolerance) const
    {
        // 未指定容差时启用默认判定容差
        constexpr double defaultTolerance = 1e-9;
        if (tolerance == 0.0)
        {
            tolerance = defaultTolerance;
        }

        // 比较两个绝对值是否成比例：都以较大者为分母，避免放大相对误差
        auto closeAbs = [&](double first, double second)
        {
            const double firstAbs  = std::abs(first);
            const double secondAbs = std::abs(second);
            if (secondAbs > firstAbs)
            {
                return (secondAbs - firstAbs) / secondAbs <= tolerance;
            }
            if (firstAbs > secondAbs)
            {
                return (firstAbs - secondAbs) / firstAbs <= tolerance;
            }
            return true;
        };

        // 列向量的平方长度及其几何平均，用于判定从右侧作用的缩放
        const double columnSquaredX = getCol(0).squaredLength();
        const double columnSquaredY = getCol(1).squaredLength();
        const double columnSquaredZ = getCol(2).squaredLength();
        const double columnProduct  = std::sqrt(columnSquaredX * columnSquaredY * columnSquaredZ);

        // 行向量的平方长度及其几何平均，用于判定从左侧作用的缩放
        const double rowSquaredX = getRow(0).squaredLength();
        const double rowSquaredY = getRow(1).squaredLength();
        const double rowSquaredZ = getRow(2).squaredLength();
        const double rowProduct  = std::sqrt(rowSquaredX * rowSquaredY * rowSquaredZ);

        const double determinant = determinant3();

        // 几何平均与行列式都对不上：投影、剪切等
        if (!closeAbs(columnProduct, determinant) && !closeAbs(rowProduct, determinant))
        {
            return ScaleType::Other;
        }

        if (closeAbs(rowProduct, determinant) && (!closeAbs(rowSquaredX, rowSquaredY) || !closeAbs(rowSquaredY, rowSquaredZ)))
        {
            return ScaleType::NonUniformLeft;
        }

        if (closeAbs(columnProduct, determinant) && (!closeAbs(columnSquaredX, columnSquaredY) || !closeAbs(columnSquaredY, columnSquaredZ)))
        {
            return ScaleType::NonUniformRight;
        }

        // 行列式偏离 1 说明三轴同倍率缩放
        if (std::abs(determinant - 1.0) > tolerance)
        {
            return ScaleType::Uniform;
        }

        return ScaleType::NoScaling;
    }

    std::array<Matrix4D, 4> Matrix4D::decompose() const
    {
        // 分解目标：matrix = move * rotation * scale * shear，返回值顺序为剪
        // shear、scale、rotation、move
        Matrix4D moveMatrix;
        Matrix4D rotationMatrix;
        Matrix4D scaleMatrix;
        Matrix4D residualMatrix(*this);

        // 抽出平移分量
        moveMatrix.move(residualMatrix.getCol(3));
        residualMatrix.setCol(3, Vector3d());

        // 在剩余矩阵的列里找一组互相垂直的方向作为旋转基
        int                     primaryDirection = -1;
        std::array<Vector3d, 3> directions       = {Vector3d(1.0, 0.0, 0.0), Vector3d(0.0, 1.0, 0.0), Vector3d(0.0, 0.0, 1.0)};
        for (int index = 0; index < 3; ++index)
        {
            if (residualMatrix.getCol(index).IsNull())
            {
                // 该列退化为零向量，不能作为基向量
                continue;
            }
            if (primaryDirection < 0)
            {
                directions[index] = residualMatrix.getCol(index);
                directions[index].Normalize();
                primaryDirection = index;
                continue;
            }

            Vector3d crossProduct = directions[primaryDirection].Cross(residualMatrix.getCol(index));
            if (crossProduct.IsNull())
            {
                // 与主方向平行，换下一列再试
                continue;
            }
            crossProduct.Normalize();
            const int lastDirection = 3 - index - primaryDirection;
            // 依据两列的前后次序确定第三列的正负，保持右手系
            if (index - primaryDirection == 1)
            {
                directions[lastDirection] = crossProduct;
                directions[index]         = crossProduct.Cross(directions[primaryDirection]);
            } else
            {
                directions[lastDirection] = -crossProduct;
                directions[index]         = directions[primaryDirection].Cross(-crossProduct);
            }
            primaryDirection = -2; // 三个方向已全部确定
            break;
        }
        if (primaryDirection >= 0)
        {
            // 只有一个有效方向：用坐标轴叉积补齐另外两个方向
            Vector3d crossProduct = directions[primaryDirection].Cross(Vector3d(0.0, 0.0, 1.0));
            if (crossProduct.IsNull())
            {
                crossProduct = directions[primaryDirection].Cross(Vector3d(0.0, 1.0, 0.0));
            }
            directions[(primaryDirection + 1) % 3] = crossProduct;
            directions[(primaryDirection + 2) % 3] = directions[primaryDirection].Cross(crossProduct);
        }
        rotationMatrix.setCol(0, directions[0]);
        rotationMatrix.setCol(1, directions[1]);
        rotationMatrix.setCol(2, directions[2]);
        rotationMatrix.inverseGauss();
        residualMatrix = rotationMatrix * residualMatrix;

        // 行列式为负说明含镜像：绕 Z 轴转 180 度把符号并入旋转，保证三个缩放因子同号
        if (residualMatrix.determinant() < 0)
        {
            rotationMatrix.rotZ(std::numbers::pi);
            residualMatrix.rotZ(std::numbers::pi);
        }
        rotationMatrix.inverseGauss();

        // 抽出对角方向上的缩放
        const double xScale        = residualMatrix.m_matrix[0][0];
        const double yScale        = residualMatrix.m_matrix[1][1];
        const double zScale        = residualMatrix.m_matrix[2][2];
        scaleMatrix.m_matrix[0][0] = xScale;
        scaleMatrix.m_matrix[1][1] = yScale;
        scaleMatrix.m_matrix[2][2] = zScale;

        // 除回缩放后剩下的是剪切；缩放因子为零时保持原值，避免除零
        residualMatrix.scale(xScale != 0 ? 1.0 / xScale : 1.0, yScale != 0 ? 1.0 / yScale : 1.0, zScale != 0 ? 1.0 / zScale : 1.0);
        // 剪切矩阵的对角线固定为 1
        residualMatrix.setDiagonal(Vector3d(1.0, 1.0, 1.0));

        // 清理 1e-15 量级的舍入残差，避免分解结果里出现伪影
        for (int row = 0; row < 3; ++row)
        {
            if (std::abs(scaleMatrix.m_matrix[row][row]) < 1e-15)
            {
                scaleMatrix.m_matrix[row][row] = 0.0;
            }
            for (int column = 0; column < 3; ++column)
            {
                if (std::abs(residualMatrix.m_matrix[row][column]) < 1e-15)
                {
                    residualMatrix.m_matrix[row][column] = 0.0;
                }
                if (std::abs(rotationMatrix.m_matrix[row][column]) < 1e-15)
                {
                    rotationMatrix.m_matrix[row][column] = 0.0;
                }
            }
        }

        return std::array<Matrix4D, 4>{residualMatrix, scaleMatrix, rotationMatrix, moveMatrix};
    }
} // namespace ExpressionEngine::Base
