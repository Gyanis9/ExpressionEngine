#include <ExpressionEngine/Base/DualQuaternion.h>
#include <ExpressionEngine/Base/Exception.h>

#include <format>


namespace ExpressionEngine::Base
{
    namespace
    {
        /// 判定「无旋转」的实部向量长度阈值
        constexpr double NoRotationThreshold = 1e-12;

        /// 判定实部/对偶部参数是否为纯实四元数的容差
        constexpr double PureRealTolerance = 1e-12;
    } // namespace

    DualQuaternion operator+(const DualQuaternion &left, const DualQuaternion &right)
    {
        return {left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w};
    }

    DualQuaternion operator-(const DualQuaternion &left, const DualQuaternion &right)
    {
        return {left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w};
    }

    DualQuaternion operator*(const DualQuaternion &left, const DualQuaternion &right)
    {
        // 四元数乘法公式：分量本身是对偶数，ε 项的运算由 DualNumber 的运算符完成
        return {left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y, left.w * right.y + left.y * right.w + left.z * right.x - left.x * right.z,
                left.w * right.z + left.z * right.w + left.x * right.y - left.y * right.x, left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z};
    }

    DualQuaternion operator*(const DualQuaternion &left, const double right)
    {
        return {left.x * right, left.y * right, left.z * right, left.w * right};
    }

    DualQuaternion operator*(const double left, const DualQuaternion &right)
    {
        return {right.x * left, right.y * left, right.z * left, right.w * left};
    }

    DualQuaternion operator*(const DualQuaternion &left, const DualNumber right)
    {
        return {left.x * right, left.y * right, left.z * right, left.w * right};
    }

    DualQuaternion operator*(const DualNumber left, const DualQuaternion &right)
    {
        return {right.x * left, right.y * left, right.z * left, right.w * left};
    }

    DualQuaternion::DualQuaternion(const DualQuaternion &realPart, const DualQuaternion &dualPart) :
        x(realPart.x.real, dualPart.x.real), y(realPart.y.real, dualPart.y.real), z(realPart.z.real, dualPart.z.real), w(realPart.w.real, dualPart.w.real)
    {
        // 本构造只读两个参数的实部-实部：参数若带有对偶分量，那些分量会被静默丢弃，
        // 因此这里显式拒绝，要求调用方先用 real()/dual() 取出纯实四元数
        const double realPartDualLength = realPart.dual().length();
        const double dualPartDualLength = dualPart.dual().length();
        if (realPartDualLength >= PureRealTolerance || dualPartDualLength >= PureRealTolerance)
        {
            throw ValueError(std::format("DualQuaternion：由实部与对偶部构造时，两个参数都必须是纯实四元数（对偶分量全为零），"
                                         "但检测到实部的对偶分量长度为 {:g}、对偶部的为 {:g}；请先用 real() 或 dual() "
                                         "取出纯实四元数后再构造",
                                         realPartDualLength, dualPartDualLength));
        }
    }

    double DualQuaternion::dot(const DualQuaternion &left, const DualQuaternion &right)
    {
        // 只比较实部：旋转方向一致与否决定插值是否走短弧
        return left.x.real * right.x.real + left.y.real * right.y.real + left.z.real * right.z.real + left.w.real * right.w.real;
    }

    DualQuaternion DualQuaternion::pow(const double t, const bool shorten) const
    {
        // 算法出自 Ben Kenwright 的《Dual-Quaternions: From Classical Mechanics to Computer
        // Graphics and Beyond》：先换算到螺旋坐标，插值后再换算回四元数
        const double vectorLength = this->vector().length();
        if (vectorLength < NoRotationThreshold)
        {
            // 无旋转时归一化因子发散、算法失效：旋转保持恒等，只把平移按 t 线性缩放
            return {this->real(), this->dual() * t};
        }

        const double normalizeMultiplier = 1.0 / vectorLength;

        DualQuaternion self = *this;
        if (shorten)
        {
            // 实部点积为负说明走的是远弧，取反改为短弧；用负容差而非零，使 180° 附近的
            // 选择保持稳定
            if (dot(self, identity()) < -NoRotationThreshold)
            {
                self = -self;
            }
        }

        // 换算到螺旋坐标：rotationAngle 为转角、pitch 为沿轴位移，轴方向与力矩向量借 DualQuaternion 承载
        double               rotationAngle = self.rotationAngle();
        double               pitch         = -2.0 * self.w.dual * normalizeMultiplier;
        const DualQuaternion screwAxis     = self.real().vector() * normalizeMultiplier;
        const DualQuaternion screwMoment   = (self.dual().vector() - pitch / 2 * std::cos(rotationAngle / 2) * screwAxis) * normalizeMultiplier;

        // 螺旋坐标下插值：转角与沿轴位移同步按 t 缩放
        rotationAngle *= t;
        pitch         *= t;

        // 换算回四元数：实部为旋转，对偶部为沿螺旋轴的平移编码
        return {screwAxis * std::sin(rotationAngle / 2) + DualQuaternion(0.0, 0.0, 0.0, std::cos(rotationAngle / 2)),
                screwMoment * std::sin(rotationAngle / 2) + pitch / 2 * std::cos(rotationAngle / 2) * screwAxis +
                DualQuaternion(0.0, 0.0, 0.0, -pitch / 2 * std::sin(rotationAngle / 2))};
    }
} // namespace ExpressionEngine::Base
