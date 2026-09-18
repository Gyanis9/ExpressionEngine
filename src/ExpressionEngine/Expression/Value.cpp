#include <ExpressionEngine/Expression/Value.h>

#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Placement.h>
#include <ExpressionEngine/Base/Precision.h>
#include <ExpressionEngine/Base/Rotation.h>
#include <ExpressionEngine/Base/Vector3D.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Expression
{

    namespace
    {

        /// 几何值排版时保留的有效数字位数：够定位问题，又不会排出一长串小数
        constexpr int s_geometryDigits = 6;

        /// 是否为同一个舍入量级上的相等（与 FreeCAD 的 essentiallyEqual 一致）
        bool essentiallyEqual(double left, double right)
        {
            constexpr double epsilon = std::numeric_limits<double>::epsilon();
            const double     scale   = std::fabs(left) > std::fabs(right) ? std::fabs(right) : std::fabs(left);
            return std::fabs(left - right) <= scale * epsilon;
        }

        /// 两个矩阵是否逐元素落在同一容差内
        bool matricesEqual(const Base::Matrix4D &left, const Base::Matrix4D &right)
        {
            for (unsigned int row = 0; row < 4; ++row)
            {
                for (unsigned int column = 0; column < 4; ++column)
                {
                    if (std::fabs(left[row][column] - right[row][column]) > Base::Precision::Confusion())
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        /// 向量的紧凑写法，形如 "(1, 2, 3)"
        std::string formatVector(const Base::Vector3d &vector)
        {
            return std::format("({:.{}}, {:.{}}, {:.{}})", vector.x, s_geometryDigits, vector.y, s_geometryDigits, vector.z, s_geometryDigits);
        }

        /// 矩阵的紧凑写法，形如 "Matrix((1, 0, 0, 0), (0, 1, 0, 0), ...)"
        std::string formatMatrix(const Base::Matrix4D &matrix)
        {
            std::string text = "Matrix(";
            for (unsigned int row = 0; row < 4; ++row)
            {
                text += std::format("({:.{}}, {:.{}}, {:.{}}, {:.{}})", matrix[row][0], s_geometryDigits, matrix[row][1], s_geometryDigits, matrix[row][2], s_geometryDigits,
                                    matrix[row][3], s_geometryDigits);
                if (row != 3)
                {
                    text += ", ";
                }
            }
            text += ')';
            return text;
        }

        /// 旋转按四元数排版，形如 "Rotation (0, 0, 0, 1)"
        std::string formatRotation(const Base::Rotation &rotation)
        {
            const double *quaternion = rotation.getValue();
            return std::format("Rotation ({:.{}}, {:.{}}, {:.{}}, {:.{}})", quaternion[0], s_geometryDigits, quaternion[1], s_geometryDigits, quaternion[2], s_geometryDigits,
                               quaternion[3], s_geometryDigits);
        }

        /// 位姿按位置与旋转排版，形如 "Placement [Pos=(0, 0, 0), Rot=(0, 0, 0, 1)]"
        std::string formatPlacement(const Base::Placement &placement)
        {
            const Base::Vector3d &position   = placement.getPosition();
            const double         *quaternion = placement.getRotation().getValue();
            return std::format("Placement [Pos=({:.{}}, {:.{}}, {:.{}}), Rot=({:.{}}, {:.{}}, {:.{}}, {:.{}})]", position.x, s_geometryDigits, position.y, s_geometryDigits,
                               position.z, s_geometryDigits, quaternion[0], s_geometryDigits, quaternion[1], s_geometryDigits, quaternion[2], s_geometryDigits, quaternion[3],
                               s_geometryDigits);
        }

        /// 把数值型取值看成数量；非数值型返回空
        std::optional<Units::Quantity> numericAsQuantity(const Value &value)
        {
            if (const auto *quantity = std::get_if<Units::Quantity>(&value))
            {
                return *quantity;
            }
            if (const auto *number = std::get_if<double>(&value))
            {
                return Units::Quantity(*number);
            }
            if (const auto *boolean = std::get_if<bool>(&value))
            {
                // 布尔按 0/1 参与数值比较，与 FreeCAD 的 anyToQuantity 一致
                return Units::Quantity(*boolean ? 1.0 : 0.0);
            }
            return std::nullopt;
        }

    } // namespace

    bool isNumeric(const Value &value)
    {
        return std::holds_alternative<Units::Quantity>(value) || std::holds_alternative<double>(value);
    }

    bool isGeometric(const Value &value)
    {
        return std::holds_alternative<Base::Vector3d>(value) || std::holds_alternative<Base::Matrix4D>(value) || std::holds_alternative<Base::Rotation>(value) ||
               std::holds_alternative<Base::Placement>(value);
    }

    std::string_view valueTypeName(const Value &value)
    {
        if (std::holds_alternative<Units::Quantity>(value))
        {
            return "数量";
        }
        if (std::holds_alternative<double>(value))
        {
            return "纯数";
        }
        if (std::holds_alternative<bool>(value))
        {
            return "布尔";
        }
        if (std::holds_alternative<std::string>(value))
        {
            return "文本";
        }
        if (std::holds_alternative<Base::Vector3d>(value))
        {
            return "向量";
        }
        if (std::holds_alternative<Base::Matrix4D>(value))
        {
            return "矩阵";
        }
        if (std::holds_alternative<Base::Rotation>(value))
        {
            return "旋转";
        }
        return "位姿";
    }

    Units::Quantity toQuantity(const Value &value, std::string_view context)
    {
        if (const auto *quantity = std::get_if<Units::Quantity>(&value))
        {
            return *quantity;
        }
        if (const auto *number = std::get_if<double>(&value))
        {
            return Units::Quantity(*number);
        }
        if (const auto *boolean = std::get_if<bool>(&value))
        {
            // 布尔当量使用，与 FreeCAD 的 anyToQuantity(bool) 一致
            return Units::Quantity(*boolean ? 1.0 : 0.0);
        }
        if (const auto *text = std::get_if<std::string>(&value))
        {
            try
            {
                return Units::Quantity::parse(*text);
            } catch (const Base::Exception &error)
            {
                // 文本形态的数值解析失败时报解析错，并说明该怎么改
                throw Base::ParserError(std::format("{}需要数量，但文本 '{}' 不是可解析的数量（{}）；"
                                                    "请写成 '1.5 mm'、'90 deg' 这样的形式",
                                                    context, *text, error.message()));
            }
        }

        throw Base::TypeError(std::format("{}需要数量，但拿到的是{}；几何值请改用 vdot()、vangle() 等向量函数"
                                          "或取具体分量后再参与数值运算",
                                          context, valueTypeName(value)));
    }

    double toDouble(const Value &value, std::string_view context)
    {
        if (const auto *number = std::get_if<double>(&value))
        {
            return *number;
        }
        if (const auto *boolean = std::get_if<bool>(&value))
        {
            return *boolean ? 1.0 : 0.0;
        }
        if (const auto *quantity = std::get_if<Units::Quantity>(&value))
        {
            if (!quantity->isDimensionless())
            {
                throw Base::TypeError(std::format("{}需要纯数，但拿到带量纲的量 {}；"
                                                  "请先除以同量纲的参照量把它变成纯数",
                                                  context, quantity->getUserString()));
            }
            return quantity->getValue();
        }

        throw Base::TypeError(std::format("{}需要纯数，但拿到的是{}；请改用数字或无量纲的量", context, valueTypeName(value)));
    }

    bool toBool(const Value &value, std::string_view context)
    {
        if (const auto *boolean = std::get_if<bool>(&value))
        {
            return *boolean;
        }
        if (const auto *quantity = std::get_if<Units::Quantity>(&value))
        {
            return quantity->getValue() != 0.0;
        }
        if (const auto *number = std::get_if<double>(&value))
        {
            return *number != 0.0;
        }

        throw Base::TypeError(std::format("{}需要能判定真假的取值，但拿到的是{}；"
                                          "请改用比较运算（如 x > 0、相等判断）得到布尔值",
                                          context, valueTypeName(value)));
    }

    std::string toString(const Value &value)
    {
        if (const auto *quantity = std::get_if<Units::Quantity>(&value))
        {
            return quantity->getUserString();
        }
        if (const auto *number = std::get_if<double>(&value))
        {
            // 纯数按无量纲量排版，与数量走同一套单位方案
            return Units::Quantity(*number).getUserString();
        }
        if (const auto *boolean = std::get_if<bool>(&value))
        {
            return *boolean ? "真" : "假";
        }
        if (const auto *text = std::get_if<std::string>(&value))
        {
            return *text;
        }
        if (const auto *vector = std::get_if<Base::Vector3d>(&value))
        {
            return formatVector(*vector);
        }
        if (const auto *matrix = std::get_if<Base::Matrix4D>(&value))
        {
            return formatMatrix(*matrix);
        }
        if (const auto *rotation = std::get_if<Base::Rotation>(&value))
        {
            return formatRotation(*rotation);
        }
        if (const auto *placement = std::get_if<Base::Placement>(&value))
        {
            return formatPlacement(*placement);
        }

        // 变体没有其它备选类型，走到这里说明 Value 的定义与排版函数脱节，属于库内部不一致
        throw std::logic_error("Value 出现未处理的备选类型，请同步更新 toString()");
    }

    bool valuesEqual(const Value &left, const Value &right)
    {
        if (left.index() != right.index())
        {
            // 契约规定类型不同一律不相等，不做跨类型的数值换算
            return false;
        }

        return std::visit(
                [](const auto &leftValue, const auto &rightValue) -> bool
                {
                    using LeftType  = std::decay_t<decltype(leftValue)>;
                    using RightType = std::decay_t<decltype(rightValue)>;
                    if constexpr (!std::is_same_v<LeftType, RightType>)
                    {
                        // 备选下标相同保证了不会走到这里，兜底返回不相等以免掩盖实现错误
                        return false;
                    } else if constexpr (std::is_same_v<LeftType, Units::Quantity>)
                    {
                        // 量纲不同时 Quantity 的相等判定返回 false，不抛错
                        return leftValue == rightValue;
                    } else if constexpr (std::is_same_v<LeftType, double>)
                    {
                        return essentiallyEqual(leftValue, rightValue);
                    } else if constexpr (std::is_same_v<LeftType, bool>)
                    {
                        return leftValue == rightValue;
                    } else if constexpr (std::is_same_v<LeftType, std::string>)
                    {
                        return leftValue == rightValue;
                    } else if constexpr (std::is_same_v<LeftType, Base::Vector3d>)
                    {
                        return leftValue.IsEqual(rightValue, Base::Precision::Confusion());
                    } else if constexpr (std::is_same_v<LeftType, Base::Matrix4D>)
                    {
                        return matricesEqual(leftValue, rightValue);
                    } else if constexpr (std::is_same_v<LeftType, Base::Rotation>)
                    {
                        return leftValue.isSame(rightValue, Base::Precision::Angular());
                    } else
                    {
                        // 备选类型只剩位姿；若 Value 将来新增备选，这里的静态断言会先报出来
                        static_assert(std::is_same_v<LeftType, Base::Placement>, "Value 增加了新的备选类型，请同步更新相等判定");
                        return leftValue.getPosition().IsEqual(rightValue.getPosition(), Base::Precision::Confusion()) &&
                               leftValue.getRotation().isSame(rightValue.getRotation(), Base::Precision::Angular());
                    }
                },
                left, right);
    }

    bool valueLessThan(const Value &left, const Value &right)
    {
        const std::optional<Units::Quantity> leftNumber  = numericAsQuantity(left);
        const std::optional<Units::Quantity> rightNumber = numericAsQuantity(right);
        if (leftNumber.has_value() && rightNumber.has_value())
        {
            if (leftNumber->getUnit() != rightNumber->getUnit())
            {
                // 量纲不同时大小无意义，宁可报错也不按数值硬比
                throw Base::UnitsMismatchError(std::format("比较两个量的大小时单位必须一致：左侧是 {}，右侧是 {}；"
                                                           "请先把两侧换算成同一单位",
                                                           leftNumber->getUnit().getString(), rightNumber->getUnit().getString()));
            }
            return leftNumber->getValue() < rightNumber->getValue();
        }

        if (const auto *leftText = std::get_if<std::string>(&left))
        {
            const auto *rightText = std::get_if<std::string>(&right);
            if (rightText == nullptr)
            {
                throw Base::TypeError(std::format("文本只能和文本比较大小，右侧是{}；请统一两侧的取值类型", valueTypeName(right)));
            }
            return *leftText < *rightText;
        }

        if (const auto *leftBoolean = std::get_if<bool>(&left))
        {
            const auto *rightBoolean = std::get_if<bool>(&right);
            if (rightBoolean == nullptr)
            {
                throw Base::TypeError(std::format("布尔只能和布尔比较大小，右侧是{}；请统一两侧的取值类型", valueTypeName(right)));
            }
            return !*leftBoolean && *rightBoolean;
        }

        throw Base::TypeError(std::format("{}与{}之间没有大小关系，只能做 == 与 != 比较；"
                                          "几何值请改用 vdot()、vangle() 等函数取数值后再比较",
                                          valueTypeName(left), valueTypeName(right)));
    }

} // namespace ExpressionEngine::Expression
