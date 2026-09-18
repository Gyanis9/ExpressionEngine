#include <ExpressionEngine/Base/DualQuaternion.h>

#include <cmath>
#include <format>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Base
{
    namespace
    {
        /// 判定「无旋转」的实部向量长度阈值，与 FreeCAD DualQuat 保持一致
        constexpr double NoRotationThreshold = 1e-12;

        /// 判定实部/对偶部参数是否为纯实四元数的容差
        constexpr double PureRealTolerance = 1e-12;
    } // namespace

    DualQuat operator+(const DualQuat &left, const DualQuat &right)
    {
        return {left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w};
    }

    DualQuat operator-(const DualQuat &left, const DualQuat &right)
    {
        return {left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w};
    }

    DualQuat operator*(const DualQuat &left, const DualQuat &right)
    {
        // 四元数乘法公式：分量本身是对偶数，ε 项的运算由 DualNumber 的运算符完成
        return {left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y, left.w * right.y + left.y * right.w + left.z * right.x - left.x * right.z,
                left.w * right.z + left.z * right.w + left.x * right.y - left.y * right.x, left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z};
    }

    DualQuat operator*(const DualQuat &left, const double right)
    {
        return {left.x * right, left.y * right, left.z * right, left.w * right};
    }

    DualQuat operator*(const double left, const DualQuat &right)
    {
        return {right.x * left, right.y * left, right.z * left, right.w * left};
    }

    DualQuat operator*(const DualQuat &left, const DualNumber right)
    {
        return {left.x * right, left.y * right, left.z * right, left.w * right};
    }

    DualQuat operator*(const DualNumber left, const DualQuat &right)
    {
        return {right.x * left, right.y * left, right.z * left, right.w * left};
    }

    DualQuat::DualQuat(const DualQuat &realPart, const DualQuat &dualPart) :
        x(realPart.x.re, dualPart.x.re), y(realPart.y.re, dualPart.y.re), z(realPart.z.re, dualPart.z.re), w(realPart.w.re, dualPart.w.re)
    {
        // 本构造只读两个参数的实部-实部：参数若带有对偶分量，那些分量会被静默丢弃，
        // 因此这里显式拒绝，要求调用方先用 real()/dual() 取出纯实四元数
        const double realPartDualLength = realPart.dual().length();
        const double dualPartDualLength = dualPart.dual().length();
        if (realPartDualLength >= PureRealTolerance || dualPartDualLength >= PureRealTolerance)
        {
            throw ValueError(std::format("DualQuat：由实部与对偶部构造时，两个参数都必须是纯实四元数（对偶分量全为零），"
                                         "但检测到实部的对偶分量长度为 {:g}、对偶部的为 {:g}；请先用 real() 或 dual() "
                                         "取出纯实四元数后再构造",
                                         realPartDualLength, dualPartDualLength));
        }
    }

    double DualQuat::dot(const DualQuat &left, const DualQuat &right)
    {
        // 只比较实部：旋转方向一致与否决定插值是否走短弧
        return left.x.re * right.x.re + left.y.re * right.y.re + left.z.re * right.z.re + left.w.re * right.w.re;
    }

    DualQuat DualQuat::pow(const double t, const bool shorten) const
    {
        // 算法出自 Ben Kenwright 的《Dual-Quaternions: From Classical Mechanics to Computer
        // Graphics and Beyond》：先换算到螺旋坐标，插值后再换算回四元数
        const double vectorLength = this->vec().length();
        if (vectorLength < NoRotationThreshold)
        {
            // 无旋转时归一化因子发散、算法失效：旋转保持恒等，只把平移按 t 线性缩放
            return {this->real(), this->dual() * t};
        }

        const double normalizeMultiplier = 1.0 / vectorLength;

        DualQuat self = *this;
        if (shorten)
        {
            // 实部点积为负说明走的是远弧，取反改为短弧；用负容差而非零，使 180° 附近的
            // 选择保持稳定
            if (dot(self, identity()) < -NoRotationThreshold)
            {
                self = -self;
            }
        }

        // 换算到螺旋坐标：theta 为转角、pitch 为沿轴位移，轴方向与力矩向量借 DualQuat 承载
        double         theta       = self.theta();
        double         pitch       = -2.0 * self.w.du * normalizeMultiplier;
        const DualQuat screwAxis   = self.real().vec() * normalizeMultiplier;
        const DualQuat screwMoment = (self.dual().vec() - pitch / 2 * std::cos(theta / 2) * screwAxis) * normalizeMultiplier;

        // 螺旋坐标下插值：转角与沿轴位移同步按 t 缩放
        theta *= t;
        pitch *= t;

        // 换算回四元数：实部为旋转，对偶部为沿螺旋轴的平移编码
        return {screwAxis * std::sin(theta / 2) + DualQuat(0.0, 0.0, 0.0, std::cos(theta / 2)),
                screwMoment * std::sin(theta / 2) + pitch / 2 * std::cos(theta / 2) * screwAxis + DualQuat(0.0, 0.0, 0.0, -pitch / 2 * std::sin(theta / 2))};
    }
} // namespace ExpressionEngine::Base
