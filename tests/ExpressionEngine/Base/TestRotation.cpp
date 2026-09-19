// 覆盖 Rotation 的关键行为：单位旋转、四元数与轴角互转、旋转复合与取逆、归一化与零四元数
// 拒绝面、矩阵往返、slerp、欧拉角（含非法序列拒绝）以及 makeRotationByAxes 的拒绝面。

#include <gtest/gtest.h>

#include <numbers>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Rotation.h>

using ExpressionEngine::Base::Matrix4D;
using ExpressionEngine::Base::Rotation;
using ExpressionEngine::Base::ValueError;
using ExpressionEngine::Base::Vector3d;
using ExpressionEngine::Base::Vector3f;

namespace
{

    /// 判定用的通用容差，单位与四元数分量一致
    constexpr double Tolerance = 1e-12;

} // namespace

/**
 * @brief 钉住：默认构造就是单位旋转（四元数 0,0,0,1），且不会改变任何向量
 */
TEST(RotationTest, DefaultConstructorIsIdentity)
{
    const Rotation rotation;
    const double * values = rotation.getValue();
    EXPECT_DOUBLE_EQ(values[0], 0.0);
    EXPECT_DOUBLE_EQ(values[1], 0.0);
    EXPECT_DOUBLE_EQ(values[2], 0.0);
    EXPECT_DOUBLE_EQ(values[3], 1.0);
    EXPECT_TRUE(rotation.isIdentity());
    EXPECT_FALSE(rotation.isNull());

    const Vector3d point(1.0, 2.0, 3.0);
    EXPECT_TRUE(rotation.multiplyVector(point).isEqual(point, Tolerance));
}

/**
 * @brief 钉住：轴角构造出的四元数符合半角公式，且四元数与轴角能双向取回
 */
TEST(RotationTest, AxisAngleAndQuaternionRoundTrip)
{
    constexpr double quarterTurn = std::numbers::pi / 2.0;
    const Rotation   rotation(Vector3d(0.0, 0.0, 1.0), quarterTurn);

    const double *values = rotation.getValue();
    EXPECT_NEAR(values[0], 0.0, Tolerance);
    EXPECT_NEAR(values[1], 0.0, Tolerance);
    EXPECT_NEAR(values[2], std::sin(quarterTurn / 2.0), Tolerance);
    EXPECT_NEAR(values[3], std::cos(quarterTurn / 2.0), Tolerance);

    // 绕 Z 轴转 90°：X 轴应落到 Y 轴上
    EXPECT_TRUE(rotation.multiplyVector(Vector3d(1.0, 0.0, 0.0)).isEqual(Vector3d(0.0, 1.0, 0.0), Tolerance));

    Vector3d axis;
    double   angle = 0.0;
    rotation.getValue(axis, angle);
    EXPECT_TRUE(axis.isEqual(Vector3d(0.0, 0.0, 1.0), Tolerance));
    EXPECT_DOUBLE_EQ(angle, quarterTurn);

    // getRawValue 不归一化轴：轴未缩放时两者结果一致
    rotation.getRawValue(axis, angle);
    EXPECT_TRUE(axis.isEqual(Vector3d(0.0, 0.0, 1.0), Tolerance));
    EXPECT_DOUBLE_EQ(angle, quarterTurn);

    // 零向量轴回退为 Z 轴，退化输入不应产生 NaN
    const Rotation fallback(Vector3d(0.0, 0.0, 0.0), quarterTurn);
    fallback.getValue(axis, angle);
    EXPECT_FALSE(std::isnan(axis.x) || std::isnan(axis.y) || std::isnan(axis.z));
    EXPECT_TRUE(axis.isEqual(Vector3d(0.0, 0.0, 1.0), Tolerance));
}

/**
 * @brief 钉住：构造时对非单位四元数做归一化，缩放因子不影响所表示的旋转
 */
TEST(RotationTest, NonUnitQuaternionIsNormalized)
{
    // (0, 0, 2, 0) 是绕 Z 轴 180° 的四元数放大两倍
    const Rotation scaled(0.0, 0.0, 2.0, 0.0);
    EXPECT_FALSE(scaled.isNull());
    EXPECT_TRUE(scaled.multiplyVector(Vector3d(1.0, 0.0, 0.0)).isEqual(Vector3d(-1.0, 0.0, 0.0), Tolerance));

    const double *values = scaled.getValue();
    EXPECT_NEAR(values[2], 1.0, Tolerance);
    EXPECT_NEAR(values[3], 0.0, Tolerance);
}

/**
 * @brief 钉住：零四元数可构造、可检出（isNull），但其轴角退化且不产生 NaN；
 *        它是调用方必须自己检查的拒绝面，不是内部会替你修正的错误
 */
TEST(RotationTest, NullQuaternionIsDetectableButHasNoUsableAxis)
{
    const Rotation nullRotation(0.0, 0.0, 0.0, 0.0);
    EXPECT_TRUE(nullRotation.isNull());
    EXPECT_FALSE(nullRotation.isIdentity());

    const double *values = nullRotation.getValue();
    for (int index = 0; index < 4; ++index)
    {
        EXPECT_DOUBLE_EQ(values[index], 0.0) << "第 " << index << " 个分量被非预期地改写";
    }

    Vector3d axis;
    double   angle = 0.0;
    nullRotation.getValue(axis, angle);
    // 零轴表示没有可信方向，绝不能伪造出一个轴向
    EXPECT_DOUBLE_EQ(axis.length(), 0.0);
    EXPECT_FALSE(std::isnan(axis.x) || std::isnan(axis.y) || std::isnan(axis.z));
    EXPECT_FALSE(std::isnan(angle));
}

/**
 * @brief 钉住：复合是四元数右乘（右操作数先施加），两次 90° 复合等于 180°，
 *        且 multiplyLeft 与 multiplyRight 的作用顺序相反
 */
TEST(RotationTest, CompositionFollowsQuaternionMultiplication)
{
    constexpr double quarterTurn = std::numbers::pi / 2.0;
    const Rotation   rotationZ90(Vector3d(0.0, 0.0, 1.0), quarterTurn);
    const Rotation   rotationX90(Vector3d(1.0, 0.0, 0.0), quarterTurn);
    const Vector3d   point(1.0, 0.0, 0.0);

    // composed == rotationZ90 ∘ rotationX90：先 X 后 Z
    const Rotation composed   = rotationZ90 * rotationX90;
    const Vector3d sequential = rotationZ90.multiplyVector(rotationX90.multiplyVector(point));
    EXPECT_TRUE(composed.multiplyVector(point).isEqual(sequential, Tolerance));

    const Rotation doubled = rotationZ90 * rotationZ90;
    EXPECT_TRUE(doubled.isSame(Rotation(Vector3d(0.0, 0.0, 1.0), std::numbers::pi), Tolerance));

    Rotation leftProduct = rotationZ90;
    leftProduct.multiplyLeft(rotationX90);
    EXPECT_TRUE(leftProduct.isSame(rotationX90 * rotationZ90, Tolerance));

    Rotation rightProduct = rotationZ90;
    rightProduct.multiplyRight(rotationX90);
    EXPECT_TRUE(rightProduct.isSame(rotationZ90 * rotationX90, Tolerance));
}

/**
 * @brief 钉住：invert() 与 inverse() 都还原出逆旋转，与自身复合得到单位旋转
 */
TEST(RotationTest, InverseUndoesRotation)
{
    const Rotation rotation(Vector3d(1.0, 2.0, 3.0), 0.7);
    EXPECT_TRUE((rotation * rotation.inverse()).isIdentity(Tolerance));

    Rotation inPlace(rotation);
    inPlace.invert();
    EXPECT_TRUE(inPlace.isSame(rotation.inverse(), Tolerance));

    // 逆旋转把像点送回原点
    const Vector3d point(0.5, -1.0, 2.0);
    const Vector3d rotated = rotation.multiplyVector(point);
    EXPECT_TRUE(rotation.inverse().multiplyVector(rotated).isEqual(point, 1e-12));
}

/**
 * @brief 钉住：slerp 端点即端点旋转、中点是半角旋转，t 越界被钳制到端点
 */
TEST(RotationTest, SlerpHitsEndpointsAndClampsParameter)
{
    const Rotation start = Rotation::identity();
    const Rotation end(Vector3d(0.0, 0.0, 1.0), std::numbers::pi / 2.0);

    EXPECT_TRUE(Rotation::slerp(start, end, 0.0).isSame(start, Tolerance));
    EXPECT_TRUE(Rotation::slerp(start, end, 1.0).isSame(end, Tolerance));
    EXPECT_TRUE(Rotation::slerp(start, end, 0.5).isSame(Rotation(Vector3d(0.0, 0.0, 1.0), std::numbers::pi / 4.0), 1e-9));
    EXPECT_TRUE(Rotation::slerp(start, end, -1.0).isSame(start, Tolerance));
    EXPECT_TRUE(Rotation::slerp(start, end, 2.0).isSame(end, Tolerance));
}

/**
 * @brief 钉住：setEulerAngles / getEulerAngles 在同一序列下按角度制往返一致，
 *        偏航-俯仰-滚转同样往返一致
 */
TEST(RotationTest, EulerAnglesAndYawPitchRollRoundTripInDegrees)
{
    Rotation rotation;
    rotation.setEulerAngles(Rotation::Intrinsic_ZXY, 30.0, 40.0, 50.0);

    double alpha = 0.0;
    double beta  = 0.0;
    double gamma = 0.0;
    rotation.getEulerAngles(Rotation::Intrinsic_ZXY, alpha, beta, gamma);
    EXPECT_NEAR(alpha, 30.0, 1e-9) << "第一转角应往返一致";
    EXPECT_NEAR(beta, 40.0, 1e-9) << "第二转角应往返一致";
    EXPECT_NEAR(gamma, 50.0, 1e-9) << "第三转角应往返一致";

    // 静态工厂与成员函数等价
    const Rotation fromFactory = Rotation::fromEulerAngles(Rotation::Intrinsic_ZXY, 30.0, 40.0, 50.0);
    EXPECT_TRUE(fromFactory.isSame(rotation, 1e-9));

    rotation.setYawPitchRoll(10.0, 20.0, 30.0);
    double y = 0.0;
    double p = 0.0;
    double r = 0.0;
    rotation.getYawPitchRoll(y, p, r);
    EXPECT_NEAR(y, 10.0, 1e-9);
    EXPECT_NEAR(p, 20.0, 1e-9);
    EXPECT_NEAR(r, 30.0, 1e-9);
}

/**
 * @brief 钉住：非法欧拉序列（Invalid 与哨兵值）在设置与读取两侧都被 ValueError 拒绝，
 *        而不是悄悄按某种序列解释
 */
TEST(RotationTest, EulerAnglesRejectInvalidSequence)
{
    Rotation rotation;
    double   alpha = 0.0;
    double   beta  = 0.0;
    double   gamma = 0.0;

    EXPECT_THROW(rotation.setEulerAngles(Rotation::Invalid, 10.0, 20.0, 30.0), ValueError);
    EXPECT_THROW(rotation.setEulerAngles(Rotation::EulerSequenceLast, 10.0, 20.0, 30.0), ValueError);
    EXPECT_THROW(rotation.getEulerAngles(Rotation::Invalid, alpha, beta, gamma), ValueError);
    EXPECT_THROW(rotation.getEulerAngles(Rotation::EulerSequenceLast, alpha, beta, gamma), ValueError);

    // 合法序列不受影响
    EXPECT_NO_THROW(rotation.setEulerAngles(Rotation::Intrinsic_ZXY, 10.0, 20.0, 30.0));
}

/**
 * @brief 钉住：序列名与序列编号双向映射，名字匹配大小写不敏感，非法名字返回 nullptr/Invalid
 */
TEST(RotationTest, EulerSequenceNameMappingIsCaseInsensitive)
{
    EXPECT_STREQ(Rotation::eulerSequenceName(Rotation::EulerAngles), "Euler");
    EXPECT_STREQ(Rotation::eulerSequenceName(Rotation::Intrinsic_ZXY), "IZXY");
    EXPECT_EQ(Rotation::eulerSequenceName(Rotation::Invalid), nullptr);
    EXPECT_EQ(Rotation::eulerSequenceName(Rotation::EulerSequenceLast), nullptr);

    EXPECT_EQ(Rotation::eulerSequenceFromName("izxy"), Rotation::Intrinsic_ZXY);
    EXPECT_EQ(Rotation::eulerSequenceFromName("YawPitchRoll"), Rotation::YawPitchRoll);
    EXPECT_EQ(Rotation::eulerSequenceFromName("nonsense"), Rotation::Invalid);
    EXPECT_EQ(Rotation::eulerSequenceFromName(nullptr), Rotation::Invalid);
}

/**
 * @brief 钉住：makeRotationByAxes 把局部轴映射到给定方向，并对优先级串与全零方向抛 ValueError
 */
TEST(RotationTest, MakeRotationByAxesMapsLocalAxesAndRejectsBadInput)
{
    // 正交单位方向按默认优先级 "ZXY" 构造出单位旋转
    EXPECT_TRUE(Rotation::makeRotationByAxes(Vector3d(1.0, 0.0, 0.0), Vector3d(0.0, 1.0, 0.0), Vector3d(0.0, 0.0, 1.0)).isIdentity(1e-12));

    // 只给出 zDirection（优先级最高的 Z）时，局部 Z 轴应对准它
    const Rotation aligned = Rotation::makeRotationByAxes(Vector3d(), Vector3d(), Vector3d(1.0, 0.0, 0.0));
    EXPECT_TRUE(aligned.multiplyVector(Vector3d(0.0, 0.0, 1.0)).isEqual(Vector3d(1.0, 0.0, 0.0), 1e-12));

    // 拒绝面：长度不为 3、含小写、轴重复、三方向全零
    EXPECT_THROW(Rotation::makeRotationByAxes(Vector3d(1.0, 0.0, 0.0), Vector3d(), Vector3d(), "ZX"), ValueError);
    EXPECT_THROW(Rotation::makeRotationByAxes(Vector3d(1.0, 0.0, 0.0), Vector3d(), Vector3d(), "zxy"), ValueError);
    EXPECT_THROW(Rotation::makeRotationByAxes(Vector3d(1.0, 0.0, 0.0), Vector3d(), Vector3d(), "XXY"), ValueError);
    EXPECT_THROW(Rotation::makeRotationByAxes(Vector3d(), Vector3d(), Vector3d(), "ZXY"), ValueError);
    EXPECT_THROW(Rotation::makeRotationByAxes(Vector3d(1.0, 0.0, 0.0), Vector3d(), Vector3d(), nullptr), ValueError);
}

/**
 * @brief 钉住：由起点方向与终点方向构造的最短旋转，含同向与反向两个退化分支
 */
TEST(RotationTest, RotateFromToHandlesParallelAndAntiparallelVectors)
{
    // 同向：单位旋转
    EXPECT_TRUE(Rotation(Vector3d(0.0, 0.0, 1.0), Vector3d(0.0, 0.0, 2.0)).isIdentity(Tolerance));

    // 反向：必然存在 180° 旋转把起点方向翻到终点方向
    const Rotation flip(Vector3d(1.0, 0.0, 0.0), Vector3d(-1.0, 0.0, 0.0));
    EXPECT_TRUE(flip.multiplyVector(Vector3d(1.0, 0.0, 0.0)).isEqual(Vector3d(-1.0, 0.0, 0.0), 1e-12));

    // 一般情形：终点方向可作为旋转轴外的验证点
    const Rotation rotate(Vector3d(1.0, 0.0, 0.0), Vector3d(0.0, 1.0, 0.0));
    EXPECT_TRUE(rotate.multiplyVector(Vector3d(1.0, 0.0, 0.0)).isEqual(Vector3d(0.0, 1.0, 0.0), 1e-12));
}

/**
 * @brief 钉住：四元数展开的旋转矩阵与多向量变换一致，矩阵构造能往返取回原旋转
 */
TEST(RotationTest, MatrixRoundTripKeepsRotation)
{
    const Rotation original(Vector3d(1.0, 2.0, 3.0), 0.7);
    Matrix4D       matrix;
    original.getValue(matrix);

    const Rotation fromMatrix(matrix);
    EXPECT_TRUE(fromMatrix.isSame(original, 1e-9));

    // 矩阵路径与四元数直接变换必须给出同一个像点
    const Vector3d point(0.5, -1.5, 2.5);
    EXPECT_TRUE((matrix * point).isEqual(original.multiplyVector(point), 1e-9));
}

/**
 * @brief 钉住：单精度变换与双精度变换在单精度容差内一致（multiplyVector 的 float 重载）
 */
TEST(RotationTest, FloatVectorOverloadMatchesDoublePath)
{
    const Rotation rotation(Vector3d(0.0, 1.0, 0.0), 0.6);
    const Vector3f source(1.0F, 2.0F, 3.0F);

    const Vector3f floatResult  = rotation.multiplyVector(source);
    const Vector3d doubleResult = rotation.multiplyVector(Vector3d(1.0, 2.0, 3.0));

    EXPECT_NEAR(floatResult.x, doubleResult.x, 1e-5);
    EXPECT_NEAR(floatResult.y, doubleResult.y, 1e-5);
    EXPECT_NEAR(floatResult.z, doubleResult.z, 1e-5);
}
