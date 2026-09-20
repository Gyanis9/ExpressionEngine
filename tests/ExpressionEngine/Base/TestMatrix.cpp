#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <numbers>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Vector3D.h>

namespace
{

    using ExpressionEngine::Base::IndexError;
    using ExpressionEngine::Base::Matrix4D;
    using ExpressionEngine::Base::ValueError;
    using ExpressionEngine::Base::Vector3d;
    using ExpressionEngine::Base::Vector3f;

} // namespace

/**
 * @brief 钉住默认构造为单位阵、16 元素构造、行列式与行下标越界拒绝面
 */
TEST(Matrix4D, ConstructionAndEquality)
{
    const Matrix4D unity;
    EXPECT_TRUE(unity.isUnity());
    EXPECT_DOUBLE_EQ(unity[0][0], 1.0);
    EXPECT_DOUBLE_EQ(unity[0][1], 0.0);
    EXPECT_DOUBLE_EQ(unity[3][3], 1.0);
    EXPECT_FALSE(unity.isNull());

    const Matrix4D scaled(2.0, 0.0, 0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 0.0, 1.0);
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
TEST(Matrix4D, MultiplyAndVectorTransform)
{
    const Matrix4D unity;
    const Matrix4D scaled(2.0, 0.0, 0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    // 单位阵是乘法单位元
    EXPECT_TRUE((unity * scaled) == scaled);
    EXPECT_TRUE((scaled * unity) == scaled);

    Matrix4D translation;
    translation.move(Vector3d(1.0, 2.0, 3.0));
    EXPECT_TRUE((translation * Vector3d(1.0, 1.0, 1.0)) == Vector3d(2.0, 3.0, 4.0));

    // 左乘语义：translation * scaled 表示先缩放再平移
    const Matrix4D combined    = translation * scaled;
    const Vector3d transformed = combined * Vector3d(1.0, 1.0, 1.0);
    EXPECT_TRUE(transformed == Vector3d(3.0, 5.0, 7.0));

    // multiplyVector 就地变换与 operator* 结果一致
    Vector3d inPlace(1.0, 1.0, 1.0);
    combined.multiplyVector(inPlace, inPlace);
    EXPECT_TRUE(inPlace == transformed);

    EXPECT_DOUBLE_EQ(scaled.trace3(), 9.0);
    EXPECT_DOUBLE_EQ(scaled.trace(), 10.0);
    EXPECT_TRUE(scaled.diagonal() == Vector3d(2.0, 3.0, 4.0));
}

/**
 * @brief 钉住刚体逆、一般矩阵求逆与奇异矩阵的拒绝面
 */
TEST(Matrix4D, InverseAndSingularRejection)
{
    // 刚体变换：inverse() 利用旋转正交性给出精确的逆
    Matrix4D rigid;
    rigid.rotateZ(std::numbers::pi / 2.0);
    rigid.move(Vector3d(1.0, 2.0, 0.0));
    Matrix4D rigidInverse = rigid;
    rigidInverse.inverse();
    EXPECT_TRUE((rigid * rigidInverse).isUnity(1e-12));

    // 一般可逆矩阵：inverseGauss 后与原矩阵相乘应得到单位阵
    const Matrix4D general(2.0, 0.0, 0.0, 1.0, 0.0, 3.0, 0.0, 2.0, 0.0, 0.0, 4.0, 3.0, 0.0, 0.0, 0.0, 1.0);
    Matrix4D       generalInverse = general;
    generalInverse.inverseGauss();
    EXPECT_TRUE((general * generalInverse).isUnity(1e-12));

    // 秩亏矩阵（前两行相同）必须抛出 ValueError，而不是返回不可信的逆
    const Matrix4D singular(1.0, 2.0, 3.0, 4.0, 1.0, 2.0, 3.0, 4.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    Matrix4D       rejected = singular;
    EXPECT_THROW(rejected.inverseGauss(), ValueError);

    // 零矩阵同样是奇异矩阵
    Matrix4D zeroMatrix;
    zeroMatrix.nullify();
    EXPECT_THROW(zeroMatrix.inverseGauss(), ValueError);
}

/**
 * @brief 钉住 float 入口与 double 入口给出同一个变换
 * @details 这些重载只把分量换成 double 再走同一条路，因此选 float 能精确表示的数值，
 *          旋转角取 0 以避开三角函数的精度差，两侧就应当逐位一致。
 */
TEST(Matrix4D, FloatOverloadsMatchDoubleOnes)
{
    Matrix4D byDouble;
    byDouble.move(Vector3d(1.5, -2.25, 3.0));
    byDouble.scale(Vector3d(2.0, 0.5, 4.0));

    Matrix4D byFloat;
    byFloat.move(Vector3f(1.5f, -2.25f, 3.0f));
    byFloat.scale(Vector3f(2.0f, 0.5f, 4.0f));
    EXPECT_TRUE(byFloat == byDouble);

    Matrix4D rotatedDouble;
    rotatedDouble.rotateLine(Vector3d(0.0, 0.0, 1.0), 0.0);
    Matrix4D rotatedFloat;
    rotatedFloat.rotateLine(Vector3f(0.0f, 0.0f, 1.0f), 0.0f);
    EXPECT_TRUE(rotatedFloat == rotatedDouble);

    // 三元组构造与向量变换同样各有 float 入口
    const Matrix4D constructed(Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f), 0.0f);
    EXPECT_TRUE(constructed == Matrix4D());
    EXPECT_TRUE((Matrix4D() * Vector3f(1.0f, 2.0f, 3.0f)) == Vector3f(1.0f, 2.0f, 3.0f));
}

namespace
{

    using ExpressionEngine::Base::ScaleType;

    /// 逐元素比较两个矩阵，容差覆盖分解与求逆的数值误差
    void expectClose(const Matrix4D &actual, const Matrix4D &expected, const double tolerance = 1e-9)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                EXPECT_NEAR(actual[row][column], expected[row][column], tolerance) << row << ',' << column;
            }
        }
    }

} // namespace

/**
 * @brief 钉住 OpenGL 数组是列主序，平移落在 12..14
 */
TEST(Matrix4D, OpenGlMatrixUsesColumnMajorOrder)
{
    Matrix4D moved;
    moved.move(Vector3d(1.0, 2.0, 3.0));

    double values[16] = {};
    moved.getOpenGlMatrix(values);
    EXPECT_DOUBLE_EQ(values[12], 1.0);
    EXPECT_DOUBLE_EQ(values[13], 2.0);
    EXPECT_DOUBLE_EQ(values[14], 3.0);
    // 对角元仍在 0、5、10、15 上，没有被整体转置
    EXPECT_DOUBLE_EQ(values[0], 1.0);
    EXPECT_DOUBLE_EQ(values[5], 1.0);
    EXPECT_DOUBLE_EQ(values[3], 0.0);

    Matrix4D restored;
    restored.setOpenGlMatrix(values);
    EXPECT_TRUE(restored == moved);
}

/**
 * @brief 钉住缩放类型的判定：单位阵与旋转没有缩放，对角缩放按左右侧区分
 */
TEST(Matrix4D, ScaleTypeClassification)
{
    EXPECT_EQ(Matrix4D().hasScale(), ScaleType::NoScaling);

    Matrix4D rotated;
    rotated.rotateZ(std::asin(1.0));
    EXPECT_EQ(rotated.hasScale(), ScaleType::NoScaling);

    Matrix4D uniform;
    uniform.scale(2.0);
    EXPECT_EQ(uniform.hasScale(), ScaleType::Uniform);

    Matrix4D stretched;
    stretched.scale(Vector3d(2.0, 1.0, 1.0));
    EXPECT_EQ(stretched.hasScale(), ScaleType::NonUniformLeft);
}

/**
 * @brief 钉住分解出的四个因子按 move * rotation * scale * shear 乘回原矩阵
 */
TEST(Matrix4D, DecomposeMultipliesBackToTheOriginal)
{
    Matrix4D transform;
    transform.scale(Vector3d(2.0, 3.0, 4.0));
    Matrix4D rotated;
    rotated.rotateX(std::asin(1.0) * 0.5);
    transform = rotated * transform;
    transform.move(Vector3d(1.0, -2.0, 0.5));

    const std::array<Matrix4D, 4> parts = transform.decompose();
    // 返回顺序是剪切、缩放、旋转、平移
    expectClose(parts[3] * parts[2] * parts[1] * parts[0], transform);
    // 平移分量整个落在 move 因子上
    EXPECT_DOUBLE_EQ(parts[3].getCol(3).x, 1.0);
    EXPECT_DOUBLE_EQ(parts[3].getCol(3).y, -2.0);
    EXPECT_DOUBLE_EQ(parts[3].getCol(3).z, 0.5);
}

/**
 * @brief 钉住正交求逆与高斯求逆一致，且逆乘回去是单位阵
 */
TEST(Matrix4D, InverseOrthogonalMatchesGauss)
{
    Matrix4D transform;
    transform.rotateZ(std::asin(1.0));
    transform.move(Vector3d(1.0, -2.0, 0.5));

    Matrix4D orthogonalInverse = transform;
    orthogonalInverse.inverseOrthogonal();

    Matrix4D gaussInverse = transform;
    gaussInverse.inverseGauss();

    expectClose(orthogonalInverse, gaussInverse);
    EXPECT_TRUE((transform * orthogonalInverse).isUnity(1e-9));
}

/**
 * @brief 钉住行列读写与对角设置都落在同一格上
 */
TEST(Matrix4D, RowAndColumnAccessors)
{
    Matrix4D matrix;
    // setDiagonal 只改 3x3 部分，齐次分量保持单位阵的 1
    matrix.setDiagonal(Vector3d(2.0, 2.0, 3.0));
    EXPECT_DOUBLE_EQ(matrix[0][0], 2.0);
    EXPECT_DOUBLE_EQ(matrix[2][2], 3.0);
    EXPECT_DOUBLE_EQ(matrix[3][3], 1.0);
    EXPECT_DOUBLE_EQ(matrix[0][1], 0.0);

    matrix.setRow(0, Vector3d(1.0, 2.0, 3.0));
    EXPECT_TRUE(matrix.getRow(0) == Vector3d(1.0, 2.0, 3.0));
    matrix.setCol(0, Vector3d(4.0, 5.0, 6.0));
    EXPECT_TRUE(matrix.getCol(0) == Vector3d(4.0, 5.0, 6.0));

    // 行列式与子行列式按当前内容计算
    const Matrix4D unity;
    EXPECT_DOUBLE_EQ(unity.determinant(), 1.0);
    EXPECT_DOUBLE_EQ(unity.determinant3(), 1.0);
}

/**
 * @brief 钉住变换类型的文字分析与矩阵文本往返
 */
TEST(Matrix4D, AnalyseNamesTheTransformKind)
{
    EXPECT_EQ(Matrix4D().analyse(), "Unity Matrix");

    Matrix4D scaled;
    scaled.scale(Vector3d(2.0, 3.0, 4.0));
    EXPECT_EQ(scaled.analyse(), "Scale [2, 3, 4]");

    Matrix4D rotated;
    rotated.rotateZ(std::asin(1.0));
    EXPECT_EQ(rotated.analyse(), "Rotation Matrix");

    Matrix4D movedRotated = rotated;
    movedRotated.move(Vector3d(1.0, 0.0, 0.0));
    EXPECT_EQ(movedRotated.analyse(), "Rotation Matrix with Translation");

    // 剪切不是正交变换，落到仿射分支并给出行列式
    Matrix4D sheared;
    sheared.setRow(0, Vector3d(1.0, 1.0, 0.0));
    EXPECT_EQ(sheared.analyse(), "Affine with det= 1");
}

/**
 * @brief 钉住 toString 与 fromString 互为逆运算，读不下的文本报错
 */
TEST(Matrix4D, TextRoundTripsAndRejectsGarbage)
{
    Matrix4D source;
    source.scale(Vector3d(2.0, 3.0, 4.0));
    source.move(Vector3d(5.0, -6.0, 7.0));

    Matrix4D restored;
    restored.fromString(source.toString());
    EXPECT_TRUE(restored == source);

    Matrix4D broken;
    EXPECT_THROW(broken.fromString("1 2 3"), ValueError);
}

/**
 * @brief 钉住反对称阵与并矢积只填左上 3x3，齐次分量保持单位阵
 */
TEST(Matrix4D, HatAndOuterFillOnlyTheThreeByThreeBlock)
{
    const Matrix4D crossOperator = Matrix4D().hat(Vector3d(0.0, 0.0, 1.0));
    // [z]x * x = z × x = y
    EXPECT_TRUE((crossOperator * Vector3d(1.0, 0.0, 0.0)) == Vector3d(0.0, 1.0, 0.0));
    EXPECT_DOUBLE_EQ(crossOperator[3][3], 1.0);
    // 与自身叉积为零
    EXPECT_TRUE((crossOperator * Vector3d(0.0, 0.0, 2.0)) == Vector3d());

    const Matrix4D dyadic = Matrix4D().outer(Vector3d(1.0, 0.0, 0.0), Vector3d(0.0, 1.0, 0.0));
    EXPECT_DOUBLE_EQ(dyadic[0][1], 1.0);
    EXPECT_DOUBLE_EQ(dyadic[1][1], 0.0);
    EXPECT_DOUBLE_EQ(dyadic[3][3], 1.0);

    EXPECT_EQ(Matrix4D::getMemSpace(), static_cast<unsigned long>(sizeof(Matrix4D)));
}
