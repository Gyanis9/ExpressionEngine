// 覆盖 Placement 的关键行为：恒等位姿、multiplyVector 的「先旋转后平移」语义、复合顺序与取逆、
// 矩阵与对偶四元数往返、pow/sclerp 的螺旋插值，以及 DualQuaternion 的取值与拒绝面。

#include <gtest/gtest.h>

#include <numbers>

#include <ExpressionEngine/Base/DualQuaternion.h>
#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Placement.h>
#include <ExpressionEngine/Base/Rotation.h>

using ExpressionEngine::Base::DualQuaternion;
using ExpressionEngine::Base::Matrix4D;
using ExpressionEngine::Base::Placement;
using ExpressionEngine::Base::Rotation;
using ExpressionEngine::Base::ValueError;
using ExpressionEngine::Base::Vector3d;
using ExpressionEngine::Base::Vector3f;

namespace
{

    /// 旋转与四元数比较用的通用容差
    constexpr double Tolerance = 1e-12;

    /// 绕 Z 轴四分之一圈，多个用例复用
    constexpr double QuarterTurn = std::numbers::pi / 2.0;

    /// 测试辅助：按出参形式调用 multiplyVector，返回像点便于直接断言
    Vector3d applyPlacement(const Placement &placement, const Vector3d &point)
    {
        Vector3d destination;
        placement.multiplyVector(point, destination);
        return destination;
    }

} // namespace

/**
 * @brief 钉住：默认构造是恒等位姿（零位置 + 单位旋转），且 isIdentity 的容差版本一致
 */
TEST(PlacementTest, DefaultConstructorIsIdentity)
{
    const Placement placement;
    EXPECT_TRUE(placement.isIdentity());
    EXPECT_TRUE(placement.isIdentity(Tolerance));
    EXPECT_TRUE(placement.getPosition().isEqual(Vector3d(0.0, 0.0, 0.0), 0.0));
    EXPECT_TRUE(placement.getRotation().isIdentity());
    EXPECT_TRUE(placement.toMatrix().isUnity(1e-12));

    // 恒等位姿不改变任何点
    const Vector3d point(1.0, 2.0, 3.0);
    Vector3d       destination;
    placement.multiplyVector(point, destination);
    EXPECT_TRUE(destination.isEqual(point, Tolerance));
}

/**
 * @brief 钉住：multiplyVector 等价于「先旋转再加位置」，以旋转中心构造时该中心落到 position + center
 */
TEST(PlacementTest, MultVecAppliesRotationThenTranslation)
{
    const Rotation  rotation(Vector3d(0.0, 0.0, 1.0), QuarterTurn);
    const Placement placement(Vector3d(1.0, 2.0, 3.0), rotation);

    const Vector3d point(1.0, 0.0, 0.0);
    // 绕 Z 转 90° 得 (0,1,0)，再加位置 (1,2,3)
    EXPECT_TRUE(applyPlacement(placement, point).isEqual(Vector3d(1.0, 3.0, 3.0), Tolerance));

    // 单精度重载给出同一结果
    Vector3f floatDestination;
    placement.multiplyVector(Vector3f(1.0F, 0.0F, 0.0F), floatDestination);
    EXPECT_NEAR(floatDestination.x, 1.0F, 1e-5F);
    EXPECT_NEAR(floatDestination.y, 3.0F, 1e-5F);
    EXPECT_NEAR(floatDestination.z, 3.0F, 1e-5F);

    // 以 center 为旋转中心：center 自身是不动点（除 position 外不再有别的位移）
    const Vector3d  center(0.0, 5.0, 0.0);
    const Placement aboutCenter(Vector3d(0.0, 0.0, 0.0), rotation, center);
    EXPECT_TRUE(applyPlacement(aboutCenter, center).isEqual(center, 1e-12));
}

/**
 * @brief 钉住：复合按「右操作数先作用」的约定，逆位姿与自身复合回到恒等，multiplyLeft 顺序相反
 */
TEST(PlacementTest, CompositionAppliesRightOperandFirstAndInverseUndoesIt)
{
    const Rotation  rotation(Vector3d(0.0, 0.0, 1.0), QuarterTurn);
    const Placement first(Vector3d(1.0, 0.0, 0.0), rotation);              // 先转 90°，再平移 (1,0,0)
    const Placement second(Vector3d(0.0, 2.0, 0.0), Rotation::identity()); // 只平移 (0,2,0)
    const Vector3d  point(1.0, 0.0, 0.0);

    const Placement product = first * second;
    // 等价于逐个施加：second 的平移先发生
    EXPECT_TRUE(applyPlacement(product, point).isEqual(applyPlacement(first, applyPlacement(second, point)), Tolerance));
    EXPECT_TRUE(applyPlacement(product, point).isEqual(Vector3d(-1.0, 1.0, 0.0), 1e-12));

    // 逆：把像点送回原点，且与自身复合得到恒等位姿
    const Vector3d image = applyPlacement(product, point);
    EXPECT_TRUE(applyPlacement(product.inverse(), image).isEqual(point, 1e-12));
    EXPECT_TRUE((product * product.inverse()).isIdentity(1e-12));
    EXPECT_TRUE(product.inverse().inverse().isSame(product, 1e-12));

    // multiplyLeft 等价于交换顺序的乘积
    Placement leftProduct = first;
    leftProduct.multiplyLeft(second);
    EXPECT_TRUE(leftProduct.isSame(second * first, 1e-12));
    EXPECT_TRUE(applyPlacement(leftProduct, point).isEqual(Vector3d(1.0, 3.0, 0.0), 1e-12));

    // 精确比较与取反：不同位姿不相等
    EXPECT_TRUE(Placement() == Placement());
    EXPECT_TRUE(product != Placement());
}

/**
 * @brief 钉住：toMatrix 的平移列即位置，fromMatrix 能原样取回，矩阵变换与 multiplyVector 一致
 */
TEST(PlacementTest, MatrixRoundTripKeepsPositionAndRotation)
{
    const Rotation  rotation(Vector3d(1.0, 2.0, 3.0), 0.7);
    const Placement original(Vector3d(1.5, -2.5, 3.5), rotation);

    const Matrix4D matrix = original.toMatrix();
    EXPECT_DOUBLE_EQ(matrix[0][3], 1.5) << "第四列应为位置分量";
    EXPECT_DOUBLE_EQ(matrix[1][3], -2.5);
    EXPECT_DOUBLE_EQ(matrix[2][3], 3.5);

    const Placement roundTrip(matrix);
    EXPECT_TRUE(roundTrip.isSame(original, 1e-9));

    // 矩阵路径与 multiplyVector 必须给出同一个像点
    const Vector3d point(0.5, 1.0, -1.5);
    EXPECT_TRUE((matrix * point).isEqual(applyPlacement(original, point), 1e-9));
}

/**
 * @brief 钉住：toDualQuaternion 与 fromDualQuaternion 往返一致，move 在全局系累加平移
 */
TEST(PlacementTest, DualQuaternionRoundTripAndMove)
{
    const Rotation  rotation(Vector3d(0.0, 0.0, 1.0), QuarterTurn);
    const Placement original(Vector3d(1.0, 2.0, 3.0), rotation);

    const DualQuaternion asDualQuaternion = original.toDualQuaternion();
    const Placement      back             = Placement::fromDualQuaternion(asDualQuaternion);
    EXPECT_TRUE(back.isSame(original, 1e-12));

    // 实部即旋转四元数，长度应为 1
    EXPECT_NEAR(asDualQuaternion.length(), 1.0, Tolerance);

    Placement moved(original);
    moved.move(Vector3d(0.0, 1.0, 0.0));
    // move 不经过旋转，直接在全局系累加
    EXPECT_TRUE(moved.getPosition().isEqual(Vector3d(1.0, 3.0, 3.0), Tolerance));
    EXPECT_TRUE(moved.getRotation().isSame(rotation, Tolerance));
}

/**
 * @brief 钉住：pow 的端点语义（t=0 恒等、t=1 原位姿），以及螺旋插值下平移沿螺旋轴按比例推进
 */
TEST(PlacementTest, PowInterpolatesAlongScrewMotion)
{
    // 平移与转轴同向：半程应得半程平移与半角旋转
    const Rotation  rotation(Vector3d(0.0, 0.0, 1.0), QuarterTurn);
    const Placement original(Vector3d(0.0, 0.0, 2.0), rotation);

    EXPECT_TRUE(original.pow(0.0).isIdentity(1e-12));
    EXPECT_TRUE(original.pow(1.0).isSame(original, 1e-9));

    const Placement half = original.pow(0.5);
    EXPECT_TRUE(half.getPosition().isEqual(Vector3d(0.0, 0.0, 1.0), 1e-9)) << "沿螺旋轴的平移应线性推进";
    EXPECT_TRUE(half.getRotation().isSame(Rotation(Vector3d(0.0, 0.0, 1.0), std::numbers::pi / 4.0), 1e-9));

    // 无旋转时退化为平移的线性插值
    const Placement pureTranslation(Vector3d(0.0, 0.0, 2.0), Rotation::identity());
    EXPECT_TRUE(pureTranslation.pow(0.5).getPosition().isEqual(Vector3d(0.0, 0.0, 1.0), 1e-12));
}

/**
 * @brief 钉住：slerp 端点即端点、位置线性；sclerp 端点即端点且与 pow 的螺旋路径一致
 */
TEST(PlacementTest, SlerpAndSclerpHitEndpoints)
{
    const Placement start;
    const Placement end(Vector3d(2.0, 0.0, 0.0), Rotation(Vector3d(0.0, 0.0, 1.0), QuarterTurn));

    EXPECT_TRUE(Placement::slerp(start, end, 0.0).isSame(start, Tolerance));
    EXPECT_TRUE(Placement::slerp(start, end, 1.0).isSame(end, Tolerance));
    const Placement slerpHalf = Placement::slerp(start, end, 0.5);
    EXPECT_TRUE(slerpHalf.getPosition().isEqual(Vector3d(1.0, 0.0, 0.0), Tolerance));
    EXPECT_TRUE(slerpHalf.getRotation().isSame(Rotation(Vector3d(0.0, 0.0, 1.0), std::numbers::pi / 4.0), 1e-9));

    EXPECT_TRUE(Placement::sclerp(start, end, 0.0).isIdentity(1e-12));
    EXPECT_TRUE(Placement::sclerp(start, end, 1.0).isSame(end, 1e-9));
    // sclerp 内部就是「相对位姿取 pow」，两者结果应一致
    EXPECT_TRUE(Placement::sclerp(start, end, 0.25).isSame(end.pow(0.25), 1e-9));
}

/**
 * @brief 钉住：DualQuaternion 的取值与基本量，以及 (real, dual) 构造拒绝含非零对偶分量的参数
 */
TEST(DualQuaternionTest, ValuesAndPurityRejection)
{
    const DualQuaternion identity = DualQuaternion::identity();
    EXPECT_DOUBLE_EQ(identity.length(), 1.0);
    EXPECT_DOUBLE_EQ(identity.rotationAngle(), 0.0);
    EXPECT_DOUBLE_EQ(identity.w.real, 1.0);

    const DualQuaternion negated = -DualQuaternion(1.0, 2.0, 3.0, 4.0);
    EXPECT_DOUBLE_EQ(negated.x.real, -1.0);
    EXPECT_DOUBLE_EQ(negated.w.real, -4.0);

    // 共轭只取反向量部分
    const DualQuaternion conjugate = DualQuaternion(1.0, 2.0, 3.0, 4.0).conjugate();
    EXPECT_DOUBLE_EQ(conjugate.x.real, -1.0);
    EXPECT_DOUBLE_EQ(conjugate.w.real, 4.0);

    // real()/dual() 只取一层，用于把对偶四元数拆成两个纯实四元数
    const DualQuaternion mixed(0.0, 0.0, 0.0, 1.0, 0.5, 0.0, 0.0, 0.0);
    EXPECT_DOUBLE_EQ(mixed.real().x.dual, 0.0);
    EXPECT_DOUBLE_EQ(mixed.dual().x.real, 0.5);

    // 拒绝面：实部参数带非零对偶分量时必须抛错，而不是静默丢弃那些分量
    EXPECT_THROW(static_cast<void>(DualQuaternion(mixed, DualQuaternion())), ValueError);
    EXPECT_THROW(static_cast<void>(DualQuaternion(DualQuaternion::identity(), mixed)), ValueError);
    EXPECT_NO_THROW(static_cast<void>(DualQuaternion(mixed.real(), mixed.dual())));
}
