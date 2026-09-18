#include <ExpressionEngine/Base/Rotation.h>

#include <array>
#include <cassert>
#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <string_view>
#include <utility>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Precision.h>

namespace ExpressionEngine::Base {
namespace {
/// 角度制与弧度制的换算系数：π/180
constexpr double DegreesToRadiansFactor = std::numbers::pi / 180.0;

/**
 * @brief 角度制转弧度制
 * @param degrees 角度值
 * @return 弧度值
 */
constexpr double radiansFromDegrees(double degrees) {
    return degrees * DegreesToRadiansFactor;
}

/**
 * @brief 弧度制转角度制
 * @param radians 弧度值
 * @return 角度值
 */
constexpr double degreesFromRadians(double radians) {
    return radians / DegreesToRadiansFactor;
}

/**
 * @brief 按 ASCII 规则做大小写不敏感的字符串比较
 * @details 只折 A-Z 的大小写，不做 locale 相关的映射，使解析结果与机器语言环境无关。
 * @param left 左操作数
 * @param right 右操作数
 * @return true 两者除大小写外逐字节相同
 */
bool isAsciiCaseInsensitiveEqual(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        unsigned char leftChar = static_cast<unsigned char>(left[index]);
        unsigned char rightChar = static_cast<unsigned char>(right[index]);
        // 仅把 ASCII 大写字母折成小写，其余字节（含非 ASCII）按原值比较
        if (leftChar >= 'A' && leftChar <= 'Z') {
            leftChar = static_cast<unsigned char>(leftChar + ('a' - 'A'));
        }
        if (rightChar >= 'A' && rightChar <= 'Z') {
            rightChar = static_cast<unsigned char>(rightChar + ('a' - 'A'));
        }
        if (leftChar != rightChar) {
            return false;
        }
    }
    return true;
}

/// 欧拉序列对应的轴三元组与旋转方向约定，轴序号 1..3 依次表示 X/Y/Z
struct EulerSequenceParameters {
    int m_firstAxis;           ///< 第一个旋转轴
    int m_secondAxis;          ///< 第二个旋转轴
    int m_thirdAxis;           ///< 第三个旋转轴
    bool m_hasOddPermutation;  ///< 前两个轴是否为奇排列（如 XZ 相对于 XYZ）
    bool m_repeatsFirstAxis;   ///< 第三轴是否与第一轴相同（真欧拉角）
    bool m_usesFixedAxes;      ///< 是否绕固定轴旋转（外旋）

    /**
     * @brief 由第一轴与排列性质推导出完整轴三元组
     * @param firstAxisIndex 第一轴，1..3
     * @param hasOddPermutation 前两轴是否奇排列
     * @param repeatsFirstAxis 第三轴是否与第一轴相同
     * @param usesFixedAxes 是否绕固定轴旋转
     */
    EulerSequenceParameters(int firstAxisIndex,
                            bool hasOddPermutation,
                            bool repeatsFirstAxis,
                            bool usesFixedAxes)
        : m_firstAxis(firstAxisIndex)
          // 轴序号按模 3 步进，奇排列时先跨一个轴
          ,
          m_secondAxis(1 + (firstAxisIndex + (hasOddPermutation ? 1 : 0)) % 3),
          m_thirdAxis(1 + (firstAxisIndex + (hasOddPermutation ? 0 : 1)) % 3),
          m_hasOddPermutation(hasOddPermutation), m_repeatsFirstAxis(repeatsFirstAxis),
          m_usesFixedAxes(usesFixedAxes) {
    }
};

/**
 * @brief 把欧拉序列翻译成通用转换所需的参数
 * @details 转换算法来自 Ken Shoemake（Graphics Gems IV），与 OCCT gp_Quaternion 的用法一致：
 *          内旋角度转换复用外旋代码，通过反转轴序与交换首末角实现。
 * @param sequence 欧拉角序列
 * @return 该序列对应的轴三元组参数
 */
EulerSequenceParameters translateEulerSequence(const Rotation::EulerSequence sequence) {
    constexpr bool Even = false;
    constexpr bool Odd = true;
    constexpr bool TwoAxes = true;
    constexpr bool ThreeAxes = false;
    constexpr bool Extrinsic = true;
    constexpr bool Intrinsic = false;

    switch (sequence) {
    case Rotation::Extrinsic_XYZ:
        return {1, Even, ThreeAxes, Extrinsic};
    case Rotation::Extrinsic_XZY:
        return {1, Odd, ThreeAxes, Extrinsic};
    case Rotation::Extrinsic_YZX:
        return {2, Even, ThreeAxes, Extrinsic};
    case Rotation::Extrinsic_YXZ:
        return {2, Odd, ThreeAxes, Extrinsic};
    case Rotation::Extrinsic_ZXY:
        return {3, Even, ThreeAxes, Extrinsic};
    case Rotation::Extrinsic_ZYX:
        return {3, Odd, ThreeAxes, Extrinsic};

    // 内旋按「同角度、轴序相反」的等价外旋处理，首末角在转换过程中互换
    case Rotation::Intrinsic_XYZ:
        return {3, Odd, ThreeAxes, Intrinsic};
    case Rotation::Intrinsic_XZY:
        return {2, Even, ThreeAxes, Intrinsic};
    case Rotation::Intrinsic_YZX:
        return {1, Odd, ThreeAxes, Intrinsic};
    case Rotation::Intrinsic_YXZ:
        return {3, Even, ThreeAxes, Intrinsic};
    case Rotation::Intrinsic_ZXY:
        return {2, Odd, ThreeAxes, Intrinsic};
    case Rotation::Intrinsic_ZYX:
        return {1, Even, ThreeAxes, Intrinsic};

    // 真欧拉角对轴序对称，内旋与外旋共用同一轴三元组
    case Rotation::Extrinsic_XYX:
        return {1, Even, TwoAxes, Extrinsic};
    case Rotation::Extrinsic_XZX:
        return {1, Odd, TwoAxes, Extrinsic};
    case Rotation::Extrinsic_YZY:
        return {2, Even, TwoAxes, Extrinsic};
    case Rotation::Extrinsic_YXY:
        return {2, Odd, TwoAxes, Extrinsic};
    case Rotation::Extrinsic_ZXZ:
        return {3, Even, TwoAxes, Extrinsic};
    case Rotation::Extrinsic_ZYZ:
        return {3, Odd, TwoAxes, Extrinsic};

    case Rotation::Intrinsic_XYX:
        return {1, Even, TwoAxes, Intrinsic};
    case Rotation::Intrinsic_XZX:
        return {1, Odd, TwoAxes, Intrinsic};
    case Rotation::Intrinsic_YZY:
        return {2, Even, TwoAxes, Intrinsic};
    case Rotation::Intrinsic_YXY:
        return {2, Odd, TwoAxes, Intrinsic};
    case Rotation::Intrinsic_ZXZ:
        return {3, Even, TwoAxes, Intrinsic};
    case Rotation::Intrinsic_ZYZ:
        return {3, Odd, TwoAxes, Intrinsic};

    // 两个别名序列落在同一实现上；其余取值（Invalid 等）由调用方在入口处拦下
    case Rotation::EulerAngles:
        return {3, Even, TwoAxes, Intrinsic};
    case Rotation::YawPitchRoll:
        return {1, Even, ThreeAxes, Intrinsic};
    default:
        return {3, Even, TwoAxes, Intrinsic};
    }
}

/// 欧拉序列名字表，下标与枚举值相差 1
constexpr std::array<const char*, 26> EulerSequenceNames{
    "Euler", "YawPitchRoll", "XYZ",  "XZY",  "YZX",  "YXZ",  "ZXY",  "ZYX",  "IXYZ",
    "IXZY",  "IYZX",         "IYXZ", "IZXY", "IZYX", "XYX",  "XZX",  "YZY",  "YXY",
    "ZYZ",   "ZXZ",          "IXYX", "IXZX", "IYZY", "IYXY", "IZXZ", "IZYZ",
};

// 名字表必须与枚举一一对应，新增序列时漏改会在这里编不过
static_assert(EulerSequenceNames.size() ==
                  static_cast<std::size_t>(Rotation::EulerSequenceLast) - 1,
              "欧拉序列名字表与 EulerSequence 枚举不同步");

/// 判定「无旋转」的四元数向量部分长度阈值
constexpr double NoRotationThreshold = 1e-12;
}  // namespace

Rotation::Rotation() : m_quaternion{0.0, 0.0, 0.0, 1.0}, m_axis{0.0, 0.0, 1.0}, m_angle{0.0} {
}

Rotation::Rotation(const Vector3d& axis, const double angle) : Rotation() {
    // 先把轴定为 Z：传入零向量时 setValue() 会沿用这个保底方向，避免出现 NaN
    m_axis.Set(0.0, 0.0, 1.0);
    this->setValue(axis, angle);
}

Rotation::Rotation(const Matrix4D& matrix) : Rotation() {
    this->setValue(matrix);
}

Rotation::Rotation(const double q[4]) : Rotation() {
    this->setValue(q);
}

Rotation::Rotation(double q0, double q1, double q2, double q3) : Rotation() {
    this->setValue(q0, q1, q2, q3);
}

Rotation::Rotation(const Vector3d& rotateFrom, const Vector3d& rotateTo) : Rotation() {
    this->setValue(rotateFrom, rotateTo);
}

Rotation Rotation::fromNormalVector(const Vector3d& normal) {
    // 把 Z 轴转到给定法向上，即得该法向对应的姿态
    return Rotation(Vector3d(0.0, 0.0, 1.0), normal);
}

Rotation Rotation::fromEulerAngles(EulerSequence order, double alpha, double beta, double gamma) {
    Rotation rotation;
    rotation.setEulerAngles(order, alpha, beta, gamma);
    return rotation;
}

const double* Rotation::getValue() const {
    return m_quaternion.data();
}

void Rotation::getValue(double& q0, double& q1, double& q2, double& q3) const {
    q0 = m_quaternion[0];
    q1 = m_quaternion[1];
    q2 = m_quaternion[2];
    q3 = m_quaternion[3];
}

void Rotation::evaluateVector() {
    // 四元数可写成 q = (sin(θ/2)·n, cos(θ/2))，|w| 恰好为 1 表示没有旋转
    // 注意 w 不允许等于 ±1，那种情形下轴角无定义，走下面的兜底分支
    if ((m_quaternion[3] > -1.0) && (m_quaternion[3] < 1.0)) {
        double rfAngle = std::acos(m_quaternion[3]) * 2.0;
        double scale = std::sin(rfAngle / 2.0);
        // 轴长可能来自用户传入的非单位轴向，先取回其长度，零长按 1 处理以免除零
        double length = m_axis.Length();
        if (length < Vector3d::epsilon()) {
            length = 1.0;
        }
        m_axis.x = m_quaternion[0] * length / scale;
        m_axis.y = m_quaternion[1] * length / scale;
        m_axis.z = m_quaternion[2] * length / scale;

        m_angle = rfAngle;
    } else {
        // |w| == 1：旋转角为 0，轴角退回 Z 轴 + 0 度
        m_axis.Set(0.0, 0.0, 1.0);
        m_angle = 0.0;
    }
}

void Rotation::setValue(double q0, double q1, double q2, double q3) {
    m_quaternion[0] = q0;
    m_quaternion[1] = q1;
    m_quaternion[2] = q2;
    m_quaternion[3] = q3;
    this->normalize();
    this->evaluateVector();
}

void Rotation::getValue(Vector3d& axis, double& angle) const {
    angle = m_angle;
    axis.x = m_axis.x;
    axis.y = m_axis.y;
    axis.z = m_axis.z;
    // setValue(axis, angle) 可能保存的是未归一化的轴，对外一律给出单位轴；
    // 零四元数没有可用的轴，此时保留零向量返回，由调用方用 isNull() 判定
    if (!axis.IsNull()) {
        axis.Normalize();
    }
}

void Rotation::getRawValue(Vector3d& axis, double& angle) const {
    angle = m_angle;
    axis.x = m_axis.x;
    axis.y = m_axis.y;
    axis.z = m_axis.z;
}

void Rotation::getValue(Matrix4D& matrix) const {
    // 先把四元数归一化，容忍调用方通过 operator[] 写出的非单位四元数
    const double length =
        std::sqrt(m_quaternion[0] * m_quaternion[0] + m_quaternion[1] * m_quaternion[1] +
                  m_quaternion[2] * m_quaternion[2] + m_quaternion[3] * m_quaternion[3]);
    const double x = m_quaternion[0] / length;
    const double y = m_quaternion[1] / length;
    const double z = m_quaternion[2] / length;
    const double w = m_quaternion[3] / length;

    // 按 |q| = 1 展开旋转矩阵，第四行与第四列补成齐次形式
    matrix[0][0] = 1.0 - 2.0 * (y * y + z * z);
    matrix[0][1] = 2.0 * (x * y - z * w);
    matrix[0][2] = 2.0 * (x * z + y * w);
    matrix[0][3] = 0.0;

    matrix[1][0] = 2.0 * (x * y + z * w);
    matrix[1][1] = 1.0 - 2.0 * (x * x + z * z);
    matrix[1][2] = 2.0 * (y * z - x * w);
    matrix[1][3] = 0.0;

    matrix[2][0] = 2.0 * (x * z - y * w);
    matrix[2][1] = 2.0 * (y * z + x * w);
    matrix[2][2] = 1.0 - 2.0 * (x * x + y * y);
    matrix[2][3] = 0.0;

    matrix[3][0] = 0.0;
    matrix[3][1] = 0.0;
    matrix[3][2] = 0.0;
    matrix[3][3] = 1.0;
}

void Rotation::setValue(const double q[4]) {
    m_quaternion[0] = q[0];
    m_quaternion[1] = q[1];
    m_quaternion[2] = q[2];
    m_quaternion[3] = q[3];
    this->normalize();
    this->evaluateVector();
}

void Rotation::setValue(const Matrix4D& matrix) {
    // 先分解出纯旋转部分：源矩阵可能含缩放或剪切，直接读取会污染四元数
    const Matrix4D rotationMatrix = matrix.decompose()[2];

    const double trace = rotationMatrix[0][0] + rotationMatrix[1][1] + rotationMatrix[2][2];
    if (trace > 0.0) {
        // 迹为正时以标量分量为求解起点，除以 2·s 得到其余分量
        double scale = std::sqrt(1.0 + trace);
        m_quaternion[3] = 0.5 * scale;
        scale = 0.5 / scale;
        m_quaternion[0] = rotationMatrix[2][1] - rotationMatrix[1][2];
        m_quaternion[0] *= scale;
        m_quaternion[1] = rotationMatrix[0][2] - rotationMatrix[2][0];
        m_quaternion[1] *= scale;
        m_quaternion[2] = rotationMatrix[1][0] - rotationMatrix[0][1];
        m_quaternion[2] *= scale;
    } else {
        // 迹非正时改用对角线上最大分量作主元，从对应 2x2 子式解出其余分量
        // 取值方式参考 geometrictools 的 RotationIssues.pdf
        unsigned short mainIndex = 0;
        if (rotationMatrix[1][1] > rotationMatrix[0][0]) {
            mainIndex = 1;
        }
        if (rotationMatrix[2][2] > rotationMatrix[mainIndex][mainIndex]) {
            mainIndex = 2;
        }

        const unsigned short nextIndex = static_cast<unsigned short>((mainIndex + 1) % 3);
        const unsigned short lastIndex = static_cast<unsigned short>((mainIndex + 2) % 3);

        double scale = std::sqrt(
            (rotationMatrix[mainIndex][mainIndex] -
             (rotationMatrix[nextIndex][nextIndex] + rotationMatrix[lastIndex][lastIndex])) +
            1.0);
        m_quaternion[mainIndex] = scale * 0.5;
        scale = 0.5 / scale;
        m_quaternion[3] =
            (rotationMatrix[lastIndex][nextIndex] - rotationMatrix[nextIndex][lastIndex]) * scale;
        m_quaternion[nextIndex] =
            (rotationMatrix[nextIndex][mainIndex] + rotationMatrix[mainIndex][nextIndex]) * scale;
        m_quaternion[lastIndex] =
            (rotationMatrix[lastIndex][mainIndex] + rotationMatrix[mainIndex][lastIndex]) * scale;
    }

    // 矩阵路径只刷新轴角缓存，不做归一化：分解出的旋转部分已正交
    this->evaluateVector();
}

void Rotation::setValue(const Vector3d& axis, double angle) {
    // 保留用户给定的角度原值，getRawValue() 据此返回原始输入
    m_angle = angle;
    // 折算到 [0, 2π)：四元数只依赖半角，超出周期的角度会让后续 getValue() 回读不一致
    const double normalizedAngle =
        angle - std::floor(angle / (2.0 * std::numbers::pi)) * (2.0 * std::numbers::pi);
    m_quaternion[3] = std::cos(normalizedAngle / 2.0);

    Vector3d normalizedAxis = axis;
    // 零向量没有方向，不能直接归一化：只在长度非零时归一化，再按归一化结果决定是否沿用既有轴
    double normalizedLength = 0.0;
    if (normalizedAxis.Length() > 0.0) {
        normalizedAxis.Normalize();
        normalizedLength = normalizedAxis.Length();
    }
    if (std::isfinite(normalizedLength) && normalizedLength > 0.5) {
        m_axis = axis;
    } else {
        normalizedAxis = m_axis;
        normalizedAxis.Normalize();
    }

    const double scale = std::sin(normalizedAngle / 2.0);
    m_quaternion[0] = normalizedAxis.x * scale;
    m_quaternion[1] = normalizedAxis.y * scale;
    m_quaternion[2] = normalizedAxis.z * scale;
}

void Rotation::setValue(const Vector3d& rotateFrom, const Vector3d& rotateTo) {
    // 方向向量为零时旋转无从定义，报错而不是给出无意义的四元数
    if (rotateFrom.IsNull() || rotateTo.IsNull()) {
        throw ValueError("setValue(from, to) 需要两个非零方向向量：零向量没有方向，"
                         "请先给向量赋值，或改用 setValue(axis, angle) 直接给出转轴与转角。");
    }

    Vector3d from = rotateFrom;
    from.Normalize();
    Vector3d to = rotateTo;
    to.Normalize();

    // 两个方向的叉积是旋转轴：它是 (0, from, to) 三点所定平面的法向
    const double dot = from * to;
    const Vector3d axis = from % to;
    const double axisLength = axis.Length();

    if (axisLength == 0.0) {
        // 两向量平行（含反向）
        if (dot > 0.0) {
            // 同向：无需旋转
            this->setValue(0.0, 0.0, 0.0, 1.0);
        } else {
            // 反向：任一垂直于 from 的轴都可作 180° 旋转轴，优先取与 X 轴的叉积
            Vector3d perpendicular = from % Vector3d(1.0, 0.0, 0.0);
            if (perpendicular.Length() < Vector3d::epsilon()) {
                // from 与 X 轴平行时叉积退化，改与 Y 轴叉乘
                perpendicular = from % Vector3d(0.0, 1.0, 0.0);
            }
            this->setValue(perpendicular.x, perpendicular.y, perpendicular.z, 0.0);
        }
    } else {
        // 两向量不平行
        // 注意：给定起点与其像点不足以唯一确定四元数，凡转轴与两向量夹角相同的四元数都成立
        const double angle = std::acos(dot);
        this->setValue(axis, angle);
    }
}

void Rotation::normalize() {
    const double length =
        std::sqrt(m_quaternion[0] * m_quaternion[0] + m_quaternion[1] * m_quaternion[1] +
                  m_quaternion[2] * m_quaternion[2] + m_quaternion[3] * m_quaternion[3]);
    // 零四元数不做缩放：保持全零以便 isNull() 检出，若强行归一化会产生 NaN
    if (length > 0.0) {
        m_quaternion[0] /= length;
        m_quaternion[1] /= length;
        m_quaternion[2] /= length;
        m_quaternion[3] /= length;
    }
}

Rotation& Rotation::invert() {
    // 单位四元数的逆即共轭：取反向量部分
    m_quaternion[0] = -m_quaternion[0];
    m_quaternion[1] = -m_quaternion[1];
    m_quaternion[2] = -m_quaternion[2];

    // 轴缓存同步取反，保证 getValue(axis, angle) 给出的方向仍与实际转向一致
    m_axis.x = -m_axis.x;
    m_axis.y = -m_axis.y;
    m_axis.z = -m_axis.z;

    return *this;
}

Rotation Rotation::inverse() const {
    Rotation rotation;
    rotation.m_quaternion[0] = -m_quaternion[0];
    rotation.m_quaternion[1] = -m_quaternion[1];
    rotation.m_quaternion[2] = -m_quaternion[2];
    rotation.m_quaternion[3] = m_quaternion[3];

    rotation.m_axis[0] = -m_axis[0];
    rotation.m_axis[1] = -m_axis[1];
    rotation.m_axis[2] = -m_axis[2];
    rotation.m_angle = m_angle;
    return rotation;
}

Rotation& Rotation::operator*=(const Rotation& other) {
    return multRight(other);
}

Rotation Rotation::operator*(const Rotation& other) const {
    Rotation result(*this);
    result *= other;
    return result;
}

Rotation& Rotation::multRight(const Rotation& other) {
    // 四元数乘法 (x0,y0,z0,w0) ⊗ (x1,y1,z1,w1)，右乘表示先施加 other
    double x0{};
    double y0{};
    double z0{};
    double w0{};
    this->getValue(x0, y0, z0, w0);

    double x1{};
    double y1{};
    double z1{};
    double w1{};
    other.getValue(x1, y1, z1, w1);

    this->setValue(w0 * x1 + x0 * w1 + y0 * z1 - z0 * y1,
                   w0 * y1 - x0 * z1 + y0 * w1 + z0 * x1,
                   w0 * z1 + x0 * y1 - y0 * x1 + z0 * w1,
                   w0 * w1 - x0 * x1 - y0 * y1 - z0 * z1);
    return *this;
}

Rotation& Rotation::multLeft(const Rotation& other) {
    // 与右乘同一公式，但操作数角色互换，等价于 other ⊗ this
    double x0{};
    double y0{};
    double z0{};
    double w0{};
    other.getValue(x0, y0, z0, w0);

    double x1{};
    double y1{};
    double z1{};
    double w1{};
    this->getValue(x1, y1, z1, w1);

    this->setValue(w0 * x1 + x0 * w1 + y0 * z1 - z0 * y1,
                   w0 * y1 - x0 * z1 + y0 * w1 + z0 * x1,
                   w0 * z1 + x0 * y1 - y0 * x1 + z0 * w1,
                   w0 * w1 - x0 * x1 - y0 * y1 - z0 * z1);
    return *this;
}

bool Rotation::operator==(const Rotation& other) const {
    return isSame(other);
}

bool Rotation::operator!=(const Rotation& other) const {
    return !(*this == other);
}

Vector3d Rotation::multVec(const Vector3d& src) const {
    Vector3d dst;
    multVec(src, dst);
    return dst;
}

void Rotation::multVec(const Vector3d& src, Vector3d& dst) const {
    // 直接把旋转矩阵的元素展开成坐标式，省去构造 Matrix4D 的开销
    const double x = m_quaternion[0];
    const double y = m_quaternion[1];
    const double z = m_quaternion[2];
    const double w = m_quaternion[3];
    const double x2 = x * x;
    const double y2 = y * y;
    const double z2 = z * z;
    const double w2 = w * w;

    const double dx =
        (x2 + w2 - y2 - z2) * src.x + 2.0 * (x * y - z * w) * src.y + 2.0 * (x * z + y * w) * src.z;
    const double dy =
        2.0 * (x * y + z * w) * src.x + (w2 - x2 + y2 - z2) * src.y + 2.0 * (y * z - x * w) * src.z;
    const double dz =
        2.0 * (x * z - y * w) * src.x + 2.0 * (x * w + y * z) * src.y + (w2 - x2 - y2 + z2) * src.z;
    dst.x = dx;
    dst.y = dy;
    dst.z = dz;
}

void Rotation::multVec(const Vector3f& src, Vector3f& dst) const {
    // 借双精度路径做变换：单精度入参的量化误差已远大于这步提升带来的收益
    Vector3d sourceDouble = toVector<double>(src);
    multVec(sourceDouble, sourceDouble);
    dst = toVector<float>(sourceDouble);
}

Vector3f Rotation::multVec(const Vector3f& src) const {
    Vector3f dst;
    multVec(src, dst);
    return dst;
}

void Rotation::scaleAngle(const double scaleFactor) {
    Vector3d axis;
    double angle{};
    this->getValue(axis, angle);
    // 重新走 setValue(axis, angle)：顺带把轴归一化并刷新轴角缓存
    this->setValue(axis, angle * scaleFactor);
}

Rotation Rotation::slerp(const Rotation& q0, const Rotation& q1, double t) {
    // 参数钳制到 [0, 1]，使越界输入退化为端点而不是外插
    if (t < 0.0) {
        t = 0.0;
    } else if (t > 1.0) {
        t = 1.0;
    }

    double scale0 = 1.0 - t;
    double scale1 = t;
    const double dot =
        q0.m_quaternion[0] * q1.m_quaternion[0] + q0.m_quaternion[1] * q1.m_quaternion[1] +
        q0.m_quaternion[2] * q1.m_quaternion[2] + q0.m_quaternion[3] * q1.m_quaternion[3];
    // 点积为负说明两四元数分处单位球两侧，取反后走短弧，否则会绕远路
    bool negate = false;
    double absoluteDot = dot;
    if (dot < 0.0) {
        absoluteDot = -dot;
        negate = true;
    }

    if ((1.0 - absoluteDot) > Vector3d::epsilon()) {
        const double angle = std::acos(absoluteDot);
        const double sineAngle = std::sin(angle);
        // 角度过小（sin 趋零）时退化为线性插值，避免除零
        if (sineAngle > Vector3d::epsilon()) {
            scale0 = std::sin((1.0 - t) * angle) / sineAngle;
            scale1 = std::sin(t * angle) / sineAngle;
        }
    }

    if (negate) {
        scale1 = -scale1;
    }

    const double x = scale0 * q0.m_quaternion[0] + scale1 * q1.m_quaternion[0];
    const double y = scale0 * q0.m_quaternion[1] + scale1 * q1.m_quaternion[1];
    const double z = scale0 * q0.m_quaternion[2] + scale1 * q1.m_quaternion[2];
    const double w = scale0 * q0.m_quaternion[3] + scale1 * q1.m_quaternion[3];
    return Rotation(x, y, z, w);
}

Rotation Rotation::identity() {
    return Rotation(0.0, 0.0, 0.0, 1.0);
}

Rotation Rotation::makeRotationByAxes(Vector3d xdir,
                                      Vector3d ydir,
                                      Vector3d zdir,
                                      const char* priorityOrder) {
    constexpr int XAxis = 0;
    constexpr int YAxis = 1;
    constexpr int ZAxis = 2;

    // 与 FreeCAD 一致地采用 OCC 的重合容差：方向长度低于该值即视为未提供
    const double ConfusionTolerance = Precision::Confusion();

    if (priorityOrder == nullptr) {
        throw ValueError(
            "makeRotationByAxes：优先级串为空指针，请传入由三个大写轴字母组成的字符串，"
            "例如 \"ZXY\"（默认值）");
    }

    const std::string_view priorityView(priorityOrder);
    if (priorityView.size() != 3) {
        throw ValueError(
            std::format("makeRotationByAxes：优先级串长度为 {}，必须恰为 3，例如 \"ZXY\"",
                        priorityView.size()));
    }

    std::array<int, 3> order{};
    for (int index = 0; index < 3; ++index) {
        order[static_cast<std::size_t>(index)] =
            priorityView[static_cast<std::size_t>(index)] - 'X';
        // 只接受大写的 X/Y/Z：'X'..'Z' 与 'X' 的差落在 0..2，小写字母与其它字符一律非法
        if (order[static_cast<std::size_t>(index)] < 0 ||
            order[static_cast<std::size_t>(index)] > 2) {
            throw ValueError(std::format(
                "makeRotationByAxes：优先级串第 {} 个字符不是大写 X、Y 或 Z，请改成 \"{}\" 形式",
                index + 1,
                "ZXY"));
        }
    }

    // 三个轴必须各出现一次，否则后出现的重复轴会覆盖前面的设定
    if (order[0] == order[1] || order[1] == order[2] || order[2] == order[0]) {
        throw ValueError("makeRotationByAxes：优先级串未把 X、Y、Z 各列出一次，请改成 "
                         "\"ZXY\" 这类三轴各一次的排列");
    }

    // 三个方向按轴序号索引，便于用优先级串里的数字取用
    std::array<Vector3d*, 3> directions{&xdir, &ydir, &zdir};

    // 把一个元素移到末尾并让其余元素前移，用于淘汰已不可用的优先级项
    auto dropPriority = [&order](int index) {
        int temporary{};
        if (index == 0) {
            temporary = order[0];
            order[0] = order[1];
            order[1] = order[2];
            order[2] = temporary;
        } else if (index == 1) {
            temporary = order[1];
            order[1] = order[2];
            order[2] = temporary;
        }
        // index == 2 时无事可做：淘汰末位不影响其它优先级
    };

    // 取优先级最高的非零方向作主轴，全部为零则无法构造旋转
    Vector3d mainDirection;
    for (int attempt = 0; attempt < 3; ++attempt) {
        mainDirection = *directions[static_cast<std::size_t>(order[0])];
        if (mainDirection.Length() > ConfusionTolerance) {
            break;
        }

        dropPriority(0);

        if (attempt == 2) {
            throw ValueError("makeRotationByAxes：三个方向向量全为零，无法确定主轴；"
                             "请至少给出一个非零方向");
        }
    }
    mainDirection.Normalize();

    // 取次优先级方向作提示方向：它与主轴必须不平行，否则无法定出旋转平面
    Vector3d hintDirection;
    for (int attempt = 0; attempt < 2; ++attempt) {
        hintDirection = *directions[static_cast<std::size_t>(order[1])];
        if ((hintDirection.Cross(mainDirection)).Length() > ConfusionTolerance) {
            break;
        }

        dropPriority(1);

        if (attempt == 1) {
            // 无可用的提示方向：置零表示接下来需要按主轴自动猜一个
            hintDirection = Vector3d();
        }
    }
    if (hintDirection.Length() == 0.0) {
        // 按主轴选择最贴近的全局轴作提示方向，并同步改写剩余优先级顺序
        switch (order[0]) {
        case XAxis: {
            // 主轴是 X：优先把 Z 方向对齐到全局 Z
            order[1] = ZAxis;
            order[2] = YAxis;
            hintDirection = Vector3d(0.0, 0.0, 1.0);
            if ((hintDirection.Cross(mainDirection)).Length() <= ConfusionTolerance) {
                // 主轴本身沿 Z，改把 Y 方向对齐到全局 Y
                hintDirection = Vector3d(0.0, 1.0, 0.0);
                order[1] = YAxis;
                order[2] = ZAxis;
            }
        } break;
        case YAxis: {
            // 主轴是 Y：优先把 Z 方向对齐到全局 Z，符号跟随主轴
            order[1] = ZAxis;
            order[2] = XAxis;
            hintDirection = mainDirection.z > -ConfusionTolerance ? Vector3d(0.0, 0.0, 1.0)
                                                                  : Vector3d(0.0, 0.0, -1.0);
            if ((hintDirection.Cross(mainDirection)).Length() <= ConfusionTolerance) {
                // 主轴本身沿 Z，改把 X 方向对齐到全局 X
                hintDirection = Vector3d(1.0, 0.0, 0.0);
                order[1] = XAxis;
                order[2] = ZAxis;
            }
        } break;
        case ZAxis: {
            // 主轴是 Z：优先把 Y 方向对齐到全局 Z
            order[1] = YAxis;
            order[2] = XAxis;
            hintDirection = Vector3d(0.0, 0.0, 1.0);
            if ((hintDirection.Cross(mainDirection)).Length() <= ConfusionTolerance) {
                // 主轴本身沿 Z，改把 X 方向对齐到全局 X
                hintDirection = Vector3d(1.0, 0.0, 0.0);
                order[1] = XAxis;
                order[2] = YAxis;
            }
        } break;
        default:
            break;
        }
    }

    // 内部不变式：三条优先级必须两两不同，否则 finaldirs 会被重复写入
    assert(order[0] != order[1]);
    assert(order[1] != order[2]);
    assert(order[2] != order[0]);

    hintDirection.Normalize();
    // 先叉乘得到第三个轴方向，再回转一次叉乘把提示方向校正到与主轴垂直
    Vector3d lastDirection = mainDirection.Cross(hintDirection);
    lastDirection.Normalize();
    hintDirection = lastDirection.Cross(mainDirection);
    hintDirection.Normalize();

    std::array<Vector3d, 3> finalDirections;
    finalDirections[static_cast<std::size_t>(order[0])] = mainDirection;
    finalDirections[static_cast<std::size_t>(order[1])] = hintDirection;
    finalDirections[static_cast<std::size_t>(order[2])] = lastDirection;

    // 修正手性：三个轴必须构成右手系，否则翻转最不重要的那个轴
    if (finalDirections[static_cast<std::size_t>(XAxis)].Cross(
            finalDirections[static_cast<std::size_t>(YAxis)]) *
            finalDirections[static_cast<std::size_t>(ZAxis)] <
        0.0) {
        finalDirections[static_cast<std::size_t>(order[2])] =
            finalDirections[static_cast<std::size_t>(order[2])] * (-1.0);
    }

    // 三个方向作为矩阵的列向量，即局部轴在全局系下的像
    Matrix4D matrix;
    matrix.setToUnity();
    for (int index = 0; index < 3; ++index) {
        const Vector3d& direction = finalDirections[static_cast<std::size_t>(index)];
        matrix[0][static_cast<unsigned int>(index)] = direction.x;
        matrix[1][static_cast<unsigned int>(index)] = direction.y;
        matrix[2][static_cast<unsigned int>(index)] = direction.z;
    }

    return Rotation(matrix);
}

void Rotation::setYawPitchRoll(double y, double p, double r) {
    // 输入是角度制的 XY'Z'' 内旋角，转弧度后按半角公式合成四元数
    y = radiansFromDegrees(y);
    p = radiansFromDegrees(p);
    r = radiansFromDegrees(r);

    const double c1 = std::cos(y / 2.0);
    const double s1 = std::sin(y / 2.0);
    const double c2 = std::cos(p / 2.0);
    const double s2 = std::sin(p / 2.0);
    const double c3 = std::cos(r / 2.0);
    const double s3 = std::sin(r / 2.0);

    this->setValue(c1 * c2 * s3 - s1 * s2 * c3,
                   c1 * s2 * c3 + s1 * c2 * s3,
                   s1 * c2 * c3 - c1 * s2 * s3,
                   c1 * c2 * c3 + s1 * s2 * s3);
}

void Rotation::getYawPitchRoll(double& y, double& p, double& r) const {
    const double q00 = m_quaternion[0] * m_quaternion[0];
    const double q11 = m_quaternion[1] * m_quaternion[1];
    const double q22 = m_quaternion[2] * m_quaternion[2];
    const double q33 = m_quaternion[3] * m_quaternion[3];
    const double q01 = m_quaternion[0] * m_quaternion[1];
    const double q02 = m_quaternion[0] * m_quaternion[2];
    const double q03 = m_quaternion[0] * m_quaternion[3];
    const double q12 = m_quaternion[1] * m_quaternion[2];
    const double q13 = m_quaternion[1] * m_quaternion[3];
    const double q23 = m_quaternion[2] * m_quaternion[3];
    const double pitchTerm = 2.0 * (q13 - q02);

    // 容差取自 OCCT gp_Quaternion：用于识别俯仰角到达 ±90° 的万向锁
    constexpr double GimbalLockTolerance = 16 * std::numeric_limits<double>::epsilon();
    if (std::fabs(pitchTerm - 1.0) <= GimbalLockTolerance) {
        // 万向锁（北极）：偏航与滚转退化为绕同一轴的合成角，约定把偏航取 0
        y = 0.0;
        p = std::numbers::pi / 2.0;
        r = 2.0 * std::atan2(m_quaternion[0], m_quaternion[3]);
    } else if (std::fabs(pitchTerm + 1.0) <= GimbalLockTolerance) {
        // 万向锁（南极）：同上，俯仰取 -90°
        y = 0.0;
        p = -std::numbers::pi / 2.0;
        r = 2.0 * std::atan2(m_quaternion[0], m_quaternion[3]);
    } else {
        y = std::atan2(2.0 * (q01 + q23), (q00 + q33) - (q11 + q22));
        // 先钳制再 asin：浮点误差可能让 pitchTerm 略微越出 [-1, 1] 而得到 NaN
        p = pitchTerm > 1.0 ? std::numbers::pi / 2.0
                            : (pitchTerm < -1.0 ? -std::numbers::pi / 2.0 : std::asin(pitchTerm));
        r = std::atan2(2.0 * (q12 + q03), (q22 + q33) - (q00 + q11));
    }

    // 对外统一使用角度制，与 setYawPitchRoll 的输入单位一致
    y = degreesFromRadians(y);
    p = degreesFromRadians(p);
    r = degreesFromRadians(r);
}

bool Rotation::isSame(const Rotation& other) const {
    // 四元数整体取反表示同一旋转，两种符号都要接受
    return (
        (m_quaternion[0] == other.m_quaternion[0] && m_quaternion[1] == other.m_quaternion[1] &&
         m_quaternion[2] == other.m_quaternion[2] && m_quaternion[3] == other.m_quaternion[3]) ||
        (m_quaternion[0] == -other.m_quaternion[0] && m_quaternion[1] == -other.m_quaternion[1] &&
         m_quaternion[2] == -other.m_quaternion[2] && m_quaternion[3] == -other.m_quaternion[3]));
}

bool Rotation::isSame(const Rotation& other, double tol) const {
    // Coin3d 的做法：两四元数分量差的平方和可化简为 2 - 2·dot，故只需比较点积
    // 该化简成立的前提是双方均已归一化；取绝对值以兼容整体取反的等价表示
    const double dot =
        other.m_quaternion[0] * m_quaternion[0] + other.m_quaternion[1] * m_quaternion[1] +
        other.m_quaternion[2] * m_quaternion[2] + other.m_quaternion[3] * m_quaternion[3];
    return std::fabs(dot) >= 1.0 - tol / 2;
}

bool Rotation::isIdentity() const {
    return ((m_quaternion[0] == 0.0 && m_quaternion[1] == 0.0 && m_quaternion[2] == 0.0) &&
            (m_quaternion[3] == 1.0 || m_quaternion[3] == -1.0));
}

bool Rotation::isIdentity(double tol) const {
    return isSame(Rotation(), tol);
}

bool Rotation::isNull() const {
    return (m_quaternion[0] == 0.0 && m_quaternion[1] == 0.0 && m_quaternion[2] == 0.0 &&
            m_quaternion[3] == 0.0);
}

const char* Rotation::eulerSequenceName(EulerSequence seq) {
    // 哨兵与非法值没有名字，返回空指针而不是伪造表项
    if (seq == Invalid || seq >= EulerSequenceLast) {
        return nullptr;
    }
    return EulerSequenceNames[static_cast<std::size_t>(seq) - 1];
}

Rotation::EulerSequence Rotation::eulerSequenceFromName(const char* name) {
    if (name != nullptr) {
        for (std::size_t index = 0; index < EulerSequenceNames.size(); ++index) {
            if (isAsciiCaseInsensitiveEqual(name, EulerSequenceNames[index])) {
                return static_cast<EulerSequence>(index + 1);
            }
        }
    }
    return Invalid;
}

void Rotation::setEulerAngles(EulerSequence order, double alpha, double beta, double gamma) {
    // 非法序列无法映射到轴三元组，直接拒绝而不是悄悄按某个序列解释
    if (order == Invalid || order >= EulerSequenceLast) {
        throw ValueError(std::format(
            "setEulerAngles：欧拉序列取值 {} 非法，请传入 Invalid 与 EulerSequenceLast 之间的具体"
            "序列，例如 Intrinsic_ZXY；由名字转换时可用 eulerSequenceFromName() 并检查它是否"
            "返回 Invalid",
            static_cast<int>(order)));
    }

    const EulerSequenceParameters parameters = translateEulerSequence(order);

    // 对外接口用角度制，内部计算一律用弧度制
    alpha = radiansFromDegrees(alpha);
    beta = radiansFromDegrees(beta);
    gamma = radiansFromDegrees(gamma);

    double first = alpha;
    double second = beta;
    double third = gamma;
    if (!parameters.m_usesFixedAxes) {
        // 内旋等价于「同角度、轴序相反」的外旋，故交换首末角
        std::swap(first, third);
    }

    if (parameters.m_hasOddPermutation) {
        // 奇排列需要额外对第二个角度取反，以对齐轴三重组的右手系
        second = -second;
    }

    const double halfFirst = 0.5 * first;
    const double halfSecond = 0.5 * second;
    const double halfThird = 0.5 * third;
    const double cosFirst = std::cos(halfFirst);
    const double cosSecond = std::cos(halfSecond);
    const double cosThird = std::cos(halfThird);
    const double sinFirst = std::sin(halfFirst);
    const double sinSecond = std::sin(halfSecond);
    const double sinThird = std::sin(halfThird);
    const double cosCos = cosFirst * cosThird;
    const double cosSin = cosFirst * sinThird;
    const double sinCos = sinFirst * cosThird;
    const double sinSin = sinFirst * sinThird;

    std::array<double, 4> values{};  // 顺序为 w, x, y, z
    if (parameters.m_repeatsFirstAxis) {
        // 真欧拉角：首末轴相同，分量组合方式与泰特-布莱恩角不同
        values[static_cast<std::size_t>(parameters.m_firstAxis)] = cosSecond * (cosSin + sinCos);
        values[static_cast<std::size_t>(parameters.m_secondAxis)] = sinSecond * (cosCos + sinSin);
        values[static_cast<std::size_t>(parameters.m_thirdAxis)] = sinSecond * (cosSin - sinCos);
        values[0] = cosSecond * (cosCos - sinSin);
    } else {
        values[static_cast<std::size_t>(parameters.m_firstAxis)] =
            cosSecond * sinCos - sinSecond * cosSin;
        values[static_cast<std::size_t>(parameters.m_secondAxis)] =
            cosSecond * sinSin + sinSecond * cosCos;
        values[static_cast<std::size_t>(parameters.m_thirdAxis)] =
            cosSecond * cosSin - sinSecond * sinCos;
        values[0] = cosSecond * cosCos + sinSecond * sinSin;
    }
    if (parameters.m_hasOddPermutation) {
        // 与前面取反 second 对应，这里把中间分量再翻回来
        values[static_cast<std::size_t>(parameters.m_secondAxis)] =
            -values[static_cast<std::size_t>(parameters.m_secondAxis)];
    }

    m_quaternion[0] = values[1];
    m_quaternion[1] = values[2];
    m_quaternion[2] = values[3];
    m_quaternion[3] = values[0];
    // 平移组合出的四元数只差一个公共尺度，刷新轴角缓存即可
    this->evaluateVector();
}

void Rotation::getEulerAngles(EulerSequence order,
                              double& alpha,
                              double& beta,
                              double& gamma) const {
    // 与 setEulerAngles 保持一致：非法序列无法映射到轴三元组，直接拒绝
    if (order == Invalid || order >= EulerSequenceLast) {
        throw ValueError(std::format(
            "getEulerAngles：欧拉序列取值 {} 非法，请传入 Invalid 与 EulerSequenceLast 之间的具体"
            "序列，例如 Intrinsic_ZXY；由名字转换时可用 eulerSequenceFromName() 并检查它是否"
            "返回 Invalid",
            static_cast<int>(order)));
    }

    Matrix4D matrix;
    getValue(matrix);

    // 通用算法用 1..3 表示 X/Y/Z 轴，这里换算成矩阵的 0 基下标
    const auto elementAt = [&matrix](int row, int column) -> double {
        return matrix[static_cast<unsigned int>(row - 1)][static_cast<unsigned int>(column - 1)];
    };

    const EulerSequenceParameters parameters = translateEulerSequence(order);
    if (parameters.m_repeatsFirstAxis) {
        // 真欧拉角：由第一行元素解出首末角，由 (1,1) 与副对角元素解出中间角
        const double firstRowSecond = elementAt(parameters.m_firstAxis, parameters.m_secondAxis);
        const double firstRowThird = elementAt(parameters.m_firstAxis, parameters.m_thirdAxis);
        const double sineBeta =
            std::sqrt(firstRowSecond * firstRowSecond + firstRowThird * firstRowThird);
        if (sineBeta > 16 * std::numeric_limits<double>::epsilon()) {
            alpha = std::atan2(elementAt(parameters.m_firstAxis, parameters.m_secondAxis),
                               elementAt(parameters.m_firstAxis, parameters.m_thirdAxis));
            gamma = std::atan2(elementAt(parameters.m_secondAxis, parameters.m_firstAxis),
                               -elementAt(parameters.m_thirdAxis, parameters.m_firstAxis));
        } else {
            // 中间角接近 0 或 π：首末角退化，只能定出它们的和差
            alpha = std::atan2(-elementAt(parameters.m_secondAxis, parameters.m_thirdAxis),
                               elementAt(parameters.m_secondAxis, parameters.m_secondAxis));
            gamma = 0.0;
        }
        beta = std::atan2(sineBeta, elementAt(parameters.m_firstAxis, parameters.m_firstAxis));
    } else {
        // 泰特-布莱恩角：由第二列元素解出首末角，由 (3,1) 与第一列模长解出中间角
        const double firstRowFirst = elementAt(parameters.m_firstAxis, parameters.m_firstAxis);
        const double secondRowFirst = elementAt(parameters.m_secondAxis, parameters.m_firstAxis);
        const double cosineBeta =
            std::sqrt(firstRowFirst * firstRowFirst + secondRowFirst * secondRowFirst);
        if (cosineBeta > 16 * std::numeric_limits<double>::epsilon()) {
            alpha = std::atan2(elementAt(parameters.m_thirdAxis, parameters.m_secondAxis),
                               elementAt(parameters.m_thirdAxis, parameters.m_thirdAxis));
            gamma = std::atan2(elementAt(parameters.m_secondAxis, parameters.m_firstAxis),
                               elementAt(parameters.m_firstAxis, parameters.m_firstAxis));
        } else {
            // 中间角接近 ±90°（万向锁）：首末角退化，只能定出它们的和差
            alpha = std::atan2(-elementAt(parameters.m_secondAxis, parameters.m_thirdAxis),
                               elementAt(parameters.m_secondAxis, parameters.m_secondAxis));
            gamma = 0.0;
        }
        beta = std::atan2(-elementAt(parameters.m_thirdAxis, parameters.m_firstAxis), cosineBeta);
    }
    if (parameters.m_hasOddPermutation) {
        // 与 setEulerAngles 的取反操作配对，三个角一并翻号
        alpha = -alpha;
        beta = -beta;
        gamma = -gamma;
    }
    if (!parameters.m_usesFixedAxes) {
        // 内旋在轴序上与外旋相反，首末角换回用户视角
        std::swap(alpha, gamma);
    }

    alpha = degreesFromRadians(alpha);
    beta = degreesFromRadians(beta);
    gamma = degreesFromRadians(gamma);
}
}  // namespace ExpressionEngine::Base
