#include <ExpressionEngine/Expression/Value.h>

#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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

        /// 是否为同一个舍入量级上的相等
        bool essentiallyEqual(const double left, const double right)
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
                    if (std::fabs(left[row][column] - right[row][column]) > Base::Precision::confusion())
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
            const double *        quaternion = placement.getRotation().getValue();
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
                // 布尔按 0/1 参与数值比较
                return Units::Quantity(*boolean ? 1.0 : 0.0);
            }
            return std::nullopt;
        }

    } // namespace

    ValueSequence::ValueSequence(std::shared_ptr<ValueSequenceItems> items) : m_items(std::move(items))
    {
    }

    std::size_t ValueSequence::size() const noexcept
    {
        // 默认构造的序列不带存储，与「零个元素」同义
        return m_items == nullptr ? 0 : m_items->values.size();
    }

    bool ValueSequence::empty() const noexcept
    {
        return size() == 0;
    }

    const ValueSequenceItems *ValueSequence::items() const noexcept
    {
        return m_items.get();
    }

    ValueSequence makeValueSequence(std::vector<Value> values)
    {
        if (values.empty())
        {
            return ValueSequence();
        }
        return ValueSequence(std::make_shared<ValueSequenceItems>(ValueSequenceItems{std::move(values)}));
    }

    const std::vector<Value> &sequenceValues(const ValueSequence &sequence)
    {
        static const std::vector<Value> s_empty;
        const ValueSequenceItems       *items = sequence.items();
        return items == nullptr ? s_empty : items->values;
    }

    const Value &sequenceAt(const ValueSequence &sequence, const std::size_t index)
    {
        const std::vector<Value> &values = sequenceValues(sequence);
        if (index >= values.size())
        {
            throw Base::IndexError(std::format("序列只有 {} 个元素，却要取第 {} 个；下标从 0 到 {}；请改用范围内的下标",
                                               values.size(), index, values.size() == 0 ? 0 : values.size() - 1));
        }
        return values[index];
    }

    bool isNumeric(const Value &value)
    {
        return std::holds_alternative<Units::Quantity>(value) || std::holds_alternative<double>(value);
    }

    bool isGeometric(const Value &value)
    {
        return std::holds_alternative<Base::Vector3d>(value) || std::holds_alternative<Base::Matrix4D>(value) || std::holds_alternative<Base::Rotation>(value) ||
               std::holds_alternative<Base::Placement>(value);
    }

    bool isSequence(const Value &value)
    {
        return std::holds_alternative<ValueSequence>(value);
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
        if (std::holds_alternative<Base::Placement>(value))
        {
            return "位姿";
        }
        return "序列";
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
            // 布尔当量使用
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

        if (const auto *sequence = std::get_if<ValueSequence>(&value))
        {
            throw Base::TypeError(std::format("{}需要数量，但拿到的是含 {} 个元素的序列；请用 [下标] 取出单个元素，"
                                              "或改用 sum()、average() 等聚合函数",
                                              context, sequence->size()));
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

    std::string formatExpressionNumber(const double value)
    {
        // to_chars 的默认浮点格式给出「能往返的最短写法」：既不多打一串无意义尾数，
        // 也不会像固定 16 位那样把 0.1 + 0.2 写成 0.3 后解析回另一个数
        std::array<char, 40>       buffer{};
        const std::to_chars_result result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (result.ec != std::errc())
        {
            throw std::logic_error("数值无法排版成表达式文本；请检查该取值是否为有限的双精度数");
        }
        return std::string(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
    }

    std::string quoteExpressionText(const std::string_view text)
    {
        std::string body;
        body.reserve(text.size());
        for (const char character: text)
        {
            switch (character)
            {
                case '\\':
                    body += "\\\\";
                    break;
                case '>':
                    // 正文里单独的 '>' 不是结束符的一部分，词法器会在此断开
                    body += "\\>";
                    break;
                case '#':
                    // 未转义的 '#' 会把整段文本当成 <<文档#单元格>> 引用
                    body += "\\#";
                    break;
                case '\n':
                    body += "\\n";
                    break;
                case '\r':
                    body += "\\r";
                    break;
                case '\t':
                    body += "\\t";
                    break;
                default:
                    body += character;
                    break;
            }
        }
        return "<<" + body + ">>";
    }

    std::string toExpressionText(const Value &value)
    {
        if (const auto *quantity = std::get_if<Units::Quantity>(&value))
        {
            const std::string number = formatExpressionNumber(quantity->getValue());
            // 纯数只写数字；带量纲的必须把量纲也写上，否则解析回来变成一个纯数
            return quantity->isDimensionless() ? number : number + " " + quantity->getUnit().getString();
        }
        if (const auto *number = std::get_if<double>(&value))
        {
            return formatExpressionNumber(*number);
        }
        if (const auto *boolean = std::get_if<bool>(&value))
        {
            return *boolean ? "True" : "False";
        }
        if (const auto *text = std::get_if<std::string>(&value))
        {
            return quoteExpressionText(*text);
        }
        if (const auto *vector = std::get_if<Base::Vector3d>(&value))
        {
            return std::format("vector({}; {}; {})", formatExpressionNumber(vector->x), formatExpressionNumber(vector->y), formatExpressionNumber(vector->z));
        }
        if (const auto *matrix = std::get_if<Base::Matrix4D>(&value))
        {
            std::string text = "matrix(";
            for (unsigned int row = 0; row < 4; ++row)
            {
                for (unsigned int column = 0; column < 4; ++column)
                {
                    if (row != 0 || column != 0)
                    {
                        text += "; ";
                    }
                    text += formatExpressionNumber((*matrix)[row][column]);
                }
            }
            return text + ')';
        }
        if (const auto *rotation = std::get_if<Base::Rotation>(&value))
        {
            double yaw   = 0.0;
            double pitch = 0.0;
            double roll  = 0.0;
            rotation->getYawPitchRoll(yaw, pitch, roll);
            return std::format("rotation({}; {}; {})", formatExpressionNumber(yaw), formatExpressionNumber(pitch), formatExpressionNumber(roll));
        }
        if (const auto *placement = std::get_if<Base::Placement>(&value))
        {
            return std::format("placement({}; {})", toExpressionText(placement->getPosition()), toExpressionText(placement->getRotation()));
        }
        if (const auto *sequence = std::get_if<ValueSequence>(&value))
        {
            std::string               text   = "list(";
            const std::vector<Value> &values = sequenceValues(*sequence);
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    text += "; ";
                }
                text += toExpressionText(values[index]);
            }
            return text + ')';
        }

        throw std::logic_error("Value 出现未处理的备选类型，请同步更新 toExpressionText()");
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
        if (const auto *sequence = std::get_if<ValueSequence>(&value))
        {
            std::string               text   = "list(";
            const std::vector<Value> &values = sequenceValues(*sequence);
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                if (index != 0)
                {
                    text += "; ";
                }
                text += toString(values[index]);
            }
            text += ')';
            return text;
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
                []<typename T0, typename T1>(const T0 &leftValue, const T1 &rightValue) -> bool
                {
                    using LeftType  = std::decay_t<T0>;
                    using RightType = std::decay_t<T1>;
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
                        return leftValue.isEqual(rightValue, Base::Precision::confusion());
                    } else if constexpr (std::is_same_v<LeftType, Base::Matrix4D>)
                    {
                        return matricesEqual(leftValue, rightValue);
                    } else if constexpr (std::is_same_v<LeftType, Base::Rotation>)
                    {
                        return leftValue.isSame(rightValue, Base::Precision::angular());
                    } else if constexpr (std::is_same_v<LeftType, ValueSequence>)
                    {
                        const std::vector<Value> &leftValues  = sequenceValues(leftValue);
                        const std::vector<Value> &rightValues = sequenceValues(rightValue);
                        if (leftValues.size() != rightValues.size())
                        {
                            return false;
                        }
                        for (std::size_t position = 0; position < leftValues.size(); ++position)
                        {
                            if (!valuesEqual(leftValues[position], rightValues[position]))
                            {
                                return false;
                            }
                        }
                        return true;
                    } else
                    {
                        // 备选类型只剩位姿；若 Value 将来新增备选，这里的静态断言会先报出来
                        static_assert(std::is_same_v<LeftType, Base::Placement>, "Value 增加了新的备选类型，请同步更新相等判定");
                        return leftValue.getPosition().isEqual(rightValue.getPosition(), Base::Precision::confusion()) &&
                               leftValue.getRotation().isSame(rightValue.getRotation(), Base::Precision::angular());
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
