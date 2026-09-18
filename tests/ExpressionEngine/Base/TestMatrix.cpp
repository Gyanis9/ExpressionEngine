#include <gtest/gtest.h>

#include <numbers>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Vector3D.h>

namespace {

using ExpressionEngine::Base::IndexError;
using ExpressionEngine::Base::Matrix4D;
using ExpressionEngine::Base::ValueError;
using ExpressionEngine::Base::Vector3d;

}  // namespace

/**
 * @brief 钉住默认构造为单位阵、16 元素构造、行列式与行下标越界拒绝面
 */
TEST(Matrix4D, ConstructionAndEquality) {
    const Matrix4D unity;
    EXPECT_TRUE(unity.isUnity());
    EXPECT_DOUBLE_EQ(unity[0][0], 1.0);
    EXPECT_DOUBLE_EQ(unity[0][1], 0.0);
    EXPECT_DOUBLE_EQ(unity[3][3], 1.0);
    EXPECT_FALSE(unity.isNull());

    const Matrix4D scaled(
        2.0, 0.0, 0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    EXPECT_FALSE(scaled.isUnity());
    EXPECT_DOUBLE_EQ(scaled[1][1], 3.0);
    EXPECT_DOUBLE_EQ(scaled.determinant(), 24.0);
    EXPECT_DOUBLE_EQ(scaled.determinant3(), 24.0);
    EXPECT_TRUE(scaled == scaled);
    EXPECT_TRUE(scaled != unity);

    Matrix4D nullified;
    nullified.nullify();
    EXPECT_TRUE(nullified.isNull());

    // 行下标越界必须显式报错，而不是越界读内存
    EXPECT_THROW(scaled[4], IndexError);
}

/**
 * @brief 钉住矩阵乘法、矩阵与向量变换以及平移与缩放的组合次序
 */
TEST(Matrix4D, MultiplyAndVectorTransform) {
    const Matrix4D unity;
    const Matrix4D scaled(
        2.0, 0.0, 0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    // 单位阵是乘法单位元
    EXPECT_TRUE((unity * scaled) == scaled);
    EXPECT_TRUE((scaled * unity) == scaled);

    Matrix4D translation;
    translation.move(Vector3d(1.0, 2.0, 3.0));
    EXPECT_TRUE((translation * Vector3d(1.0, 1.0, 1.0)) == Vector3d(2.0, 3.0, 4.0));

    // 左乘语义：translation * scaled 表示先缩放再平移
    const Matrix4D combined = translation * scaled;
    const Vector3d transformed = combined * Vector3d(1.0, 1.0, 1.0);
    EXPECT_TRUE(transformed == Vector3d(3.0, 5.0, 7.0));

    // multVec 就地变换与 operator* 结果一致
    Vector3d inPlace(1.0, 1.0, 1.0);
    combined.multVec(inPlace, inPlace);
    EXPECT_TRUE(inPlace == transformed);

    EXPECT_DOUBLE_EQ(scaled.trace3(), 9.0);
    EXPECT_DOUBLE_EQ(scaled.trace(), 10.0);
    EXPECT_TRUE(scaled.diagonal() == Vector3d(2.0, 3.0, 4.0));
}

/**
 * @brief 钉住刚体逆、一般矩阵求逆与奇异矩阵的拒绝面
 */
TEST(Matrix4D, InverseAndSingularRejection) {
    // 刚体变换：inverse() 利用旋转正交性给出精确的逆
    Matrix4D rigid;
    rigid.rotZ(std::numbers::pi / 2.0);
    rigid.move(Vector3d(1.0, 2.0, 0.0));
    Matrix4D rigidInverse = rigid;
    rigidInverse.inverse();
    EXPECT_TRUE((rigid * rigidInverse).isUnity(1e-12));

    // 一般可逆矩阵：inverseGauss 后与原矩阵相乘应得到单位阵
    const Matrix4D general(
        2.0, 0.0, 0.0, 1.0, 0.0, 3.0, 0.0, 2.0, 0.0, 0.0, 4.0, 3.0, 0.0, 0.0, 0.0, 1.0);
    Matrix4D generalInverse = general;
    generalInverse.inverseGauss();
    EXPECT_TRUE((general * generalInverse).isUnity(1e-12));

    // 秩亏矩阵（前两行相同）必须抛出 ValueError，而不是返回不可信的逆
    const Matrix4D singular(
        1.0, 2.0, 3.0, 4.0, 1.0, 2.0, 3.0, 4.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    Matrix4D rejected = singular;
    EXPECT_THROW(rejected.inverseGauss(), ValueError);

    // 零矩阵同样是奇异矩阵
    Matrix4D zeroMatrix;
    zeroMatrix.nullify();
    EXPECT_THROW(zeroMatrix.inverseGauss(), ValueError);
}
