#include <ExpressionEngine/Base/Vector3D.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Base
{
    template<class float_type>
    Vector3<float_type>::Vector3(float_type xValue, float_type yValue, float_type zValue) : x(xValue), y(yValue), z(zValue)
    {
    }

    template<class float_type>
    float_type &Vector3<float_type>::operator[](unsigned short index)
    {
        // 三分量在内存中连续排布，用 switch 让编译器折叠成一次地址偏移
        switch (index)
        {
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            default:
                // 越界下标若静默返回 x，会把调用方的下标错误写成数据损坏，必须显式报错
                throw IndexError("向量分量下标越界（只允许 0/1/2）；请改下标，或用 x/y/z 直接访问对应分量");
        }
    }

    template<class float_type>
    const float_type &Vector3<float_type>::operator[](unsigned short index) const
    {
        switch (index)
        {
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            default:
                throw IndexError("向量分量下标越界（只允许 0/1/2）；请改下标，或用 x/y/z 直接访问对应分量");
        }
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::operator+(const Vector3<float_type> &other) const
    {
        return Vector3<float_type>(x + other.x, y + other.y, z + other.z);
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::operator&(const Vector3<float_type> &other) const
    {
        // 逐分量伸缩：另一向量的分量取绝对值后作为缩放因子，允许传带符号方向
        return Vector3<float_type>(x * std::abs(other.x), y * std::abs(other.y), z * std::abs(other.z));
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::operator-(const Vector3<float_type> &other) const
    {
        return Vector3<float_type>(x - other.x, y - other.y, z - other.z);
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::operator-() const
    {
        return Vector3<float_type>(-x, -y, -z);
    }

    template<class float_type>
    Vector3<float_type> &Vector3<float_type>::operator+=(const Vector3<float_type> &other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    template<class float_type>
    Vector3<float_type> &Vector3<float_type>::operator-=(const Vector3<float_type> &other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    template<class float_type>
    Vector3<float_type> &Vector3<float_type>::operator*=(float_type scale)
    {
        x *= scale;
        y *= scale;
        z *= scale;
        return *this;
    }

    template<class float_type>
    Vector3<float_type> &Vector3<float_type>::operator/=(float_type divisor)
    {
        x /= divisor;
        y /= divisor;
        z /= divisor;
        return *this;
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::operator*(float_type scale) const
    {
        return Vector3<float_type>(x * scale, y * scale, z * scale);
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::operator/(float_type divisor) const
    {
        return Vector3<float_type>(x / divisor, y / divisor, z / divisor);
    }

    template<class float_type>
    float_type Vector3<float_type>::operator*(const Vector3<float_type> &other) const
    {
        return (x * other.x) + (y * other.y) + (z * other.z);
    }

    template<class float_type>
    float_type Vector3<float_type>::Dot(const Vector3<float_type> &other) const
    {
        return (x * other.x) + (y * other.y) + (z * other.z);
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::operator%(const Vector3<float_type> &other) const
    {
        return Vector3<float_type>((y * other.z) - (z * other.y), (z * other.x) - (x * other.z), (x * other.y) - (y * other.x));
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::Cross(const Vector3<float_type> &other) const
    {
        return Vector3<float_type>((y * other.z) - (z * other.y), (z * other.x) - (x * other.z), (x * other.y) - (y * other.x));
    }

    template<class float_type>
    bool Vector3<float_type>::IsOnLineSegment(const Vector3<float_type> &startPoint, const Vector3<float_type> &endPoint) const
    {
        const Vector3<float_type> segment      = endPoint - startPoint;
        const Vector3<float_type> toPoint      = *this - startPoint;
        const Vector3<float_type> crossProduct = segment.Cross(toPoint);
        const float_type          dotProduct   = segment.Dot(toPoint);

        // 叉积长度非零说明本点偏离线段所在直线
        if (crossProduct.Length() > traits_type::epsilon())
        {
            return false;
        }

        // 点积为负说明本点落在起点之前
        if (dotProduct < 0)
        {
            return false;
        }

        // 点积超过线段长度平方说明本点落在终点之后
        if (dotProduct > segment.Sqr())
        {
            return false;
        }

        return true;
    }

    template<class float_type>
    bool Vector3<float_type>::operator!=(const Vector3<float_type> &other) const
    {
        return !((*this) == other);
    }

    template<class float_type>
    bool Vector3<float_type>::operator==(const Vector3<float_type> &other) const
    {
        // 以机器精度为容差逐分量比较：浮点运算的舍入误差不应被当成不相等
        return (std::abs(x - other.x) <= traits_type::epsilon()) && (std::abs(y - other.y) <= traits_type::epsilon()) && (std::abs(z - other.z) <= traits_type::epsilon());
    }

    template<class float_type>
    bool Vector3<float_type>::IsEqual(const Vector3<float_type> &point, float_type tolerance) const
    {
        return Distance(*this, point) <= tolerance;
    }

    template<class float_type>
    bool Vector3<float_type>::IsParallel(const Vector3<float_type> &direction, float_type tolerance) const
    {
        const float_type angle = GetAngle(direction);
        // 零向量的夹角是 NaN：无法定义「平行」，按文档返回 false
        if (std::isnan(angle))
        {
            return false;
        }

        return angle <= tolerance || traits_type::pi() - angle <= tolerance;
    }

    template<class float_type>
    bool Vector3<float_type>::IsNormal(const Vector3<float_type> &direction, float_type tolerance) const
    {
        const float_type angle = GetAngle(direction);
        if (std::isnan(angle))
        {
            return false;
        }

        // 差值先在双精度下算，再显式收窄到 float_type，避免模板实例化时的隐式窄化告警
        const auto difference = static_cast<float_type>(std::abs(traits_type::pi() / 2.0 - angle));
        return difference <= tolerance;
    }

    template<class float_type>
    Vector3<float_type> &Vector3<float_type>::ProjectToPlane(const Vector3<float_type> &base, const Vector3<float_type> &normal)
    {
        // 用副本参与运算，避免中途改写入参 normal；投影 = 原点 − 法向分量
        Vector3<float_type> normalCopy(normal);
        *this = *this - (normalCopy *= ((*this - base) * normalCopy) / normalCopy.Sqr());
        return *this;
    }

    template<class float_type>
    void Vector3<float_type>::ProjectToPlane(const Vector3 &base, const Vector3 &normal, Vector3 &projection) const
    {
        Vector3<float_type> normalCopy(normal);
        projection = *this - (normalCopy *= ((*this - base) * normalCopy) / normalCopy.Sqr());
    }

    template<class float_type>
    float_type Vector3<float_type>::DistanceToPlane(const Vector3<float_type> &base, const Vector3<float_type> &normal) const
    {
        // 除以法向长度，允许传入未归一化的法向
        return ((*this - base) * normal) / normal.Length();
    }

    template<class float_type>
    float_type Vector3<float_type>::Length() const
    {
        return static_cast<float_type>(std::sqrt((x * x) + (y * y) + (z * z)));
    }

    template<class float_type>
    float_type Vector3<float_type>::DistanceToLine(const Vector3<float_type> &base, const Vector3<float_type> &direction) const
    {
        // 叉积长度 = 底 × 高，除以方向长度即得点到直线的垂距
        const Vector3<float_type> offset = *this - base;
        return static_cast<float_type>(std::abs((direction % offset).Length() / direction.Length()));
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::DistanceToLineSegment(const Vector3 &firstPoint, const Vector3 &secondPoint) const
    {
        const float_type squaredLength = DistanceP2(firstPoint, secondPoint);
        // 退化成一点的线段：最近点就是该点本身
        if (squaredLength == 0)
        {
            return firstPoint;
        }

        const Vector3<float_type> fromFirst = secondPoint - firstPoint;
        const Vector3<float_type> toPoint   = *this - firstPoint;
        const float_type          dot       = toPoint * fromFirst;
        // 参数 t 夹紧到 [0,1]，使投影落在线段范围内
        const float_type t = std::clamp(dot / squaredLength, static_cast<float_type>(0), static_cast<float_type>(1));
        return t * fromFirst - toPoint;
    }

    template<class float_type>
    Vector3<float_type> &Vector3<float_type>::ProjectToLine(const Vector3<float_type> &point, const Vector3<float_type> &line)
    {
        return (*this = ((((point * line) / line.Sqr()) * line) - point));
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::Perpendicular(const Vector3<float_type> &base, const Vector3<float_type> &direction) const
    {
        // 把本点到基点的位移投影到方向向量上，再加上基点得到垂足
        const float_type parameter = ((*this - base) * direction) / (direction * direction);
        return base + parameter * direction;
    }

    template<class float_type>
    float_type Vector3<float_type>::Sqr() const
    {
        return static_cast<float_type>((x * x) + (y * y) + (z * z));
    }

    template<class float_type>
    void Vector3<float_type>::Set(float_type xValue, float_type yValue, float_type zValue)
    {
        x = xValue;
        y = yValue;
        z = zValue;
    }

    template<class float_type>
    void Vector3<float_type>::ScaleX(float_type factor)
    {
        x *= factor;
    }

    template<class float_type>
    void Vector3<float_type>::ScaleY(float_type factor)
    {
        y *= factor;
    }

    template<class float_type>
    void Vector3<float_type>::ScaleZ(float_type factor)
    {
        z *= factor;
    }

    template<class float_type>
    void Vector3<float_type>::Scale(float_type xFactor, float_type yFactor, float_type zFactor)
    {
        x *= xFactor;
        y *= yFactor;
        z *= zFactor;
    }

    template<class float_type>
    void Vector3<float_type>::MoveX(float_type offset)
    {
        x += offset;
    }

    template<class float_type>
    void Vector3<float_type>::MoveY(float_type offset)
    {
        y += offset;
    }

    template<class float_type>
    void Vector3<float_type>::MoveZ(float_type offset)
    {
        z += offset;
    }

    template<class float_type>
    void Vector3<float_type>::Move(float_type xOffset, float_type yOffset, float_type zOffset)
    {
        x += xOffset;
        y += yOffset;
        z += zOffset;
    }

    template<class float_type>
    void Vector3<float_type>::RotateX(float_type angle)
    {
        // 缓存原始分量：旋转后 y/z 互相依赖，必须基于同一份快照计算
        const Vector3<float_type> original(*this);

        const float_type sinAngle = static_cast<float_type>(std::sin(angle));
        const float_type cosAngle = static_cast<float_type>(std::cos(angle));
        y                         = (original.y * cosAngle) - (original.z * sinAngle);
        z                         = (original.y * sinAngle) + (original.z * cosAngle);
    }

    template<class float_type>
    void Vector3<float_type>::RotateY(float_type angle)
    {
        const Vector3<float_type> original(*this);

        const float_type sinAngle = static_cast<float_type>(std::sin(angle));
        const float_type cosAngle = static_cast<float_type>(std::cos(angle));
        x                         = (original.z * sinAngle) + (original.x * cosAngle);
        z                         = (original.z * cosAngle) - (original.x * sinAngle);
    }

    template<class float_type>
    void Vector3<float_type>::RotateZ(float_type angle)
    {
        const Vector3<float_type> original(*this);

        const float_type sinAngle = static_cast<float_type>(std::sin(angle));
        const float_type cosAngle = static_cast<float_type>(std::cos(angle));
        x                         = (original.x * cosAngle) - (original.y * sinAngle);
        y                         = (original.x * sinAngle) + (original.y * cosAngle);
    }

    template<class float_type>
    Vector3<float_type> &Vector3<float_type>::Normalize()
    {
        const float_type length = Length();

        if (length == static_cast<float_type>(0.0))
        {
            // 零向量没有方向：静默不做事会把调用方的错误藏起来，这里显式报错
            throw ValueError("零向量无法归一化（长度为 0，没有方向）；请先用 IsNull() 判断并跳过，"
                             "或改为先给向量赋值再归一化");
        }

        // 长度恰为 1 时无需除法，直接返回以省去三次除法
        if (length != static_cast<float_type>(1.0))
        {
            x /= length;
            y /= length;
            z /= length;
        }

        return *this;
    }

    template<class float_type>
    Vector3<float_type> Vector3<float_type>::Normalized() const
    {
        Vector3<float_type> copy = *this;
        copy.Normalize();
        return copy;
    }

    template<class float_type>
    bool Vector3<float_type>::IsNull() const
    {
        const float_type zero{0.0};
        return (x == zero) && (y == zero) && (z == zero);
    }

    template<class float_type>
    float_type Vector3<float_type>::GetAngle(const Vector3 &other) const
    {
        const float_type lengthThis  = Length();
        const float_type lengthOther = other.Length();
        if (lengthThis <= traits_type::epsilon() || lengthOther <= traits_type::epsilon())
        {
            // 零向量与任何向量都不构成夹角：返回 NaN，让调用方（如 IsParallel）显式处理
            return std::numeric_limits<float_type>::quiet_NaN();
        }

        // 夹角余弦 = 点积 /（两向量长度之积），逐次相除避免长度乘积溢出
        float_type cosAngle = Dot(other);
        cosAngle /= lengthThis;
        cosAngle /= lengthOther;

        // 舍入误差可能让余弦略微越出 [-1,1]，必须夹紧否则 acos 返回 NaN
        if (cosAngle <= -1.0)
        {
            return traits_type::pi();
        }
        if (cosAngle >= 1.0)
        {
            return static_cast<float_type>(0.0);
        }

        return static_cast<float_type>(std::acos(cosAngle));
    }

    template<class float_type>
    float_type Vector3<float_type>::GetAngleOriented(const Vector3 &other, const Vector3 &normal) const
    {
        float_type angle = GetAngle(other);

        const Vector3<float_type> crossProduct = Cross(other);

        // 叉积与参考法向的点积定出旋转方向：为负说明按顺时针取角
        const float_type dot = crossProduct.Dot(normal);
        if (dot < 0)
        {
            angle = 2 * traits_type::pi() - angle;
        }

        return angle;
    }

    template<class float_type>
    void Vector3<float_type>::TransformToCoordinateSystem(const Vector3 &base, const Vector3 &xDirection, const Vector3 &yDirection)
    {
        // 先归一化两个给定方向，再用叉积补出第三轴，构造正交基
        Vector3<float_type> axisX = xDirection;
        Vector3<float_type> axisY = yDirection;
        Vector3<float_type> axisZ = xDirection % yDirection;
        axisX.Normalize();
        axisY.Normalize();
        axisZ.Normalize();

        // 新坐标 = 位移在三条基向量上的投影
        const Vector3<float_type> relative = *this - base;
        x                                  = axisX * relative;
        y                                  = axisY * relative;
        z                                  = axisZ * relative;
    }

    // 显式实例化：只支持 float 与 double，其他类型会在链接期而非头文件里报错
    template class Vector3<float>;
    template class Vector3<double>;
} // namespace ExpressionEngine::Base
