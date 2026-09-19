#include <ExpressionEngine/Base/Vector3D.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Base
{
    template<class FloatingType>
    Vector3<FloatingType>::Vector3(FloatingType xValue, FloatingType yValue, FloatingType zValue) : x(xValue), y(yValue), z(zValue)
    {
    }

    template<class FloatingType>
    FloatingType &Vector3<FloatingType>::operator[](unsigned short index)
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

    template<class FloatingType>
    const FloatingType &Vector3<FloatingType>::operator[](unsigned short index) const
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

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::operator+(const Vector3<FloatingType> &other) const
    {
        return Vector3<FloatingType>(x + other.x, y + other.y, z + other.z);
    }

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::operator&(const Vector3<FloatingType> &other) const
    {
        // 逐分量伸缩：另一向量的分量取绝对值后作为缩放因子，允许传带符号方向
        return Vector3<FloatingType>(x * std::abs(other.x), y * std::abs(other.y), z * std::abs(other.z));
    }

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::operator-(const Vector3<FloatingType> &other) const
    {
        return Vector3<FloatingType>(x - other.x, y - other.y, z - other.z);
    }

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::operator-() const
    {
        return Vector3<FloatingType>(-x, -y, -z);
    }

    template<class FloatingType>
    Vector3<FloatingType> &Vector3<FloatingType>::operator+=(const Vector3<FloatingType> &other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    template<class FloatingType>
    Vector3<FloatingType> &Vector3<FloatingType>::operator-=(const Vector3<FloatingType> &other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    template<class FloatingType>
    Vector3<FloatingType> &Vector3<FloatingType>::operator*=(FloatingType scale)
    {
        x *= scale;
        y *= scale;
        z *= scale;
        return *this;
    }

    template<class FloatingType>
    Vector3<FloatingType> &Vector3<FloatingType>::operator/=(FloatingType divisor)
    {
        x /= divisor;
        y /= divisor;
        z /= divisor;
        return *this;
    }

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::operator*(FloatingType scale) const
    {
        return Vector3<FloatingType>(x * scale, y * scale, z * scale);
    }

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::operator/(FloatingType divisor) const
    {
        return Vector3<FloatingType>(x / divisor, y / divisor, z / divisor);
    }

    template<class FloatingType>
    FloatingType Vector3<FloatingType>::operator*(const Vector3<FloatingType> &other) const
    {
        return (x * other.x) + (y * other.y) + (z * other.z);
    }

    template<class FloatingType>
    FloatingType Vector3<FloatingType>::dot(const Vector3<FloatingType> &other) const
    {
        return (x * other.x) + (y * other.y) + (z * other.z);
    }

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::operator%(const Vector3<FloatingType> &other) const
    {
        return Vector3<FloatingType>((y * other.z) - (z * other.y), (z * other.x) - (x * other.z), (x * other.y) - (y * other.x));
    }

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::cross(const Vector3<FloatingType> &other) const
    {
        return Vector3<FloatingType>((y * other.z) - (z * other.y), (z * other.x) - (x * other.z), (x * other.y) - (y * other.x));
    }

    template<class FloatingType>
    bool Vector3<FloatingType>::isOnLineSegment(const Vector3<FloatingType> &startPoint, const Vector3<FloatingType> &endPoint) const
    {
        const Vector3<FloatingType> segment      = endPoint - startPoint;
        const Vector3<FloatingType> toPoint      = *this - startPoint;
        const Vector3<FloatingType> crossProduct = segment.cross(toPoint);
        const FloatingType          dotProduct   = segment.dot(toPoint);

        // 叉积长度非零说明本点偏离线段所在直线
        if (crossProduct.length() > TraitsType::epsilon())
        {
            return false;
        }

        // 点积为负说明本点落在起点之前
        if (dotProduct < 0)
        {
            return false;
        }

        // 点积超过线段长度平方说明本点落在终点之后
        if (dotProduct > segment.squaredLength())
        {
            return false;
        }

        return true;
    }

    template<class FloatingType>
    bool Vector3<FloatingType>::operator!=(const Vector3<FloatingType> &other) const
    {
        return !((*this) == other);
    }

    template<class FloatingType>
    bool Vector3<FloatingType>::operator==(const Vector3<FloatingType> &other) const
    {
        // 以机器精度为容差逐分量比较：浮点运算的舍入误差不应被当成不相等
        return (std::abs(x - other.x) <= TraitsType::epsilon()) && (std::abs(y - other.y) <= TraitsType::epsilon()) && (std::abs(z - other.z) <= TraitsType::epsilon());
    }

    template<class FloatingType>
    bool Vector3<FloatingType>::isEqual(const Vector3<FloatingType> &point, FloatingType tolerance) const
    {
        return distance(*this, point) <= tolerance;
    }

    template<class FloatingType>
    bool Vector3<FloatingType>::isParallel(const Vector3<FloatingType> &direction, FloatingType tolerance) const
    {
        const FloatingType angle = getAngle(direction);
        // 零向量的夹角是 NaN：无法定义「平行」，按文档返回 false
        if (std::isnan(angle))
        {
            return false;
        }

        return angle <= tolerance || TraitsType::pi() - angle <= tolerance;
    }

    template<class FloatingType>
    bool Vector3<FloatingType>::isNormal(const Vector3<FloatingType> &direction, FloatingType tolerance) const
    {
        const FloatingType angle = getAngle(direction);
        if (std::isnan(angle))
        {
            return false;
        }

        // 差值先在双精度下算，再显式收窄到 FloatingType，避免模板实例化时的隐式窄化告警
        const auto difference = static_cast<FloatingType>(std::abs(TraitsType::pi() / 2.0 - angle));
        return difference <= tolerance;
    }

    template<class FloatingType>
    Vector3<FloatingType> &Vector3<FloatingType>::projectToPlane(const Vector3<FloatingType> &base, const Vector3<FloatingType> &normal)
    {
        // 用副本参与运算，避免中途改写入参 normal；投影 = 原点 − 法向分量
        Vector3<FloatingType> normalCopy(normal);
        *this = *this - (normalCopy *= ((*this - base) * normalCopy) / normalCopy.squaredLength());
        return *this;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::projectToPlane(const Vector3 &base, const Vector3 &normal, Vector3 &projection) const
    {
        Vector3<FloatingType> normalCopy(normal);
        projection = *this - (normalCopy *= ((*this - base) * normalCopy) / normalCopy.squaredLength());
    }

    template<class FloatingType>
    FloatingType Vector3<FloatingType>::distanceToPlane(const Vector3<FloatingType> &base, const Vector3<FloatingType> &normal) const
    {
        // 除以法向长度，允许传入未归一化的法向
        return ((*this - base) * normal) / normal.length();
    }

    template<class FloatingType>
    FloatingType Vector3<FloatingType>::length() const
    {
        return static_cast<FloatingType>(std::sqrt((x * x) + (y * y) + (z * z)));
    }

    template<class FloatingType>
    FloatingType Vector3<FloatingType>::distanceToLine(const Vector3<FloatingType> &base, const Vector3<FloatingType> &direction) const
    {
        // 叉积长度 = 底 × 高，除以方向长度即得点到直线的垂距
        const Vector3<FloatingType> offset = *this - base;
        return static_cast<FloatingType>(std::abs((direction % offset).length() / direction.length()));
    }

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::distanceToLineSegment(const Vector3 &firstPoint, const Vector3 &secondPoint) const
    {
        const FloatingType squaredLength = squaredDistance(firstPoint, secondPoint);
        // 退化成一点的线段：最近点就是该点本身
        if (squaredLength == 0)
        {
            return firstPoint;
        }

        const Vector3<FloatingType> fromFirst = secondPoint - firstPoint;
        const Vector3<FloatingType> toPoint   = *this - firstPoint;
        const FloatingType          dot       = toPoint * fromFirst;
        // 参数 t 夹紧到 [0,1]，使投影落在线段范围内
        const FloatingType t = std::clamp(dot / squaredLength, static_cast<FloatingType>(0), static_cast<FloatingType>(1));
        return t * fromFirst - toPoint;
    }

    template<class FloatingType>
    Vector3<FloatingType> &Vector3<FloatingType>::projectToLine(const Vector3<FloatingType> &point, const Vector3<FloatingType> &line)
    {
        return (*this = ((((point * line) / line.squaredLength()) * line) - point));
    }

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::perpendicular(const Vector3<FloatingType> &base, const Vector3<FloatingType> &direction) const
    {
        // 把本点到基点的位移投影到方向向量上，再加上基点得到垂足
        const FloatingType parameter = ((*this - base) * direction) / (direction * direction);
        return base + parameter * direction;
    }

    template<class FloatingType>
    FloatingType Vector3<FloatingType>::squaredLength() const
    {
        return static_cast<FloatingType>((x * x) + (y * y) + (z * z));
    }

    template<class FloatingType>
    void Vector3<FloatingType>::set(FloatingType xValue, FloatingType yValue, FloatingType zValue)
    {
        x = xValue;
        y = yValue;
        z = zValue;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::scaleX(FloatingType factor)
    {
        x *= factor;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::scaleY(FloatingType factor)
    {
        y *= factor;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::scaleZ(FloatingType factor)
    {
        z *= factor;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::scale(FloatingType xFactor, FloatingType yFactor, FloatingType zFactor)
    {
        x *= xFactor;
        y *= yFactor;
        z *= zFactor;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::moveX(FloatingType offset)
    {
        x += offset;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::moveY(FloatingType offset)
    {
        y += offset;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::moveZ(FloatingType offset)
    {
        z += offset;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::move(FloatingType xOffset, FloatingType yOffset, FloatingType zOffset)
    {
        x += xOffset;
        y += yOffset;
        z += zOffset;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::rotateX(FloatingType angle)
    {
        // 缓存原始分量：旋转后 y/z 互相依赖，必须基于同一份快照计算
        const Vector3<FloatingType> original(*this);

        const FloatingType sinAngle = static_cast<FloatingType>(std::sin(angle));
        const FloatingType cosAngle = static_cast<FloatingType>(std::cos(angle));
        y                           = (original.y * cosAngle) - (original.z * sinAngle);
        z                           = (original.y * sinAngle) + (original.z * cosAngle);
    }

    template<class FloatingType>
    void Vector3<FloatingType>::rotateY(FloatingType angle)
    {
        const Vector3<FloatingType> original(*this);

        const FloatingType sinAngle = static_cast<FloatingType>(std::sin(angle));
        const FloatingType cosAngle = static_cast<FloatingType>(std::cos(angle));
        x                           = (original.z * sinAngle) + (original.x * cosAngle);
        z                           = (original.z * cosAngle) - (original.x * sinAngle);
    }

    template<class FloatingType>
    void Vector3<FloatingType>::rotateZ(FloatingType angle)
    {
        const Vector3<FloatingType> original(*this);

        const FloatingType sinAngle = static_cast<FloatingType>(std::sin(angle));
        const FloatingType cosAngle = static_cast<FloatingType>(std::cos(angle));
        x                           = (original.x * cosAngle) - (original.y * sinAngle);
        y                           = (original.x * sinAngle) + (original.y * cosAngle);
    }

    template<class FloatingType>
    Vector3<FloatingType> &Vector3<FloatingType>::normalize()
    {
        const FloatingType magnitude = length();

        if (magnitude == static_cast<FloatingType>(0.0))
        {
            // 零向量没有方向：静默不做事会把调用方的错误藏起来，这里显式报错
            throw ValueError("零向量无法归一化（长度为 0，没有方向）；请先用 isNull() 判断并跳过，"
                             "或改为先给向量赋值再归一化");
        }

        // 长度恰为 1 时无需除法，直接返回以省去三次除法
        if (magnitude != static_cast<FloatingType>(1.0))
        {
            x /= magnitude;
            y /= magnitude;
            z /= magnitude;
        }

        return *this;
    }

    template<class FloatingType>
    Vector3<FloatingType> Vector3<FloatingType>::normalized() const
    {
        Vector3<FloatingType> copy = *this;
        copy.normalize();
        return copy;
    }

    template<class FloatingType>
    bool Vector3<FloatingType>::isNull() const
    {
        const FloatingType zero{0.0};
        return (x == zero) && (y == zero) && (z == zero);
    }

    template<class FloatingType>
    FloatingType Vector3<FloatingType>::getAngle(const Vector3 &other) const
    {
        const FloatingType lengthThis  = length();
        const FloatingType lengthOther = other.length();
        if (lengthThis <= TraitsType::epsilon() || lengthOther <= TraitsType::epsilon())
        {
            // 零向量与任何向量都不构成夹角：返回 NaN，让调用方（如 isParallel）显式处理
            return std::numeric_limits<FloatingType>::quiet_NaN();
        }

        // 夹角余弦 = 点积 /（两向量长度之积），逐次相除避免长度乘积溢出
        FloatingType cosAngle = dot(other);
        cosAngle /= lengthThis;
        cosAngle /= lengthOther;

        // 舍入误差可能让余弦略微越出 [-1,1]，必须夹紧否则 acos 返回 NaN
        if (cosAngle <= -1.0)
        {
            return TraitsType::pi();
        }
        if (cosAngle >= 1.0)
        {
            return static_cast<FloatingType>(0.0);
        }

        return static_cast<FloatingType>(std::acos(cosAngle));
    }

    template<class FloatingType>
    FloatingType Vector3<FloatingType>::getAngleOriented(const Vector3 &other, const Vector3 &normal) const
    {
        FloatingType angle = getAngle(other);

        const Vector3<FloatingType> crossProduct = cross(other);

        // 叉积与参考法向的点积定出旋转方向：为负说明按顺时针取角
        const FloatingType dot = crossProduct.dot(normal);
        if (dot < 0)
        {
            angle = 2 * TraitsType::pi() - angle;
        }

        return angle;
    }

    template<class FloatingType>
    void Vector3<FloatingType>::transformToCoordinateSystem(const Vector3 &base, const Vector3 &xDirection, const Vector3 &yDirection)
    {
        // 先归一化两个给定方向，再用叉积补出第三轴，构造正交基
        Vector3<FloatingType> axisX = xDirection;
        Vector3<FloatingType> axisY = yDirection;
        Vector3<FloatingType> axisZ = xDirection % yDirection;
        axisX.normalize();
        axisY.normalize();
        axisZ.normalize();

        // 新坐标 = 位移在三条基向量上的投影
        const Vector3<FloatingType> relative = *this - base;
        x                                    = axisX * relative;
        y                                    = axisY * relative;
        z                                    = axisZ * relative;
    }

    // 显式实例化：只支持 float 与 double，其他类型会在链接期而非头文件里报错
    template class Vector3<float>;
    template class Vector3<double>;
} // namespace ExpressionEngine::Base
