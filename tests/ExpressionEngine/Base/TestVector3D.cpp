#include <gtest/gtest.h>

#include <cmath>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Vector3D.h>

namespace
{

    using ExpressionEngine::Base::distance;
    using ExpressionEngine::Base::IndexError;
    using ExpressionEngine::Base::squaredDistance;
    using ExpressionEngine::Base::ValueError;
    using ExpressionEngine::Base::Vector3d;

} // namespace

/**
 * @brief 钉住默认构造、带值构造、容差相等比较与分量下标越界拒绝面
 */
TEST(Vector3D, ConstructionAndEquality)
{
    const Vector3d origin;
    EXPECT_DOUBLE_EQ(origin.x, 0.0);
    EXPECT_DOUBLE_EQ(origin.y, 0.0);
    EXPECT_DOUBLE_EQ(origin.z, 0.0);
    EXPECT_TRUE(origin.isNull());

    const Vector3d point(1.0, 2.0, 3.0);
    EXPECT_DOUBLE_EQ(point.x, 1.0);
    EXPECT_DOUBLE_EQ(point[0], 1.0);
    EXPECT_DOUBLE_EQ(point[2], 3.0);
    EXPECT_FALSE(point.isNull());

    // 相等按机器精度容差比较：相差 1 个 ULP 仍视为同一个点
    EXPECT_TRUE(point == Vector3d(std::nextafter(1.0, 2.0), 2.0, 3.0));
    EXPECT_TRUE(point != Vector3d(1.001, 2.0, 3.0));
    EXPECT_TRUE(Vector3d::UnitZ == Vector3d(0.0, 0.0, 1.0));

    // 越界下标必须显式报错，而不是静默返回首分量
    EXPECT_THROW((void) point[3], IndexError);

    EXPECT_DOUBLE_EQ(distance(Vector3d(0.0, 0.0, 0.0), Vector3d(3.0, 4.0, 0.0)), 5.0);
}

/**
 * @brief 钉住加减、标量乘除、点积与叉积的数值结果
 */
TEST(Vector3D, AddSubtractDotCross)
{
    const Vector3d first(1.0, 2.0, 3.0);
    const Vector3d second(4.0, 5.0, 6.0);

    EXPECT_TRUE((first + second) == Vector3d(5.0, 7.0, 9.0));
    EXPECT_TRUE((second - first) == Vector3d(3.0, 3.0, 3.0));
    EXPECT_TRUE((-first) == Vector3d(-1.0, -2.0, -3.0));
    EXPECT_TRUE((first * 2.0) == Vector3d(2.0, 4.0, 6.0));
    EXPECT_TRUE((2.0 * first) == Vector3d(2.0, 4.0, 6.0));
    EXPECT_TRUE((first / 2.0) == Vector3d(0.5, 1.0, 1.5));

    EXPECT_DOUBLE_EQ(first.dot(second), 32.0);
    EXPECT_DOUBLE_EQ(first * second, 32.0);

    // 右手系约定：X 叉乘 Y 得 Z，交换次序得反向
    EXPECT_TRUE((Vector3d::UnitX % Vector3d::UnitY) == Vector3d::UnitZ);
    EXPECT_TRUE(Vector3d::UnitY.cross(Vector3d::UnitX) == -Vector3d::UnitZ);
    EXPECT_TRUE(first.cross(second) == (first % second));
}

/**
 * @brief 钉住归一化的零向量拒绝面与正常归一化结果
 */
TEST(Vector3D, NormalizeRejectsZeroVector)
{
    const Vector3d vector(3.0, 0.0, 4.0);
    EXPECT_NEAR(vector.normalized().length(), 1.0, 1e-15);
    EXPECT_TRUE(vector.normalized() == Vector3d(0.6, 0.0, 0.8));

    Vector3d inPlace(3.0, 0.0, 4.0);
    inPlace.normalize();
    EXPECT_NEAR(inPlace.length(), 1.0, 1e-15);

    // 零向量没有方向：必须报错，不能静默返回原样
    Vector3d zeroVector;
    EXPECT_THROW(zeroVector.normalize(), ValueError);
    EXPECT_THROW((void) Vector3d().normalized(), ValueError);
}

/**
 * @brief 钉住零向量的夹角语义：getAngle 返回 NaN，平行/垂直判定返回 false
 */
TEST(Vector3D, ZeroVectorAngleIsNaN)
{
    const Vector3d zeroVector;
    EXPECT_TRUE(std::isnan(zeroVector.getAngle(Vector3d::UnitX)));
    EXPECT_FALSE(zeroVector.isParallel(Vector3d::UnitX, 1e-9));
    EXPECT_FALSE(zeroVector.isNormal(Vector3d::UnitX, 1e-9));

    EXPECT_TRUE(Vector3d::UnitX.isParallel(Vector3d::UnitX, 1e-9));
    EXPECT_TRUE(Vector3d::UnitX.isNormal(Vector3d::UnitY, 1e-9));
}

/**
 * @brief 钉住点到直线、线段与平面的距离、投影与线段包含判定
 */
TEST(Vector3D, DistancesAndProjections)
{
    const Vector3d point(2.0, 3.0, 4.0);
    EXPECT_DOUBLE_EQ(point.squaredLength(), 29.0);
    EXPECT_DOUBLE_EQ(squaredDistance(Vector3d(1.0, 0.0, 0.0), Vector3d(4.0, 0.0, 0.0)), 9.0);

    // 到无限长直线的距离只看方向，垂足落在线段内外都算
    EXPECT_DOUBLE_EQ(Vector3d(2.0, 3.0, 0.0).distanceToLine(Vector3d(), Vector3d(1.0, 0.0, 0.0)), 3.0);

    // 到线段的位移向量：垂足在线段内取垂足，落在外面取最近端点
    const Vector3d inside  = Vector3d(5.0, 1.0, 0.0).distanceToLineSegment(Vector3d(), Vector3d(10.0, 0.0, 0.0));
    const Vector3d outside = Vector3d(5.0, 1.0, 0.0).distanceToLineSegment(Vector3d(), Vector3d(2.0, 0.0, 0.0));
    EXPECT_TRUE(inside == Vector3d(0.0, -1.0, 0.0));
    EXPECT_TRUE(outside == Vector3d(-3.0, -1.0, 0.0));

    // 平面投影只去掉法向分量，距离按法向取
    Vector3d projected = point;
    projected.projectToPlane(Vector3d(), Vector3d(0.0, 0.0, 1.0));
    EXPECT_TRUE(projected == Vector3d(2.0, 3.0, 0.0));
    EXPECT_DOUBLE_EQ(point.distanceToPlane(Vector3d(), Vector3d(0.0, 0.0, 1.0)), 4.0);

    // 直线投影与平面投影同一条路：把本点投到「过 point、方向 line」的直线上，垂足就地写回
    Vector3d onLine(point);
    onLine.projectToLine(Vector3d(), Vector3d(1.0, 0.0, 0.0));
    EXPECT_TRUE(onLine == Vector3d(2.0, 0.0, 0.0));
    // 直线不过原点时以 point 为准
    Vector3d anchored = point;
    anchored.projectToLine(Vector3d(0.0, 1.0, 0.0), Vector3d(1.0, 0.0, 0.0));
    EXPECT_TRUE(anchored == Vector3d(2.0, 1.0, 0.0));
    EXPECT_TRUE(point.perpendicular(Vector3d(), Vector3d(1.0, 0.0, 0.0)) == Vector3d(2.0, 0.0, 0.0));

    // 线段包含判定是闭区间，偏出直线方向就不算
    EXPECT_TRUE(Vector3d(1.0, 0.0, 0.0).isOnLineSegment(Vector3d(), Vector3d(2.0, 0.0, 0.0)));
    EXPECT_TRUE(Vector3d(2.0, 0.0, 0.0).isOnLineSegment(Vector3d(), Vector3d(2.0, 0.0, 0.0)));
    EXPECT_FALSE(Vector3d(3.0, 0.0, 0.0).isOnLineSegment(Vector3d(), Vector3d(2.0, 0.0, 0.0)));
    EXPECT_FALSE(Vector3d(1.0, 1.0, 0.0).isOnLineSegment(Vector3d(), Vector3d(2.0, 0.0, 0.0)));

    // 带方向的夹角按法向定旋向，结果折进 [0, 2*pi)：反向不是负角而是 270°
    const double quarterTurn = std::asin(1.0);
    EXPECT_NEAR(Vector3d(1.0, 0.0, 0.0).getAngleOriented(Vector3d(0.0, 1.0, 0.0), Vector3d(0.0, 0.0, 1.0)), quarterTurn, 1e-12);
    EXPECT_NEAR(Vector3d(0.0, 1.0, 0.0).getAngleOriented(Vector3d(1.0, 0.0, 0.0), Vector3d(0.0, 0.0, 1.0)), 3.0 * quarterTurn, 1e-12);
    // 参考法向反过来，同一个转向就落到另一侧
    EXPECT_NEAR(Vector3d(1.0, 0.0, 0.0).getAngleOriented(Vector3d(0.0, 1.0, 0.0), Vector3d(0.0, 0.0, -1.0)), 3.0 * quarterTurn, 1e-12);
}
