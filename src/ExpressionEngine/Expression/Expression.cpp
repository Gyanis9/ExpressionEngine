#include <ExpressionEngine/Expression/Expression.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Placement.h>
#include <ExpressionEngine/Base/Precision.h>
#include <ExpressionEngine/Base/Rotation.h>
#include <ExpressionEngine/Base/Tools.h>
#include <ExpressionEngine/Base/Vector3D.h>
#include <ExpressionEngine/Expression/ComponentAccess.h>
#include <ExpressionEngine/Expression/Range.h>
#include <ExpressionEngine/Expression/Value.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Expression
{

    namespace
    {

        /// 角度与弧度的换算：库内角度量一律以度存储，与 FreeCAD 的基准单位一致
        double toRadians(double degrees)
        {
            return degrees * std::numbers::pi / 180.0;
        }

        double toDegrees(double radians)
        {
            return radians * 180.0 / std::numbers::pi;
        }

        /// 真值判定：与 FreeCAD 一致，|值| 达到重合精度即视为真
        bool asBoolean(double value)
        {
            return std::fabs(value) >= Base::Precision::Confusion();
        }

        /**
         * @brief 数值是否为整数
         * @param value 待判定的数值
         * @param result 输出参数；是整数时写入该整数
         * @return 数值为整数且能装进 long 时为 true
         */
        bool essentiallyInteger(double value, long &result)
        {
            double integralPart = 0.0;
            if (std::modf(value, &integralPart) != 0.0)
            {
                return false;
            }
            // 超出 long 表示范围的值当作「不是可用整数」处理，避免截断成错误的下标
            if (integralPart < static_cast<double>(std::numeric_limits<long>::min()) || integralPart > static_cast<double>(std::numeric_limits<long>::max()))
            {
                return false;
            }
            result = static_cast<long>(integralPart);
            return true;
        }

        /// 数值的表达式文本；与 FreeCAD 一致取 digits10 位有效数字，不写出无意义的尾数
        std::string formatNumber(double value)
        {
            return std::format("{:.{}g}", value, std::numeric_limits<double>::digits10);
        }

        /// 文本的表达式写法，用单引号定界并转义内部的引号
        std::string quoteText(const std::string &text)
        {
            return "'" + Base::Tools::escapeQuotesFromString(text) + "'";
        }

        /// 取值的可读文本；单独包一层是为了在成员函数里也能解析到这个自由函数
        std::string valueText(const Value &value)
        {
            return toString(value);
        }

        /// 取数值型取值的原始数值；非数值型返回 false
        bool numericMagnitude(const Value &value, double &result)
        {
            if (const auto *quantity = std::get_if<Units::Quantity>(&value))
            {
                result = quantity->getValue();
                return true;
            }
            if (const auto *number = std::get_if<double>(&value))
            {
                result = *number;
                return true;
            }
            if (const auto *boolean = std::get_if<bool>(&value))
            {
                result = *boolean ? 1.0 : 0.0;
                return true;
            }
            return false;
        }

        /// 把名字列表拼成一行，用于报错时列出可选属性
        std::string joinNames(const std::vector<std::string> &names)
        {
            if (names.empty())
            {
                return "（没有属性）";
            }
            std::string text;
            for (std::size_t index = 0; index < names.size(); ++index)
            {
                if (index != 0)
                {
                    text += "、";
                }
                text += names[index];
            }
            return text;
        }

        /// 两个可空的子表达式是否结构相同
        bool expressionsSame(const Expression *left, const Expression *right)
        {
            if (left == nullptr || right == nullptr)
            {
                // 只有两边都没有才是相同，避免把「缺分量」当成「有分量」相等
                return left == right;
            }
            return left->isSame(*right, true);
        }

        /// 参数个数不符时的统一报错
        [[noreturn]] void throwArgumentCount(std::string_view label, std::string_view requirement, std::size_t argumentCount)
        {
            throw EvaluationError(std::format("{}() 的参数个数不正确：需要{}，实际收到 {} 个；请调整调用处的参数个数", label, requirement, argumentCount));
        }

        /**
         * @brief 校验函数调用是否可用
         * @param function 函数种类
         * @param argumentCount 实参个数
         * @param label 函数名，用于报错
         * @throws EvaluationError 参数个数不符
         * @throws Base::ParserError 函数是哨兵名字，或依赖宿主的对象工厂
         */
        void validateFunctionCall(FunctionExpression::Function function, std::size_t argumentCount, std::string_view label)
        {
            using Function = FunctionExpression::Function;
            switch (function)
            {
                case Function::Absolute:
                case Function::ArcCosine:
                case Function::ArcSine:
                case Function::ArcTangent:
                case Function::CubeRoot:
                case Function::Ceiling:
                case Function::Cosine:
                case Function::HyperbolicCosine:
                case Function::Exponential:
                case Function::Floor:
                case Function::HiddenReference:
                case Function::HiddenReferenceAlias:
                case Function::Logarithm:
                case Function::LogarithmBase10:
                case Function::MatrixInvert:
                case Function::ParseQuantity:
                case Function::RotationX:
                case Function::RotationY:
                case Function::RotationZ:
                case Function::Round:
                case Function::Sine:
                case Function::HyperbolicSine:
                case Function::SquareRoot:
                case Function::Stringify:
                case Function::Tangent:
                case Function::HyperbolicTangent:
                case Function::Truncate:
                case Function::VectorNormalize:
                case Function::LogicalNot:
                    if (argumentCount != 1)
                    {
                        throwArgumentCount(label, "恰好 1 个参数", argumentCount);
                    }
                    return;
                case Function::Placement:
                    if (argumentCount > 3)
                    {
                        throwArgumentCount(label, "1 个、2 个或 3 个参数", argumentCount);
                    }
                    return;
                case Function::TranslationMatrix:
                    if (argumentCount != 1 && argumentCount != 3)
                    {
                        throwArgumentCount(label, "1 个或 3 个参数", argumentCount);
                    }
                    return;
                case Function::ArcTangent2:
                case Function::Modulo:
                case Function::MatrixRotateX:
                case Function::MatrixRotateY:
                case Function::MatrixRotateZ:
                case Function::Power:
                case Function::VectorAngle:
                case Function::VectorCross:
                case Function::VectorDot:
                case Function::VectorScaleX:
                case Function::VectorScaleY:
                case Function::VectorScaleZ:
                    if (argumentCount != 2)
                    {
                        throwArgumentCount(label, "恰好 2 个参数", argumentCount);
                    }
                    return;
                case Function::Address:
                case Function::Cathetus:
                case Function::Hypotenuse:
                case Function::Rotation:
                    if (argumentCount < 2 || argumentCount > 3)
                    {
                        throwArgumentCount(label, "2 个或 3 个参数", argumentCount);
                    }
                    return;
                case Function::MatrixScale:
                case Function::MatrixTranslate:
                    if (argumentCount != 2 && argumentCount != 4)
                    {
                        throwArgumentCount(label, "2 个或 4 个参数", argumentCount);
                    }
                    return;
                case Function::MatrixRotate:
                    if (argumentCount < 2 || argumentCount > 4)
                    {
                        throwArgumentCount(label, "2 个、3 个或 4 个参数", argumentCount);
                    }
                    return;
                case Function::Vector:
                case Function::VectorLineDistance:
                case Function::VectorLineSegmentDistance:
                case Function::VectorLineProjection:
                case Function::VectorPlaneDistance:
                case Function::VectorPlaneProjection:
                    if (argumentCount != 3)
                    {
                        throwArgumentCount(label, "恰好 3 个参数", argumentCount);
                    }
                    return;
                case Function::VectorScale:
                    if (argumentCount != 4)
                    {
                        throwArgumentCount(label, "恰好 4 个参数", argumentCount);
                    }
                    return;
                case Function::Matrix:
                    if (argumentCount > 16)
                    {
                        throwArgumentCount(label, "0 到 16 个参数", argumentCount);
                    }
                    return;
                case Function::Average:
                case Function::Count:
                case Function::Maximum:
                case Function::Minimum:
                case Function::StandardDeviation:
                case Function::Sum:
                case Function::LogicalAnd:
                case Function::LogicalOr:
                    if (argumentCount == 0)
                    {
                        throwArgumentCount(label, "至少 1 个参数", argumentCount);
                    }
                    return;
                case Function::Create:
                case Function::List:
                case Function::Tuple:
                    // 这三个函数要造宿主对象，本库没有 Python 对象模型，构造期就拦下
                    throw EvaluationError(std::format("{}() 依赖宿主提供的对象工厂；请改用 "
                                                      "matrix()、vector()、rotation()、placement() 这些构造函数，"
                                                      "或由宿主属性直接给出取值",
                                                      label));
                case Function::None:
                case Function::Aggregates:
                case Function::Last:
                    throw Base::ParserError(std::format("'{}' 不是可求值的函数名；请改用函数表里的函数名（如 sqrt、sin、mrotatez）", label));
                default:
                    throw EvaluationError(std::format("函数 '{}' 未登记参数个数要求；请同步更新函数表与参数校验", label));
            }
        }

        /// 是否聚合函数
        bool isAggregate(FunctionExpression::Function function)
        {
            using Function = FunctionExpression::Function;
            switch (function)
            {
                case Function::Average:
                case Function::Count:
                case Function::Maximum:
                case Function::Minimum:
                case Function::StandardDeviation:
                case Function::Sum:
                case Function::LogicalAnd:
                case Function::LogicalOr:
                    return true;
                default:
                    return false;
            }
        }

        /// 取第 index 个实参的向量取值
        Base::Vector3d vectorArgument(const std::vector<ExpressionPtr> &arguments, std::size_t index, std::string_view label)
        {
            const Value value = arguments[index]->evaluate();
            if (const auto *vector = std::get_if<Base::Vector3d>(&value))
            {
                return *vector;
            }
            throw Base::TypeError(std::format("{}() 的第 {} 个参数需要向量，实际是{}；请改用 vector(x, y, z) 构造向量", label, index + 1, valueTypeName(value)));
        }

        /// 取第 index 个实参的数值，要求无量纲或长度量纲，返回基准单位下的数值
        double lengthArgument(const std::vector<ExpressionPtr> &arguments, std::size_t index, std::string_view label)
        {
            const Units::Quantity quantity = toQuantity(arguments[index]->evaluate(), std::format("{}() 的第 {} 个参数", label, index + 1));
            if (!quantity.isDimensionlessOrUnit(Units::Unit::Length))
            {
                throw Base::UnitsMismatchError(std::format("{}() 的第 {} 个参数需要长度量或纯数，实际是 {}；"
                                                           "请改用 mm、in 这类长度单位",
                                                           label, index + 1, quantity.getUserString()));
            }
            return quantity.getValue();
        }

        /// 取第 index 个实参的纯数值
        double numberArgument(const std::vector<ExpressionPtr> &arguments, std::size_t index, std::string_view label)
        {
            return toDouble(arguments[index]->evaluate(), std::format("{}() 的第 {} 个参数", label, index + 1));
        }

        /// 取第二个向量实参：两参数形式直接给向量，四参数形式给三个分量
        Base::Vector3d secondVectorArgument(const std::vector<ExpressionPtr> &arguments, std::string_view label)
        {
            if (arguments.size() == 2)
            {
                return vectorArgument(arguments, 1, label);
            }
            return Base::Vector3d(numberArgument(arguments, 1, label), numberArgument(arguments, 2, label), numberArgument(arguments, 3, label));
        }

        /// 对第一个实参施加变换矩阵，支持矩阵、位姿与旋转
        Value transformFirstArgument(const std::vector<ExpressionPtr> &arguments, const Base::Matrix4D &transformationMatrix, std::string_view label)
        {
            const Value target = arguments[0]->evaluate();
            if (const auto *matrix = std::get_if<Base::Matrix4D>(&target))
            {
                return transformationMatrix * *matrix;
            }
            if (const auto *placement = std::get_if<Base::Placement>(&target))
            {
                return Base::Placement(transformationMatrix * placement->toMatrix());
            }
            if (const auto *rotation = std::get_if<Base::Rotation>(&target))
            {
                Base::Matrix4D rotationMatrix;
                rotation->getValue(rotationMatrix);
                return Base::Rotation(transformationMatrix * rotationMatrix);
            }
            throw Base::TypeError(std::format("{}() 的第一个参数需要矩阵、位姿或旋转，实际是{}；"
                                              "请用 matrix()、placement()、rotation() 构造这类取值",
                                              label, valueTypeName(target)));
        }

        /// 取数量，要求无量纲或角度量纲，返回弧度值
        double angleArgument(const Value &value, std::string_view context)
        {
            const Units::Quantity quantity = toQuantity(value, context);
            if (!quantity.isDimensionlessOrUnit(Units::Unit::Angle))
            {
                throw Base::UnitsMismatchError(std::format("{}需要角度量或纯数，实际是 {}；请改用 deg、rad 这类角度单位", context, quantity.getUserString()));
            }
            // 库内角度量以度存储，按弧度使用前统一换算
            return toRadians(quantity.getValue());
        }

        /// 数值按数量相加；两侧都是文本时做拼接
        Value addValues(const Value &left, const Value &right)
        {
            if (const auto *leftText = std::get_if<std::string>(&left))
            {
                const auto *rightText = std::get_if<std::string>(&right);
                if (rightText == nullptr)
                {
                    throw Base::TypeError(std::format("加法不能用文本与{}相加；请统一两侧为文本，或都换成数值", valueTypeName(right)));
                }
                return *leftText + *rightText;
            }
            return Value(toQuantity(left, "加法左操作数") + toQuantity(right, "加法右操作数"));
        }

        /// 数值按数量相减
        Value subtractValues(const Value &left, const Value &right)
        {
            return Value(toQuantity(left, "减法左操作数") - toQuantity(right, "减法右操作数"));
        }

        /// 数值相乘；矩阵、旋转与位姿按复合相乘，向量按缩放处理
        Value multiplyValues(const Value &left, const Value &right)
        {
            if (const auto *leftRotation = std::get_if<Base::Rotation>(&left))
            {
                if (const auto *rightRotation = std::get_if<Base::Rotation>(&right))
                {
                    return *leftRotation * *rightRotation;
                }
            }
            if (const auto *leftPlacement = std::get_if<Base::Placement>(&left))
            {
                if (const auto *rightPlacement = std::get_if<Base::Placement>(&right))
                {
                    return *leftPlacement * *rightPlacement;
                }
            }
            if (const auto *leftMatrix = std::get_if<Base::Matrix4D>(&left))
            {
                if (const auto *rightMatrix = std::get_if<Base::Matrix4D>(&right))
                {
                    return *leftMatrix * *rightMatrix;
                }
            }
            if (const auto *vector = std::get_if<Base::Vector3d>(&left))
            {
                return *vector * toDouble(right, "向量的乘数");
            }
            if (const auto *vector = std::get_if<Base::Vector3d>(&right))
            {
                return *vector * toDouble(left, "向量的乘数");
            }
            return Value(toQuantity(left, "乘法左操作数") * toQuantity(right, "乘法右操作数"));
        }

        /// 数值相除；除数为零时报错而不是产出 inf
        Value divideValues(const Value &left, const Value &right)
        {
            const Units::Quantity leftQuantity  = toQuantity(left, "除法左操作数");
            const Units::Quantity rightQuantity = toQuantity(right, "除法右操作数");
            if (rightQuantity.getValue() == 0.0)
            {
                throw Base::ValueError("除法的除数为零，无法求值；请检查表达式里的分母");
            }
            return Value(leftQuantity / rightQuantity);
        }

        /// 取余；量纲不一致或除数为零时报错
        Value moduloValues(const Value &left, const Value &right)
        {
            const Units::Quantity leftQuantity  = toQuantity(left, "取余的左操作数");
            const Units::Quantity rightQuantity = toQuantity(right, "取余的右操作数");
            if (leftQuantity.getUnit() != rightQuantity.getUnit() && !leftQuantity.isDimensionless() && !rightQuantity.isDimensionless())
            {
                throw Base::UnitsMismatchError(std::format("取余要求两侧单位一致或同为纯数：左侧是 {}，右侧是 {}；"
                                                           "请先换算成同一单位",
                                                           leftQuantity.getUnit().getString(), rightQuantity.getUnit().getString()));
            }
            if (rightQuantity.getValue() == 0.0)
            {
                throw Base::ValueError("取余的模数为零，无法求值；请检查表达式里的模数");
            }
            return Value(Units::Quantity(std::fmod(leftQuantity.getValue(), rightQuantity.getValue()), leftQuantity.getUnit()));
        }

        /// 幂运算；底数带量纲时由 Quantity::pow 要求整数指数，量纲不一致时抛 UnitsMismatchError
        Value powerValues(const Value &left, const Value &right)
        {
            const Units::Quantity base     = toQuantity(left, "幂运算的底数");
            const Units::Quantity exponent = toQuantity(right, "幂运算的指数");
            return Value(base.pow(exponent));
        }

        /// 取负；数值与向量支持取负
        Value negateValue(const Value &value)
        {
            if (const auto *quantity = std::get_if<Units::Quantity>(&value))
            {
                return -*quantity;
            }
            if (const auto *number = std::get_if<double>(&value))
            {
                return -*number;
            }
            if (const auto *vector = std::get_if<Base::Vector3d>(&value))
            {
                return *vector * -1.0;
            }
            throw Base::TypeError(std::format("取负运算符不能作用于{}；请改用数值或向量，反向的几何变换请用 minvert()", valueTypeName(value)));
        }

        /**
         * @brief 聚合收集器基类
         * @details 第一个值的单位决定结果的单位；单位不一致时由量的运算报错，不会静默按数值硬加。
         */
        class Collector
        {
        public:
            Collector() = default;

            virtual ~Collector() = default;

            /**
             * @brief 接收一个待聚合的量
             * @param value 本次收集到的量；首个值的单位会成为结果的单位
             */
            virtual void collect(const Units::Quantity &value)
            {
                if (m_first)
                {
                    m_result.setUnit(value.getUnit());
                }
            }

            /**
             * @brief 取聚合结果
             * @return 按各收集器的口径算出的结果
             */
            [[nodiscard]] virtual Units::Quantity getQuantity() const
            {
                return m_result;
            }

        protected:
            bool            m_first{true}; ///< 是否还没收集到任何值
            Units::Quantity m_result;      ///< 累计结果
        };

        /// 求和
        class SumCollector : public Collector
        {
        public:
            /**
             * @brief 累加一个量
             * @details 重写 Collector::collect()：在基类初始化单位之后做加法，结果与基类同为
             *          首个值带单位的量。
             * @param value 本次收集到的量
             */
            void collect(const Units::Quantity &value) override
            {
                Collector::collect(value);
                m_result += value;
                m_first = false;
            }
        };

        /// 求平均
        class AverageCollector : public Collector
        {
        public:
            /**
             * @brief 累加一个量并计数
             * @details 重写 Collector::collect()：基类只负责初始化单位，本类额外累计条目数，
             *          供 getQuantity() 作除数。
             * @param value 本次收集到的量
             */
            void collect(const Units::Quantity &value) override
            {
                Collector::collect(value);
                m_result += value;
                ++m_count;
                m_first = false;
            }

            /**
             * @brief 取平均值
             * @details 重写 Collector::getQuantity()：用累计和除以条目数；没有条目时求平均没有
             *          定义，报错而不是返回 inf。
             * @return 平均值，单位与首个值相同
             * @throws Base::ValueError 一个值都没有收到
             */
            [[nodiscard]] Units::Quantity getQuantity() const override
            {
                if (m_count == 0)
                {
                    // 没有任何可聚合的数值时求平均没有定义，报错好过返回 inf
                    throw Base::ValueError("average() 至少需要一个数值参数；请检查参数是否为数值属性或数量");
                }
                return m_result / static_cast<double>(m_count);
            }

        private:
            unsigned int m_count{0}; ///< 已收集的条目数
        };

        /// 求样本标准差（Welford 递推，量纲与数据一致）
        class StandardDeviationCollector : public Collector
        {
        public:
            /**
             * @brief 按 Welford 递推累加一个量
             * @details 重写 Collector::collect()：基类只负责初始化单位，本类额外维护均值与偏差
             *          平方累加，避免两遍扫描。
             * @param value 本次收集到的量
             */
            void collect(const Units::Quantity &value) override
            {
                Collector::collect(value);
                if (m_first)
                {
                    // 偏差平方累加器的量纲是数据量纲的平方，均值与数据同量纲
                    m_squaredDeviationSum = Units::Quantity(0.0, value.getUnit() * value.getUnit());
                    m_mean                = Units::Quantity(0.0, value.getUnit());
                    m_count               = 0;
                }

                const Units::Quantity delta = value - m_mean;
                ++m_count;
                m_mean                = m_mean + delta / static_cast<double>(m_count);
                m_squaredDeviationSum = m_squaredDeviationSum + delta * (value - m_mean);
                m_first               = false;
            }

            /**
             * @brief 取样本标准差
             * @details 重写 Collector::getQuantity()：样本标准差要求至少两个样本，单个样本上
             *          没有定义，与 FreeCAD 一致地报错。
             * @return 样本标准差，单位与数据相同
             * @throws EvaluationError 收集到的条目少于两个
             */
            [[nodiscard]] Units::Quantity getQuantity() const override
            {
                if (m_count < 2)
                {
                    // 单个样本上样本标准差没有定义，与 FreeCAD 一致地报错
                    throw EvaluationError("stddev() 至少需要两个数值参数；请补充样本或改用 average()");
                }
                return Units::Quantity((m_squaredDeviationSum / (m_count - 1.0)).pow(Units::Quantity(0.5)).getValue(), m_mean.getUnit());
            }

        private:
            unsigned int    m_count{0};            ///< 已收集的条目数
            Units::Quantity m_mean;                ///< 均值
            Units::Quantity m_squaredDeviationSum; ///< 偏差平方累加
        };

        /// 计数
        class CountCollector : public Collector
        {
        public:
            /**
             * @brief 只计数，不看数值
             * @details 重写 Collector::collect()：count() 与单位无关，因此忽略实参内容，
             *          只累计条目数。
             * @param value 本次收集到的量，本类不使用其取值
             */
            void collect(const Units::Quantity &) override
            {
                ++m_count;
                m_first = false;
            }

            /**
             * @brief 取条目数
             * @details 重写 Collector::getQuantity()：结果是无量纲的整数，不继承首个值的单位。
             * @return 条目数
             */
            [[nodiscard]] Units::Quantity getQuantity() const override
            {
                return Units::Quantity(static_cast<double>(m_count));
            }

        private:
            unsigned int m_count{0}; ///< 已收集的条目数
        };

        /// 求最小
        class MinimumCollector : public Collector
        {
        public:
            /**
             * @brief 保留最小值
             * @details 重写 Collector::collect()：基类初始化单位之后逐个比较；首个值没有比较对象，
             *          直接成为当前最小。
             * @param value 本次收集到的量
             */
            void collect(const Units::Quantity &value) override
            {
                Collector::collect(value);
                if (m_first || value < m_result)
                {
                    m_result = value;
                }
                m_first = false;
            }
        };

        /// 求最大
        class MaximumCollector : public Collector
        {
        public:
            /**
             * @brief 保留最大值
             * @details 重写 Collector::collect()：基类初始化单位之后逐个比较；首个值没有比较对象，
             *          直接成为当前最大。
             * @param value 本次收集到的量
             */
            void collect(const Units::Quantity &value) override
            {
                Collector::collect(value);
                if (m_first || value > m_result)
                {
                    m_result = value;
                }
                m_first = false;
            }
        };

        /// 逻辑与：全部非零才为 1
        class AndCollector : public Collector
        {
        public:
            /**
             * @brief 按逻辑与累积真值
             * @details 重写 Collector::collect()：与单位无关，首个值决定初值，之后只要出现假值就
             *          固定为 0（短路语义，后续值不再改变结果）。
             * @param value 本次收集到的量
             */
            void collect(const Units::Quantity &value) override
            {
                if (m_first)
                {
                    m_result = Units::Quantity(asBoolean(value.getValue()) ? 1.0 : 0.0);
                    m_first  = false;
                    return;
                }
                if (!asBoolean(value.getValue()))
                {
                    m_result = Units::Quantity(0.0);
                }
            }
        };

        /// 逻辑或：任一非零即为 1
        class OrCollector : public Collector
        {
        public:
            /**
             * @brief 按逻辑或累积真值
             * @details 重写 Collector::collect()：与单位无关，首个值决定初值，之后只要出现真值就
             *          固定为 1（短路语义，后续值不再改变结果）。
             * @param value 本次收集到的量
             */
            void collect(const Units::Quantity &value) override
            {
                if (m_first)
                {
                    m_result = Units::Quantity(asBoolean(value.getValue()) ? 1.0 : 0.0);
                    m_first  = false;
                    return;
                }
                if (asBoolean(value.getValue()))
                {
                    m_result = Units::Quantity(1.0);
                }
            }
        };

        /// 收集区间里各单元格的取值
        void collectRangeCells(const Expression &context, const RangeExpression &rangeExpression, Collector &collector)
        {
            const Range      range          = rangeExpression.getRange();
            IObjectResolver *objectResolver = context.resolver();
            if (objectResolver == nullptr)
            {
                throw Base::NameError(std::format("聚合函数要读取单元格区间 {}，但表达式没有绑定对象解析器"
                                                  "（IObjectResolver）；请为表达式注入宿主实现的解析器，"
                                                  "或改用具体属性引用",
                                                  range.rangeText()));
            }

            // 区间只写单元格地址，因此单元格属于当前对象；解析器对空文档名与空对象名的约定见
            // PropertyModel.h
            IObject *container = objectResolver->resolve("", "");
            if (container == nullptr)
            {
                throw Base::NameError(std::format("聚合函数要读取单元格区间 {}，但解析器没有给出当前对象；"
                                                  "请在 resolve(\"\", \"\") 中返回表达式所属对象",
                                                  range.rangeText()));
            }

            do
            {
                IProperty *property = container->findProperty(range.address());
                if (property == nullptr)
                {
                    // 与 FreeCAD 一致：区间里空着的单元格跳过，不算错误
                    continue;
                }
                const std::optional<Value> value = property->value();
                if (!value.has_value())
                {
                    throw Base::AttributeError(std::format("单元格 {} 尚未赋值，聚合函数无法取值；请先给该单元格赋值", range.address()));
                }
                collector.collect(toQuantity(*value, std::format("单元格 {}", range.address())));
            } while (range.next());
        }

        /// 按聚合函数种类收集所有实参
        Value collectAggregate(const Expression &context, FunctionExpression::Function function, const std::vector<ExpressionPtr> &arguments)
        {
            std::unique_ptr<Collector> collector;
            using Function = FunctionExpression::Function;
            switch (function)
            {
                case Function::Sum:
                    collector = std::make_unique<SumCollector>();
                    break;
                case Function::Average:
                    collector = std::make_unique<AverageCollector>();
                    break;
                case Function::StandardDeviation:
                    collector = std::make_unique<StandardDeviationCollector>();
                    break;
                case Function::Count:
                    collector = std::make_unique<CountCollector>();
                    break;
                case Function::Minimum:
                    collector = std::make_unique<MinimumCollector>();
                    break;
                case Function::Maximum:
                    collector = std::make_unique<MaximumCollector>();
                    break;
                case Function::LogicalAnd:
                    collector = std::make_unique<AndCollector>();
                    break;
                case Function::LogicalOr:
                    collector = std::make_unique<OrCollector>();
                    break;
                default:
                    // 只有聚合函数会走到这里，落到 default 说明函数表与收集器实现脱节
                    throw EvaluationError("内部错误：聚合函数表与收集器实现不一致；请报告该表达式");
            }

            for (const auto &argument: arguments)
            {
                if (const auto *rangeExpression = argument->asRangeExpression())
                {
                    collectRangeCells(context, *rangeExpression, *collector);
                    continue;
                }

                // 类型不符直接报错而不是跳过：静默跳过会让 sum() 悄悄给出偏小的结果
                const Value value     = argument->evaluate();
                double      magnitude = 0.0;
                if (!numericMagnitude(value, magnitude))
                {
                    throw Base::TypeError(std::format("聚合函数的实参需要数值，实际是{}；请改用数值属性，"
                                                      "或改用 count() 之外的处理方式",
                                                      valueTypeName(value)));
                }
                collector->collect(toQuantity(value, "聚合函数的实参"));
            }

            return collector->getQuantity();
        }

    } // namespace

    //
    // 分量
    //

    Expression::Component::Component(std::string componentName) : kind(ComponentKind::Name), name(std::move(componentName))
    {
    }

    Expression::Component::Component(const Component &other) :
        kind(other.kind), name(other.name), index(other.index ? other.index->copy() : nullptr), endIndex(other.endIndex ? other.endIndex->copy() : nullptr),
        step(other.step ? other.step->copy() : nullptr)
    {
    }

    Expression::Component::Component(Component &&other) noexcept = default;

    Expression::Component::~Component() = default;

    Expression::Component &Expression::Component::operator=(const Component &other)
    {
        if (this == &other)
        {
            return *this;
        }
        kind     = other.kind;
        name     = other.name;
        index    = other.index ? other.index->copy() : nullptr;
        endIndex = other.endIndex ? other.endIndex->copy() : nullptr;
        step     = other.step ? other.step->copy() : nullptr;
        return *this;
    }

    Expression::Component &Expression::Component::operator=(Component &&other) noexcept = default;

    Expression::Component Expression::Component::arrayIndex(ExpressionPtr indexExpression)
    {
        Component component;
        component.kind  = ComponentKind::Index;
        component.index = std::move(indexExpression);
        return component;
    }

    Expression::Component Expression::Component::mapKey(std::string key)
    {
        Component component;
        component.kind = ComponentKind::MapKey;
        component.name = std::move(key);
        return component;
    }

    Expression::Component Expression::Component::rangeComponent(ExpressionPtr begin, ExpressionPtr end, ExpressionPtr stepExpression)
    {
        Component component;
        component.kind     = ComponentKind::Range;
        component.index    = std::move(begin);
        component.endIndex = std::move(end);
        component.step     = std::move(stepExpression);
        return component;
    }

    bool Expression::Component::isSame(const Component &other) const
    {
        if (kind != other.kind || name != other.name)
        {
            return false;
        }
        return expressionsSame(index.get(), other.index.get()) && expressionsSame(endIndex.get(), other.endIndex.get()) && expressionsSame(step.get(), other.step.get());
    }

    void Expression::Component::appendText(std::string &text, bool persistent) const
    {
        switch (kind)
        {
            case ComponentKind::Name:
                // 名字分量用点号连接，如 .Rotation
                text += '.';
                text += name;
                return;
            case ComponentKind::MapKey:
                text += '[';
                text += quoteText(name);
                text += ']';
                return;
            case ComponentKind::Index:
                text += '[';
                if (index != nullptr)
                {
                    text += index->toString(persistent);
                }
                text += ']';
                return;
            case ComponentKind::Range:
                text += '[';
                if (index != nullptr)
                {
                    text += index->toString(persistent);
                }
                text += ':';
                if (endIndex != nullptr)
                {
                    text += endIndex->toString(persistent);
                }
                if (step != nullptr)
                {
                    text += ':';
                    text += step->toString(persistent);
                }
                text += ']';
                return;
        }
    }

    //
    // 表达式基类
    //

    Expression::Expression(IObjectResolver *resolver) : m_resolver(resolver)
    {
    }

    Expression::~Expression() = default;

    IObjectResolver *Expression::resolver() const noexcept
    {
        return m_resolver;
    }

    Value Expression::evaluate() const
    {
        if (!m_components.empty() && !supportsComponentAccess())
        {
            // 分量目前只接在能解析到宿主属性的引用上；其余节点若静默忽略，表达式会取到
            // 整体值而不是子值。这里在读数之前拦下，避免连宿主属性都不必要地读一遍
            throw EvaluationError(std::format("表达式 {} 带有分量或下标，但当前节点无法按分量取值；"
                                              "分量访问要求引用能解析到宿主属性（请给表达式注入对象解析器），"
                                              "区间分量只能作为聚合函数的实参",
                                              toString(true)));
        }
        return evaluateNode();
    }

    ExpressionPtr Expression::eval() const
    {
        return makeValueExpression(m_resolver, evaluate());
    }

    std::vector<VariableReference> Expression::collectReferences() const
    {
        std::vector<VariableReference> references;
        _collectReferences(references);

        // 同一引用可能沿多条路径出现（如 Box.Length + Box.Length）；按首次出现顺序去重，
        // 宿主建立依赖时不必自己再排一遍
        std::vector<VariableReference> unique;
        unique.reserve(references.size());
        for (const auto &reference: references)
        {
            bool seen = false;
            for (const auto &existing: unique)
            {
                if (existing.documentName == reference.documentName && existing.objectName == reference.objectName && existing.propertyName == reference.propertyName)
                {
                    seen = true;
                    break;
                }
            }
            if (!seen)
            {
                unique.push_back(reference);
            }
        }
        return unique;
    }

    void Expression::collectReferencesFrom(const Expression *expression, std::vector<VariableReference> &out)
    {
        if (expression != nullptr)
        {
            expression->_collectReferences(out);
        }
    }

    void Expression::_collectReferences(std::vector<VariableReference> &) const
    {
        // 基类不知道子节点结构：默认不收集，复合节点覆写后递归子表达式
    }

    bool Expression::supportsComponentAccess() const noexcept
    {
        // 默认节点没有可施加分量的宿主取值；引用节点按需覆写
        return false;
    }

    int Expression::priority() const
    {
        return s_defaultPriority;
    }

    std::string Expression::toString(bool persistent, bool checkPriority, int indent) const
    {
        std::string text;
        if (m_components.empty())
        {
            // 优先级不足的节点补括号，保证文本能原样解析回同一棵树
            const bool needsParentheses = checkPriority && priority() < s_defaultPriority;
            if (needsParentheses)
            {
                text += '(';
            }
            appendText(text, persistent, indent);
            if (needsParentheses)
            {
                text += ')';
            }
            return text;
        }

        if (!isIndexable())
        {
            text += '(';
            appendText(text, persistent, indent);
            text += ')';
        } else
        {
            appendText(text, persistent, indent);
        }
        for (const auto &component: m_components)
        {
            component.appendText(text, persistent);
        }
        return text;
    }

    ExpressionPtr Expression::copy() const
    {
        ExpressionPtr result = copyNode();
        result->m_resolver   = m_resolver;
        result->m_components = m_components;
        result->m_comment    = m_comment;
        return result;
    }

    bool Expression::isSame(const Expression &other, bool checkComment) const
    {
        if (&other == this)
        {
            return true;
        }
        if (nodeName() != other.nodeName())
        {
            return false;
        }
        if (checkComment && m_comment != other.m_comment)
        {
            return false;
        }
        return toString(true, true) == other.toString(true, true);
    }

    bool Expression::hasComponent() const noexcept
    {
        return !m_components.empty();
    }

    const Expression::ComponentList &Expression::components() const noexcept
    {
        return m_components;
    }

    void Expression::addComponent(Component component)
    {
        m_components.push_back(std::move(component));
    }

    const std::string &Expression::comment() const noexcept
    {
        return m_comment;
    }

    void Expression::setComment(std::string text)
    {
        m_comment = std::move(text);
    }

    const RangeExpression *Expression::asRangeExpression() const noexcept
    {
        return nullptr;
    }

    const OperatorExpression *Expression::asOperatorExpression() const noexcept
    {
        return nullptr;
    }

    bool Expression::isConstantNumeric() const noexcept
    {
        return false;
    }

    bool Expression::isIndexable() const
    {
        return false;
    }

    ExpressionPtr makeValueExpression(IObjectResolver *resolver, const Value &value)
    {
        if (const auto *quantity = std::get_if<Units::Quantity>(&value))
        {
            return std::make_unique<NumberExpression>(resolver, *quantity);
        }
        if (const auto *number = std::get_if<double>(&value))
        {
            return std::make_unique<NumberExpression>(resolver, Units::Quantity(*number));
        }
        if (const auto *boolean = std::get_if<bool>(&value))
        {
            return std::make_unique<ConstantExpression>(resolver, *boolean ? "True" : "False", Units::Quantity(*boolean ? 1.0 : 0.0));
        }
        if (const auto *text = std::get_if<std::string>(&value))
        {
            return std::make_unique<StringExpression>(resolver, *text);
        }
        return std::make_unique<ValueExpression>(resolver, value);
    }

    //
    // 单位节点
    //

    UnitExpression::UnitExpression(IObjectResolver *resolver, const Units::Quantity &quantity, const std::string &unitText) :
        Expression(resolver), m_quantity(quantity), m_unitText(unitText)
    {
    }

    UnitExpression::~UnitExpression() = default;

    void UnitExpression::setQuantity(const Units::Quantity &quantity)
    {
        m_quantity = quantity;
    }

    void UnitExpression::setUnit(const Units::Quantity &quantity)
    {
        m_quantity = quantity;
    }

    double UnitExpression::getValue() const
    {
        return m_quantity.getValue();
    }

    const Units::Unit &UnitExpression::getUnit() const
    {
        return m_quantity.getUnit();
    }

    const Units::Quantity &UnitExpression::getQuantity() const
    {
        return m_quantity;
    }

    std::string UnitExpression::getUnitText() const
    {
        return m_unitText;
    }

    double UnitExpression::getScaler() const
    {
        return m_quantity.getValue();
    }

    ExpressionPtr UnitExpression::simplify() const
    {
        // 单位节点本身就是常量，化简成数值节点
        return std::make_unique<NumberExpression>(resolver(), m_quantity);
    }

    std::string_view UnitExpression::nodeName() const
    {
        return "Unit";
    }

    Value UnitExpression::evaluateNode() const
    {
        return Value(m_quantity);
    }

    void UnitExpression::appendText(std::string &text, bool, int) const
    {
        text += m_unitText;
    }

    ExpressionPtr UnitExpression::copyNode() const
    {
        return std::make_unique<UnitExpression>(resolver(), m_quantity, m_unitText);
    }

    //
    // 数值节点
    //

    NumberExpression::NumberExpression(IObjectResolver *resolver, const Units::Quantity &quantity) : UnitExpression(resolver, quantity)
    {
    }

    ExpressionPtr NumberExpression::simplify() const
    {
        return copy();
    }

    void NumberExpression::negate()
    {
        setQuantity(-getQuantity());
    }

    std::optional<long> NumberExpression::integerValue() const
    {
        long result = 0;
        if (!essentiallyInteger(getValue(), result))
        {
            return std::nullopt;
        }
        return result;
    }

    std::string_view NumberExpression::nodeName() const
    {
        return "Number";
    }

    bool NumberExpression::isConstantNumeric() const noexcept
    {
        return true;
    }

    void NumberExpression::appendText(std::string &text, bool, int) const
    {
        // 只写数值：单位由单位节点或 UNIT 运算符负责排版
        text += formatNumber(getValue());
    }

    ExpressionPtr NumberExpression::copyNode() const
    {
        return std::make_unique<NumberExpression>(resolver(), getQuantity());
    }

    //
    // 命名常量节点
    //

    ConstantExpression::ConstantExpression(IObjectResolver *resolver, std::string name, const Units::Quantity &quantity) :
        NumberExpression(resolver, quantity), m_name(std::move(name))
    {
    }

    std::string ConstantExpression::getName() const
    {
        return m_name;
    }

    bool ConstantExpression::isNumber() const
    {
        // True 与 False 是布尔常量，不按数值参与运算
        return m_name != "True" && m_name != "False";
    }

    std::string_view ConstantExpression::nodeName() const
    {
        return "Constant";
    }

    Value ConstantExpression::evaluateNode() const
    {
        if (m_name == "True")
        {
            return Value(true);
        }
        if (m_name == "False")
        {
            return Value(false);
        }
        return Value(getQuantity());
    }

    void ConstantExpression::appendText(std::string &text, bool, int) const
    {
        text += m_name;
    }

    ExpressionPtr ConstantExpression::copyNode() const
    {
        return std::make_unique<ConstantExpression>(resolver(), m_name, getQuantity());
    }

    //
    // 运算符节点
    //

    OperatorExpression::OperatorExpression(IObjectResolver *resolver, ExpressionPtr left, Operator operation, ExpressionPtr right) :
        UnitExpression(resolver), m_operator(operation), m_left(std::move(left)), m_right(std::move(right))
    {
        if (m_left == nullptr)
        {
            throw EvaluationError("运算符节点缺少左操作数；请检查表达式构造过程");
        }
        if (m_operator == Operator::None)
        {
            throw EvaluationError("运算符节点的运算符为空；请检查表达式构造过程");
        }
        const bool isUnary = m_operator == Operator::Negate || m_operator == Operator::Positive;
        if (isUnary)
        {
            if (m_right != nullptr)
            {
                throw EvaluationError("一元运算符不接受右操作数；请把右操作数留空，二元运算请改用对应的运算符");
            }
            return;
        }
        if (m_right == nullptr)
        {
            throw EvaluationError("二元运算符缺少右操作数；请检查表达式构造过程");
        }
    }

    OperatorExpression::~OperatorExpression() = default;

    OperatorExpression::Operator OperatorExpression::getOperator() const noexcept
    {
        return m_operator;
    }

    const Expression *OperatorExpression::getLeft() const noexcept
    {
        return m_left.get();
    }

    const Expression *OperatorExpression::getRight() const noexcept
    {
        return m_right.get();
    }

    void OperatorExpression::setLeft(ExpressionPtr expression)
    {
        if (expression == nullptr)
        {
            throw EvaluationError("运算符的左操作数不能为空；请传入有效表达式");
        }
        m_left = std::move(expression);
    }

    void OperatorExpression::setRight(ExpressionPtr expression)
    {
        if (expression == nullptr)
        {
            throw EvaluationError("运算符的右操作数不能为空；一元运算符请改用不带右操作数的构造方式");
        }
        m_right = std::move(expression);
    }

    ExpressionPtr OperatorExpression::simplify() const
    {
        ExpressionPtr left = m_left->simplify();
        if (m_operator == Operator::Negate || m_operator == Operator::Positive)
        {
            if (left->isConstantNumeric())
            {
                return eval();
            }
            return std::make_unique<OperatorExpression>(resolver(), std::move(left), m_operator, nullptr);
        }

        ExpressionPtr right = m_right->simplify();
        if (left->isConstantNumeric() && right->isConstantNumeric())
        {
            // 两侧都是常量，结果必然也是常量，直接折叠
            return eval();
        }
        return std::make_unique<OperatorExpression>(resolver(), std::move(left), m_operator, std::move(right));
    }

    int OperatorExpression::priority() const
    {
        switch (m_operator)
        {
            case Operator::Equal:
            case Operator::NotEqual:
            case Operator::Less:
            case Operator::Greater:
            case Operator::LessEqual:
            case Operator::GreaterEqual:
                return 1;
            case Operator::Add:
            case Operator::Subtract:
                return 3;
            case Operator::Multiply:
            case Operator::Divide:
            case Operator::Modulo:
                return 4;
            case Operator::Power:
                return 5;
            case Operator::UnitScale:
            case Operator::Negate:
            case Operator::Positive:
                return 6;
            default:
                // None 在构造期已被拒绝，这里只作为兜底
                return s_defaultPriority;
        }
    }

    bool OperatorExpression::isCommutative() const
    {
        switch (m_operator)
        {
            case Operator::Equal:
            case Operator::NotEqual:
            case Operator::Add:
            case Operator::Multiply:
                return true;
            default:
                return false;
        }
    }

    bool OperatorExpression::isLeftAssociative() const
    {
        return true;
    }

    bool OperatorExpression::isRightAssociative() const
    {
        switch (m_operator)
        {
            case Operator::Add:
            case Operator::Multiply:
                return true;
            default:
                return false;
        }
    }

    std::string_view OperatorExpression::operatorText(Operator operation)
    {
        switch (operation)
        {
            case Operator::Add:
                return "+";
            case Operator::Subtract:
                return "-";
            case Operator::Multiply:
                return "*";
            case Operator::Divide:
                return "/";
            case Operator::Modulo:
                return "%";
            case Operator::Power:
                return "^";
            case Operator::Equal:
                return "==";
            case Operator::NotEqual:
                return "!=";
            case Operator::Less:
                return "<";
            case Operator::Greater:
                return ">";
            case Operator::LessEqual:
                return "<=";
            case Operator::GreaterEqual:
                return ">=";
            case Operator::UnitScale:
                return " ";
            case Operator::Negate:
                return "-";
            case Operator::Positive:
                return "+";
            default:
                return "?";
        }
    }

    OperatorExpression::Operator OperatorExpression::operatorFromText(std::string_view text)
    {
        // 减号统一解析成二元减法；一元取负由解析器按上下文改写成 Negate
        if (text == "+")
        {
            return Operator::Add;
        }
        if (text == "-")
        {
            return Operator::Subtract;
        }
        if (text == "*")
        {
            return Operator::Multiply;
        }
        if (text == "/")
        {
            return Operator::Divide;
        }
        if (text == "%")
        {
            return Operator::Modulo;
        }
        if (text == "^")
        {
            return Operator::Power;
        }
        if (text == "==")
        {
            return Operator::Equal;
        }
        if (text == "!=")
        {
            return Operator::NotEqual;
        }
        if (text == "<")
        {
            return Operator::Less;
        }
        if (text == ">")
        {
            return Operator::Greater;
        }
        if (text == "<=")
        {
            return Operator::LessEqual;
        }
        if (text == ">=")
        {
            return Operator::GreaterEqual;
        }
        return Operator::None;
    }

    std::string_view OperatorExpression::nodeName() const
    {
        return "Operator";
    }

    const OperatorExpression *OperatorExpression::asOperatorExpression() const noexcept
    {
        return this;
    }

    Value OperatorExpression::evaluateNode() const
    {
        switch (m_operator)
        {
            // 一元运算先处理，避免多求一次右操作数
            case Operator::Negate:
                return negateValue(m_left->evaluate());
            case Operator::Positive:
            {
                const Value value = m_left->evaluate();
                if (isNumeric(value) || std::holds_alternative<Base::Vector3d>(value))
                {
                    return value;
                }
                throw Base::TypeError(std::format("取正运算符不能作用于{}；请改用数值或向量", valueTypeName(value)));
            }
            // 比较运算按「小于」与「相等」组合，NaN 参与比较时结果与 FreeCAD 一致为假
            case Operator::Equal:
                return Value(valuesEqual(m_left->evaluate(), m_right->evaluate()));
            case Operator::NotEqual:
                return Value(!valuesEqual(m_left->evaluate(), m_right->evaluate()));
            case Operator::Less:
                return Value(valueLessThan(m_left->evaluate(), m_right->evaluate()));
            case Operator::Greater:
                return Value(valueLessThan(m_right->evaluate(), m_left->evaluate()));
            case Operator::LessEqual:
            {
                const Value left  = m_left->evaluate();
                const Value right = m_right->evaluate();
                return Value(valueLessThan(left, right) || valuesEqual(left, right));
            }
            case Operator::GreaterEqual:
            {
                const Value left  = m_left->evaluate();
                const Value right = m_right->evaluate();
                return Value(valueLessThan(right, left) || valuesEqual(left, right));
            }
            default:
                break;
        }

        const Value left  = m_left->evaluate();
        const Value right = m_right->evaluate();
        switch (m_operator)
        {
            case Operator::Add:
                return addValues(left, right);
            case Operator::Subtract:
                return subtractValues(left, right);
            case Operator::Multiply:
            case Operator::UnitScale:
                return multiplyValues(left, right);
            case Operator::Divide:
                return divideValues(left, right);
            case Operator::Modulo:
                return moduloValues(left, right);
            case Operator::Power:
                return powerValues(left, right);
            default:
                throw EvaluationError(std::format("运算符 '{}' 不支持求值；请检查表达式构造过程", operatorText(m_operator)));
        }
    }

    void OperatorExpression::appendText(std::string &text, bool persistent, int) const
    {
        bool needsParentheses = false;

        if (m_operator == Operator::Negate || m_operator == Operator::Positive)
        {
            // 一元运算直接贴在操作数前，操作数优先级更低时补括号
            needsParentheses = m_left->priority() < priority();
            text += m_operator == Operator::Negate ? '-' : '+';
            if (needsParentheses)
            {
                text += '(';
            }
            text += m_left->toString(persistent);
            if (needsParentheses)
            {
                text += ')';
            }
            return;
        }

        Operator leftOperator = Operator::None;
        if (const auto *leftOperatorExpression = m_left->asOperatorExpression())
        {
            leftOperator = leftOperatorExpression->getOperator();
        }
        if (m_left->priority() < priority())
        {
            // 优先级更低的操作数必须加括号，否则文本会被解析成另一棵树
            needsParentheses = true;
        } else if (leftOperator == m_operator && !isLeftAssociative())
        {
            needsParentheses = true;
        }
        if (m_operator == Operator::UnitScale && !m_left->isConstantNumeric())
        {
            // 单位写法只在左边是数值时才不加括号，如 2 mm
            needsParentheses = true;
        }

        if (needsParentheses)
        {
            text += '(';
            text += m_left->toString(persistent);
            text += ')';
        } else
        {
            text += m_left->toString(persistent);
        }

        switch (m_operator)
        {
            case Operator::Add:
            case Operator::Subtract:
            case Operator::Multiply:
            case Operator::Divide:
            case Operator::Modulo:
            case Operator::Power:
            case Operator::Equal:
            case Operator::NotEqual:
            case Operator::Less:
            case Operator::Greater:
            case Operator::LessEqual:
            case Operator::GreaterEqual:
                text += ' ';
                text += operatorText(m_operator);
                text += ' ';
                break;
            case Operator::UnitScale:
                text += ' ';
                break;
            default:
                // 一元运算在上面已经返回，走到这里说明运算符与文本化实现脱节
                throw EvaluationError(std::format("运算符 '{}' 没有文本写法；请检查表达式构造过程", operatorText(m_operator)));
        }

        needsParentheses       = false;
        Operator rightOperator = Operator::None;
        if (const auto *rightOperatorExpression = m_right->asOperatorExpression())
        {
            rightOperator = rightOperatorExpression->getOperator();
        }
        if (m_right->priority() < priority())
        {
            needsParentheses = true;
        } else if (rightOperator == m_operator)
        {
            // 同一运算符下左右结合性决定右操作数是否需要括号
            if (!isRightAssociative() || !isCommutative())
            {
                needsParentheses = true;
            }
        } else if (m_right->priority() == priority())
        {
            if (!isRightAssociative() || rightOperator == Operator::Modulo)
            {
                needsParentheses = true;
            }
        }

        if (needsParentheses)
        {
            text += '(';
            text += m_right->toString(persistent);
            text += ')';
        } else
        {
            text += m_right->toString(persistent);
        }
    }

    ExpressionPtr OperatorExpression::copyNode() const
    {
        return std::make_unique<OperatorExpression>(resolver(), m_left->copy(), m_operator, m_right ? m_right->copy() : nullptr);
    }

    //
    // 条件节点
    //

    ConditionalExpression::ConditionalExpression(IObjectResolver *resolver, ExpressionPtr condition, ExpressionPtr trueExpression, ExpressionPtr falseExpression) :
        Expression(resolver), m_condition(std::move(condition)), m_trueExpression(std::move(trueExpression)), m_falseExpression(std::move(falseExpression))
    {
        if (m_condition == nullptr || m_trueExpression == nullptr || m_falseExpression == nullptr)
        {
            throw EvaluationError("条件表达式需要条件、真分支与假分支三个子表达式；请检查表达式构造过程");
        }
    }

    ConditionalExpression::~ConditionalExpression() = default;

    const Expression *ConditionalExpression::getCondition() const noexcept
    {
        return m_condition.get();
    }

    const Expression *ConditionalExpression::getTrueExpression() const noexcept
    {
        return m_trueExpression.get();
    }

    const Expression *ConditionalExpression::getFalseExpression() const noexcept
    {
        return m_falseExpression.get();
    }

    ExpressionPtr ConditionalExpression::simplify() const
    {
        ExpressionPtr simplifiedCondition = m_condition->simplify();
        if (simplifiedCondition->isConstantNumeric())
        {
            double magnitude = 0.0;
            if (!numericMagnitude(simplifiedCondition->evaluate(), magnitude))
            {
                throw EvaluationError("条件表达式 (?:) 的条件不是数值；请改用比较运算得到布尔条件");
            }
            // 与 FreeCAD 一致：条件绝对值达到重合精度即取真分支
            if (std::fabs(magnitude) >= Base::Precision::Confusion())
            {
                return m_trueExpression->simplify();
            }
            return m_falseExpression->simplify();
        }

        return std::make_unique<ConditionalExpression>(resolver(), std::move(simplifiedCondition), m_trueExpression->simplify(), m_falseExpression->simplify());
    }

    int ConditionalExpression::priority() const
    {
        return 2;
    }

    std::string_view ConditionalExpression::nodeName() const
    {
        return "Conditional";
    }

    Value ConditionalExpression::evaluateNode() const
    {
        if (toBool(m_condition->evaluate(), "条件表达式 (?:) 的条件"))
        {
            return m_trueExpression->evaluate();
        }
        return m_falseExpression->evaluate();
    }

    void ConditionalExpression::appendText(std::string &text, bool persistent, int) const
    {
        text += m_condition->toString(persistent);
        text += " ? ";

        // 分支优先级不高于条件运算符时补括号，保证文本能原样解析回来
        if (m_trueExpression->priority() <= priority())
        {
            text += '(';
            text += m_trueExpression->toString(persistent);
            text += ')';
        } else
        {
            text += m_trueExpression->toString(persistent);
        }

        text += " : ";

        if (m_falseExpression->priority() <= priority())
        {
            text += '(';
            text += m_falseExpression->toString(persistent);
            text += ')';
        } else
        {
            text += m_falseExpression->toString(persistent);
        }
    }

    ExpressionPtr ConditionalExpression::copyNode() const
    {
        return std::make_unique<ConditionalExpression>(resolver(), m_condition->copy(), m_trueExpression->copy(), m_falseExpression->copy());
    }

    //
    // 函数节点
    //

    FunctionExpression::FunctionExpression(IObjectResolver *resolver, Function function, std::string name, std::vector<ExpressionPtr> arguments) :
        UnitExpression(resolver), m_function(function), m_name(std::move(name)), m_arguments(std::move(arguments))
    {
        for (const auto &argument: m_arguments)
        {
            if (argument == nullptr)
            {
                throw EvaluationError("函数实参不能为空；请检查表达式构造过程");
            }
        }
        const std::string_view label = m_name.empty() ? functionName(m_function) : std::string_view(m_name);
        validateFunctionCall(m_function, m_arguments.size(), label);
    }

    FunctionExpression::~FunctionExpression() = default;

    FunctionExpression::Function FunctionExpression::getFunction() const noexcept
    {
        return m_function;
    }

    const std::vector<ExpressionPtr> &FunctionExpression::getArguments() const noexcept
    {
        return m_arguments;
    }

    ExpressionPtr FunctionExpression::simplify() const
    {
        std::size_t                numericCount = 0;
        std::vector<ExpressionPtr> simplifiedArguments;
        simplifiedArguments.reserve(m_arguments.size());

        for (const auto &argument: m_arguments)
        {
            ExpressionPtr simplified = argument->simplify();
            if (simplified->isConstantNumeric())
            {
                ++numericCount;
            }
            simplifiedArguments.push_back(std::move(simplified));
        }

        if (numericCount == m_arguments.size())
        {
            // 全部实参都是常量，函数结果必然也是常量，直接折叠
            return eval();
        }
        return std::make_unique<FunctionExpression>(resolver(), m_function, m_name, std::move(simplifiedArguments));
    }

    std::string_view FunctionExpression::nodeName() const
    {
        return "Function";
    }

    Value FunctionExpression::evaluateNode() const
    {
        return evaluateFunction(*this, m_function, m_arguments);
    }

    Value FunctionExpression::evaluateAggregate(const Expression &context, Function function, const std::vector<ExpressionPtr> &arguments)
    {
        return collectAggregate(context, function, arguments);
    }

    Value FunctionExpression::evaluateFunction(const Expression &context, Function function, const std::vector<ExpressionPtr> &arguments)
    {
        using Function = FunctionExpression::Function;

        if (isAggregate(function))
        {
            return evaluateAggregate(context, function, arguments);
        }

        if (arguments.empty())
        {
            throw EvaluationError(std::format("{}() 至少需要 1 个参数；请补上实参", functionName(function)));
        }

        const std::string_view label = functionName(function);
        switch (function)
        {
            case Function::MatrixInvert:
            {
                const Value target = arguments[0]->evaluate();
                if (const auto *matrix = std::get_if<Base::Matrix4D>(&target))
                {
                    if (std::fabs(matrix->determinant()) <= std::numeric_limits<double>::epsilon())
                    {
                        throw Base::ValueError("minvert() 的矩阵不可逆（行列式接近 0）；请检查矩阵是否退化，"
                                               "或改用可逆的构造方式");
                    }
                    Base::Matrix4D inverted = *matrix;
                    inverted.inverseGauss();
                    return inverted;
                }
                if (const auto *placement = std::get_if<Base::Placement>(&target))
                {
                    return placement->inverse();
                }
                if (const auto *rotation = std::get_if<Base::Rotation>(&target))
                {
                    return rotation->inverse();
                }
                throw Base::TypeError(std::format("minvert() 需要矩阵、位姿或旋转，实际是{}；"
                                                  "请用 matrix()、placement()、rotation() 构造这类取值",
                                                  valueTypeName(target)));
            }
            case Function::MatrixRotate:
            {
                Base::Rotation rotation;
                if (arguments.size() == 4)
                {
                    // 三个数值按 yaw、pitch、roll 解释，与 FreeCAD 一致按弧度给出
                    rotation.setYawPitchRoll(numberArgument(arguments, 1, label), numberArgument(arguments, 2, label), numberArgument(arguments, 3, label));
                } else
                {
                    const Value second = arguments[1]->evaluate();
                    if (const auto *given = std::get_if<Base::Rotation>(&second))
                    {
                        if (arguments.size() != 2)
                        {
                            throwArgumentCount(label, "给旋转对象时 2 个参数", arguments.size());
                        }
                        rotation = *given;
                    } else if (const auto *matrix = std::get_if<Base::Matrix4D>(&second))
                    {
                        rotation = Base::Rotation(*matrix);
                    } else if (const auto *axis = std::get_if<Base::Vector3d>(&second))
                    {
                        if (arguments.size() != 3)
                        {
                            throwArgumentCount(label, "给轴向量时 3 个参数", arguments.size());
                        }
                        const Value third = arguments[2]->evaluate();
                        if (const auto *endDirection = std::get_if<Base::Vector3d>(&third))
                        {
                            rotation = Base::Rotation(*axis, *endDirection);
                        } else
                        {
                            rotation = Base::Rotation(*axis, angleArgument(third, "mrotate() 的第三个参数"));
                        }
                    } else
                    {
                        throw Base::TypeError(std::format("mrotate() 的第二个参数需要旋转、矩阵或轴向量，实际是{}；"
                                                          "请改用 mrotate(对象; rotation(...)) 或 mrotate(对象; 轴; 角度)",
                                                          valueTypeName(second)));
                    }
                }

                Base::Matrix4D rotationMatrix;
                rotation.getValue(rotationMatrix);
                return transformFirstArgument(arguments, rotationMatrix, label);
            }
            case Function::MatrixRotateX:
            case Function::MatrixRotateY:
            case Function::MatrixRotateZ:
            {
                const double         angle = angleArgument(arguments[1]->evaluate(), std::format("{}() 的第二个参数", label));
                const Base::Rotation rotation(Base::Vector3d(function == Function::MatrixRotateX ? 1.0 : 0.0, function == Function::MatrixRotateY ? 1.0 : 0.0,
                                                             function == Function::MatrixRotateZ ? 1.0 : 0.0),
                                              angle);
                Base::Matrix4D       rotationMatrix;
                rotation.getValue(rotationMatrix);
                return transformFirstArgument(arguments, rotationMatrix, label);
            }
            case Function::MatrixScale:
            {
                const Base::Vector3d scale = secondVectorArgument(arguments, label);
                Base::Matrix4D       scaleMatrix;
                scaleMatrix.scale(scale);
                return transformFirstArgument(arguments, scaleMatrix, label);
            }
            case Function::MatrixTranslate:
            {
                const Base::Vector3d translation = secondVectorArgument(arguments, label);
                Base::Matrix4D       translationMatrix;
                translationMatrix.move(translation);

                const Value target = arguments[0]->evaluate();
                if (const auto *rotation = std::get_if<Base::Rotation>(&target))
                {
                    // 旋转本身没有平移分量，平移量单独补成位姿，与 FreeCAD 一致
                    Base::Matrix4D rotationMatrix;
                    rotation->getValue(rotationMatrix);
                    return Base::Placement(translationMatrix * rotationMatrix);
                }
                return transformFirstArgument(arguments, translationMatrix, label);
            }
            case Function::Matrix:
            {
                if (arguments.empty())
                {
                    return Base::Matrix4D();
                }
                if (arguments.size() != 16)
                {
                    throw EvaluationError(std::format("matrix() 只支持 0 个参数（单位矩阵）或 16 个参数"
                                                      "（按行给出 4x4 元素），实际收到 {} 个；"
                                                      "按行向量构造的写法依赖宿主的序列类型，本库不支持",
                                                      arguments.size()));
                }
                std::array<double, 16> elements{};
                for (std::size_t index = 0; index < elements.size(); ++index)
                {
                    elements[index] = numberArgument(arguments, index, label);
                }
                return Base::Matrix4D(elements[0], elements[1], elements[2], elements[3], elements[4], elements[5], elements[6], elements[7], elements[8], elements[9],
                                      elements[10], elements[11], elements[12], elements[13], elements[14], elements[15]);
            }
            case Function::Placement:
            {
                if (arguments.empty())
                {
                    return Base::Placement();
                }
                if (arguments.size() == 1)
                {
                    const Value target = arguments[0]->evaluate();
                    if (const auto *matrix = std::get_if<Base::Matrix4D>(&target))
                    {
                        return Base::Placement(*matrix);
                    }
                    if (const auto *placement = std::get_if<Base::Placement>(&target))
                    {
                        return *placement;
                    }
                    throw Base::TypeError(std::format("placement() 只给一个参数时需要矩阵或位姿，实际是{}；"
                                                      "请改用 placement(位置; 旋转) 或 placement(matrix(...))",
                                                      valueTypeName(target)));
                }

                const Base::Vector3d position      = vectorArgument(arguments, 0, label);
                const Value          rotationValue = arguments[1]->evaluate();
                const auto          *rotation      = std::get_if<Base::Rotation>(&rotationValue);
                if (rotation == nullptr)
                {
                    throw Base::TypeError(std::format("placement() 的第二个参数需要旋转，实际是{}；"
                                                      "请用 rotation(...) 构造旋转",
                                                      valueTypeName(rotationValue)));
                }
                if (arguments.size() == 2)
                {
                    return Base::Placement(position, *rotation);
                }
                return Base::Placement(position, *rotation, vectorArgument(arguments, 2, label));
            }
            case Function::Rotation:
            {
                const Value first = arguments[0]->evaluate();
                if (arguments.size() == 3)
                {
                    // 三个数值按 yaw、pitch、roll 解释，与 FreeCAD 一致按弧度给出
                    Base::Rotation rotation;
                    rotation.setYawPitchRoll(numberArgument(arguments, 0, label), numberArgument(arguments, 1, label), numberArgument(arguments, 2, label));
                    return rotation;
                }

                const auto *axis = std::get_if<Base::Vector3d>(&first);
                if (axis == nullptr)
                {
                    throw Base::TypeError(std::format("rotation() 的第一个参数需要轴向量，实际是{}；"
                                                      "请改用 rotation(轴; 角度) 或 rotation(起点; 终点)",
                                                      valueTypeName(first)));
                }
                const Value second = arguments[1]->evaluate();
                if (const auto *endDirection = std::get_if<Base::Vector3d>(&second))
                {
                    // 两个向量表示从起点方向转到终点方向
                    return Base::Rotation(*axis, *endDirection);
                }
                // 轴加角度：与 FreeCAD 的 rotation(axis, angle) 一致，角度按度给出
                return Base::Rotation(*axis, angleArgument(second, "rotation() 的第二个参数"));
            }
            case Function::Vector:
                return Base::Vector3d(numberArgument(arguments, 0, label), numberArgument(arguments, 1, label), numberArgument(arguments, 2, label));
            case Function::Stringify:
                return valueText(arguments[0]->evaluate());
            case Function::ParseQuantity:
            {
                const Value       value        = arguments[0]->evaluate();
                const auto       *text         = std::get_if<std::string>(&value);
                const std::string quantityText = text != nullptr ? *text : valueText(value);
                try
                {
                    return Units::Quantity::parse(quantityText);
                } catch (const Base::Exception &error)
                {
                    throw Base::ParserError(std::format("parsequant() 无法把 '{}' 解析成数量（{}）；"
                                                        "请改用 '1.5 mm'、'90 deg' 这样的文本",
                                                        quantityText, error.message()));
                }
            }
            case Function::TranslationMatrix:
            {
                if (arguments.size() == 1)
                {
                    const Base::Vector3d translation = vectorArgument(arguments, 0, label);
                    Base::Matrix4D       matrix;
                    matrix.move(translation);
                    return matrix;
                }
                const double   x = lengthArgument(arguments, 0, label);
                const double   y = lengthArgument(arguments, 1, label);
                const double   z = lengthArgument(arguments, 2, label);
                Base::Matrix4D matrix;
                matrix.move(x, y, z);
                return matrix;
            }
            case Function::Address:
            {
                long                  row            = 0;
                long                  column         = 0;
                const Units::Quantity rowQuantity    = toQuantity(arguments[0]->evaluate(), "address() 的第一个参数");
                const Units::Quantity columnQuantity = toQuantity(arguments[1]->evaluate(), "address() 的第二个参数");
                if (!rowQuantity.isDimensionless() || !columnQuantity.isDimensionless())
                {
                    throw Base::UnitsMismatchError("address() 的行号与列号必须是纯数；请去掉单位");
                }
                if (!essentiallyInteger(rowQuantity.getValue(), row) || !essentiallyInteger(columnQuantity.getValue(), column))
                {
                    throw Base::TypeError("address() 的行号与列号必须是整数；请改成 1、2 这样的整数");
                }

                bool absoluteRow    = true;
                bool absoluteColumn = true;
                if (arguments.size() > 2)
                {
                    long                  referenceType = 0;
                    const Units::Quantity typeQuantity  = toQuantity(arguments[2]->evaluate(), "address() 的第三个参数");
                    if (!typeQuantity.isDimensionless() || !essentiallyInteger(typeQuantity.getValue(), referenceType))
                    {
                        throw Base::TypeError("address() 的引用类型必须是整数；请改成 1、2、3 或 4");
                    }
                    if (referenceType < 1 || referenceType > 4)
                    {
                        throw Base::ValueError(std::format("address() 的引用类型 {} 超出范围；"
                                                           "合法取值是 1（绝对）、2（行绝对列相对）、"
                                                           "3（行相对列绝对）、4（相对）",
                                                           referenceType));
                    }
                    absoluteRow    = referenceType == 1 || referenceType == 2;
                    absoluteColumn = referenceType == 1 || referenceType == 3;
                }

                const CellAddress cell(static_cast<int>(row) - 1, static_cast<int>(column) - 1, absoluteRow, absoluteColumn);
                if (!cell.isValid())
                {
                    throw Base::ValueError(std::format("address() 给出的单元格（第 {} 行、第 {} 列）超出范围；"
                                                       "行号 1 到 {}，列号 1 到 {}",
                                                       row, column, CellAddress::s_maxRows, CellAddress::s_maxColumns));
                }
                return cell.toString();
            }
            case Function::HiddenReference:
            case Function::HiddenReferenceAlias:
                return arguments[0]->evaluate();
            case Function::VectorAngle:
            case Function::VectorCross:
            case Function::VectorDot:
            case Function::VectorLineDistance:
            case Function::VectorLineSegmentDistance:
            case Function::VectorLineProjection:
            case Function::VectorNormalize:
            case Function::VectorPlaneDistance:
            case Function::VectorPlaneProjection:
            case Function::VectorScale:
            case Function::VectorScaleX:
            case Function::VectorScaleY:
            case Function::VectorScaleZ:
            {
                Base::Vector3d firstVector = vectorArgument(arguments, 0, label);
                switch (function)
                {
                    case Function::VectorNormalize:
                    {
                        const double length = firstVector.Length();
                        if (length == 0.0)
                        {
                            // 零向量没有方向，归一化没有定义，报错好过产出 NaN
                            throw Base::ValueError("vnormalize() 收到零向量，无法归一化；请改用非零向量");
                        }
                        return firstVector / length;
                    }
                    case Function::VectorScale:
                        firstVector.Scale(lengthArgument(arguments, 1, label), lengthArgument(arguments, 2, label), lengthArgument(arguments, 3, label));
                        return firstVector;
                    case Function::VectorScaleX:
                        firstVector.ScaleX(lengthArgument(arguments, 1, label));
                        return firstVector;
                    case Function::VectorScaleY:
                        firstVector.ScaleY(lengthArgument(arguments, 1, label));
                        return firstVector;
                    case Function::VectorScaleZ:
                        firstVector.ScaleZ(lengthArgument(arguments, 1, label));
                        return firstVector;
                    default:
                        break;
                }

                const Base::Vector3d secondVector = vectorArgument(arguments, 1, label);
                switch (function)
                {
                    case Function::VectorAngle:
                        return Units::Quantity(toDegrees(firstVector.GetAngle(secondVector)), Units::Unit::Angle);
                    case Function::VectorCross:
                        return firstVector.Cross(secondVector);
                    case Function::VectorDot:
                        // 点积是无量纲的纯数，与 FreeCAD 返回 Python 浮点一致
                        return firstVector.Dot(secondVector);
                    default:
                        break;
                }

                const Base::Vector3d thirdVector = vectorArgument(arguments, 2, label);
                switch (function)
                {
                    case Function::VectorLineDistance:
                        return Units::Quantity(firstVector.DistanceToLine(secondVector, thirdVector), Units::Unit::Length);
                    case Function::VectorLineSegmentDistance:
                        return firstVector.DistanceToLineSegment(secondVector, thirdVector);
                    case Function::VectorLineProjection:
                        // 垂足 = 直线上一点 + 位移在直线方向上的分量。
                        // 不用 Vector3d::ProjectToLine：该函数的公式不读取自身，结果与待投影的点无关。
                        if (thirdVector.Sqr() == 0.0)
                        {
                            throw Base::ValueError("vlineproj() 的直线方向是零向量，无法确定直线；请给出非零方向");
                        }
                        return secondVector + (((firstVector - secondVector) * thirdVector) / thirdVector.Sqr()) * thirdVector;
                    case Function::VectorPlaneDistance:
                        return Units::Quantity(firstVector.DistanceToPlane(secondVector, thirdVector), Units::Unit::Length);
                    case Function::VectorPlaneProjection:
                        firstVector.ProjectToPlane(secondVector, thirdVector);
                        return firstVector;
                    default:
                        break;
                }
                throw EvaluationError(std::format("{}() 没有对应的向量求值实现；请报告该表达式", label));
            }
            default:
                break;
        }

        // 其余函数都按数量求值
        const Units::Quantity          firstQuantity = toQuantity(arguments[0]->evaluate(), std::format("{}() 的第一个参数", label));
        std::optional<Units::Quantity> secondQuantity;
        std::optional<Units::Quantity> thirdQuantity;
        if (arguments.size() > 1)
        {
            secondQuantity = toQuantity(arguments[1]->evaluate(), std::format("{}() 的第二个参数", label));
        }
        if (arguments.size() > 2)
        {
            thirdQuantity = toQuantity(arguments[2]->evaluate(), std::format("{}() 的第三个参数", label));
        }
        if (arguments.size() > 3)
        {
            throwArgumentCount(label, "最多 3 个数量参数", arguments.size());
        }

        double      output = 0.0;
        Units::Unit unit;
        double      scaler = 1.0;
        double      value  = firstQuantity.getValue();

        // 单位检查与换算；规则与 FreeCAD 一致
        switch (function)
        {
            case Function::Cosine:
            case Function::Sine:
            case Function::Tangent:
            case Function::RotationX:
            case Function::RotationY:
            case Function::RotationZ:
                if (!firstQuantity.isDimensionlessOrUnit(Units::Unit::Angle))
                {
                    throw Base::UnitsMismatchError(std::format("{}() 的参数需要角度量或纯数，实际是 {}；"
                                                               "请改用 deg、rad 这类角度单位",
                                                               label, firstQuantity.getUserString()));
                }
                // 角度量在库内以度存储，三角函数按弧度计算
                value = toRadians(value);
                unit  = Units::Unit();
                break;
            case Function::ArcCosine:
            case Function::ArcSine:
            case Function::ArcTangent:
                if (!firstQuantity.isDimensionless())
                {
                    throw Base::UnitsMismatchError(std::format("{}() 的参数需要纯数（反三角函数取比值），实际是 {}；"
                                                               "请先除以同量纲的参照量",
                                                               label, firstQuantity.getUserString()));
                }
                // 反三角函数输出弧度，而库内的角度量以度存储，因此乘上 180/pi
                unit   = Units::Unit::Angle;
                scaler = 180.0 / std::numbers::pi;
                break;
            case Function::Exponential:
            case Function::Logarithm:
            case Function::LogarithmBase10:
            case Function::HyperbolicSine:
            case Function::HyperbolicTangent:
            case Function::HyperbolicCosine:
                if (!firstQuantity.isDimensionless())
                {
                    throw Base::UnitsMismatchError(std::format("{}() 的参数需要纯数，实际是 {}；请先除以同量纲的参照量", label, firstQuantity.getUserString()));
                }
                unit = Units::Unit();
                break;
            case Function::Round:
            case Function::Truncate:
            case Function::Ceiling:
            case Function::Floor:
            case Function::Absolute:
                // 取整与绝对值保持量纲不变
                unit = firstQuantity.getUnit();
                break;
            case Function::SquareRoot:
                unit = firstQuantity.getUnit().sqrt();
                break;
            case Function::CubeRoot:
                unit = firstQuantity.getUnit().cbrt();
                break;
            case Function::ArcTangent2:
                if (firstQuantity.getUnit() != secondQuantity->getUnit())
                {
                    throw Base::UnitsMismatchError(std::format("atan2() 的两个参数单位必须一致：左侧是 {}，右侧是 {}；"
                                                               "请先换算成同一单位",
                                                               firstQuantity.getUnit().getString(), secondQuantity->getUnit().getString()));
                }
                unit   = Units::Unit::Angle;
                scaler = 180.0 / std::numbers::pi;
                break;
            case Function::Modulo:
                if (firstQuantity.getUnit() != secondQuantity->getUnit() && !firstQuantity.isDimensionless() && !secondQuantity->isDimensionless())
                {
                    throw Base::UnitsMismatchError(std::format("mod() 要求两侧单位一致或同为纯数：左侧是 {}，右侧是 {}；"
                                                               "请先换算成同一单位",
                                                               firstQuantity.getUnit().getString(), secondQuantity->getUnit().getString()));
                }
                unit = firstQuantity.getUnit();
                break;
            case Function::Power:
            {
                if (!secondQuantity->isDimensionless())
                {
                    throw Base::UnitsMismatchError(std::format("pow() 的指数不能带量纲，实际是 {}；请改用纯数指数", secondQuantity->getUserString()));
                }
                const double exponent = secondQuantity->getValue();
                if (!firstQuantity.isDimensionless())
                {
                    // 带量纲的底数只允许整数指数，否则量纲会出现分数次幂
                    if (std::fabs(exponent - std::round(exponent)) < 1e-9)
                    {
                        unit = firstQuantity.getUnit().pow(exponent);
                    } else
                    {
                        throw Base::UnitsMismatchError(std::format("pow() 的底数带量纲时指数必须是整数，实际是 {}；"
                                                                   "请把指数改成整数",
                                                                   exponent));
                    }
                }
                break;
            }
            case Function::Hypotenuse:
            case Function::Cathetus:
                if (firstQuantity.getUnit() != secondQuantity->getUnit())
                {
                    throw Base::UnitsMismatchError(std::format("{}() 的各参数单位必须一致：第一个是 {}，第二个是 {}；"
                                                               "请先换算成同一单位",
                                                               label, firstQuantity.getUnit().getString(), secondQuantity->getUnit().getString()));
                }
                if (arguments.size() > 2 && secondQuantity->getUnit() != thirdQuantity->getUnit())
                {
                    throw Base::UnitsMismatchError(std::format("{}() 的各参数单位必须一致：第二个是 {}，第三个是 {}；"
                                                               "请先换算成同一单位",
                                                               label, secondQuantity->getUnit().getString(), thirdQuantity->getUnit().getString()));
                }
                unit = firstQuantity.getUnit();
                break;
            case Function::TranslationMatrix:
                if (firstQuantity.isDimensionlessOrUnit(Units::Unit::Length) && secondQuantity->isDimensionlessOrUnit(Units::Unit::Length) &&
                    thirdQuantity->isDimensionlessOrUnit(Units::Unit::Length))
                {
                    break;
                }
                throw Base::UnitsMismatchError("translationm() 的三个平移分量必须是长度量或纯数；"
                                               "请改用 mm、in 这类长度单位");
            case Function::LogicalNot:
                // 与 FreeCAD 一致：只看数值不看量纲
                unit = Units::Unit();
                break;
            default:
                throw EvaluationError(std::format("{}() 不支持求值；请改用函数表里已实现的函数", label));
        }

        // 计算取值
        switch (function)
        {
            case Function::ArcCosine:
                output = std::acos(value);
                break;
            case Function::ArcSine:
                output = std::asin(value);
                break;
            case Function::ArcTangent:
                output = std::atan(value);
                break;
            case Function::Absolute:
                output = std::fabs(value);
                break;
            case Function::Exponential:
                output = std::exp(value);
                break;
            case Function::Logarithm:
                output = std::log(value);
                break;
            case Function::LogarithmBase10:
                output = std::log(value) / std::log(10.0);
                break;
            case Function::Sine:
                output = std::sin(value);
                break;
            case Function::HyperbolicSine:
                output = std::sinh(value);
                break;
            case Function::Tangent:
                output = std::tan(value);
                break;
            case Function::HyperbolicTangent:
                output = std::tanh(value);
                break;
            case Function::SquareRoot:
                output = std::sqrt(value);
                break;
            case Function::CubeRoot:
                output = std::cbrt(value);
                break;
            case Function::Cosine:
                output = std::cos(value);
                break;
            case Function::HyperbolicCosine:
                output = std::cosh(value);
                break;
            case Function::Modulo:
            {
                if (secondQuantity->getValue() == 0.0)
                {
                    throw Base::ValueError("mod() 的模数为零，无法求值；请检查表达式里的模数");
                }
                output = std::fmod(value, secondQuantity->getValue());
                break;
            }
            case Function::ArcTangent2:
                output = std::atan2(value, secondQuantity->getValue());
                break;
            case Function::Power:
                output = std::pow(value, secondQuantity->getValue());
                break;
            case Function::Hypotenuse:
                output = std::sqrt(std::pow(firstQuantity.getValue(), 2) + std::pow(secondQuantity->getValue(), 2) +
                                   (thirdQuantity.has_value() ? std::pow(thirdQuantity->getValue(), 2) : 0.0));
                break;
            case Function::Cathetus:
                output = std::sqrt(std::pow(firstQuantity.getValue(), 2) - std::pow(secondQuantity->getValue(), 2) -
                                   (thirdQuantity.has_value() ? std::pow(thirdQuantity->getValue(), 2) : 0.0));
                break;
            case Function::Round:
                output = std::round(value);
                break;
            case Function::Truncate:
                output = std::trunc(value);
                break;
            case Function::Ceiling:
                output = std::ceil(value);
                break;
            case Function::Floor:
                output = std::floor(value);
                break;
            case Function::RotationX:
            case Function::RotationY:
            case Function::RotationZ:
                return Base::Rotation(
                        Base::Vector3d(function == Function::RotationX ? 1.0 : 0.0, function == Function::RotationY ? 1.0 : 0.0, function == Function::RotationZ ? 1.0 : 0.0),
                        value);
            case Function::TranslationMatrix:
            {
                Base::Matrix4D matrix;
                matrix.move(firstQuantity.getValue(), secondQuantity->getValue(), thirdQuantity->getValue());
                return matrix;
            }
            case Function::LogicalNot:
                output = asBoolean(value) ? 0.0 : 1.0;
                break;
            default:
                throw EvaluationError(std::format("{}() 不支持求值；请改用函数表里已实现的函数", label));
        }

        return Units::Quantity(scaler * output, unit);
    }

    std::string_view FunctionExpression::functionName(Function function)
    {
        using Function = FunctionExpression::Function;
        switch (function)
        {
            case Function::Absolute:
                return "abs";
            case Function::ArcCosine:
                return "acos";
            case Function::ArcSine:
                return "asin";
            case Function::ArcTangent:
                return "atan";
            case Function::ArcTangent2:
                return "atan2";
            case Function::Cathetus:
                return "cath";
            case Function::CubeRoot:
                return "cbrt";
            case Function::Ceiling:
                return "ceil";
            case Function::Cosine:
                return "cos";
            case Function::HyperbolicCosine:
                return "cosh";
            case Function::Exponential:
                return "exp";
            case Function::Floor:
                return "floor";
            case Function::Hypotenuse:
                return "hypot";
            case Function::Logarithm:
                return "log";
            case Function::LogarithmBase10:
                return "log10";
            case Function::Modulo:
                return "mod";
            case Function::Power:
                return "pow";
            case Function::Round:
                return "round";
            case Function::Sine:
                return "sin";
            case Function::HyperbolicSine:
                return "sinh";
            case Function::SquareRoot:
                return "sqrt";
            case Function::Tangent:
                return "tan";
            case Function::HyperbolicTangent:
                return "tanh";
            case Function::Truncate:
                return "trunc";
            case Function::VectorAngle:
                return "vangle";
            case Function::VectorCross:
                return "vcross";
            case Function::VectorDot:
                return "vdot";
            case Function::VectorLineDistance:
                return "vlinedist";
            case Function::VectorLineSegmentDistance:
                return "vlinesegdist";
            case Function::VectorLineProjection:
                return "vlineproj";
            case Function::VectorNormalize:
                return "vnormalize";
            case Function::VectorPlaneDistance:
                return "vplanedist";
            case Function::VectorPlaneProjection:
                return "vplaneproj";
            case Function::VectorScale:
                return "vscale";
            case Function::VectorScaleX:
                return "vscalex";
            case Function::VectorScaleY:
                return "vscaley";
            case Function::VectorScaleZ:
                return "vscalez";
            case Function::MatrixInvert:
                return "minvert";
            case Function::MatrixRotate:
                return "mrotate";
            case Function::MatrixRotateX:
                return "mrotatex";
            case Function::MatrixRotateY:
                return "mrotatey";
            case Function::MatrixRotateZ:
                return "mrotatez";
            case Function::MatrixScale:
                return "mscale";
            case Function::MatrixTranslate:
                return "mtranslate";
            case Function::Create:
                return "create";
            case Function::List:
                return "list";
            case Function::Matrix:
                return "matrix";
            case Function::Placement:
                return "placement";
            case Function::Rotation:
                return "rotation";
            case Function::RotationX:
                return "rotationx";
            case Function::RotationY:
                return "rotationy";
            case Function::RotationZ:
                return "rotationz";
            case Function::Stringify:
                return "str";
            case Function::ParseQuantity:
                return "parsequant";
            case Function::TranslationMatrix:
                return "translationm";
            case Function::Tuple:
                return "tuple";
            case Function::Vector:
                return "vector";
            case Function::Address:
                return "address";
            case Function::HiddenReference:
                return "hiddenref";
            case Function::HiddenReferenceAlias:
                return "href";
            case Function::LogicalNot:
                return "not";
            case Function::Average:
                return "average";
            case Function::Count:
                return "count";
            case Function::Maximum:
                return "max";
            case Function::Minimum:
                return "min";
            case Function::StandardDeviation:
                return "stddev";
            case Function::Sum:
                return "sum";
            case Function::LogicalAnd:
                return "and";
            case Function::LogicalOr:
                return "or";
            case Function::None:
            case Function::Aggregates:
            case Function::Last:
                // 哨兵值没有函数名
                return "?";
        }
        return "?";
    }

    FunctionExpression::Function FunctionExpression::functionFromName(std::string_view name)
    {
        using Function = FunctionExpression::Function;
        struct Entry
        {
            std::string_view name;     ///< 函数名
            Function         function; ///< 函数种类
        };

        // 名字与 FreeCAD 的函数表一致，全部小写，供后续解析器直接查表
        static const auto entries = std::to_array<Entry>({
                {"abs", Function::Absolute},
                {"acos", Function::ArcCosine},
                {"asin", Function::ArcSine},
                {"atan", Function::ArcTangent},
                {"atan2", Function::ArcTangent2},
                {"cath", Function::Cathetus},
                {"cbrt", Function::CubeRoot},
                {"ceil", Function::Ceiling},
                {"cos", Function::Cosine},
                {"cosh", Function::HyperbolicCosine},
                {"exp", Function::Exponential},
                {"floor", Function::Floor},
                {"hypot", Function::Hypotenuse},
                {"log", Function::Logarithm},
                {"log10", Function::LogarithmBase10},
                {"mod", Function::Modulo},
                {"pow", Function::Power},
                {"round", Function::Round},
                {"sin", Function::Sine},
                {"sinh", Function::HyperbolicSine},
                {"sqrt", Function::SquareRoot},
                {"tan", Function::Tangent},
                {"tanh", Function::HyperbolicTangent},
                {"trunc", Function::Truncate},
                {"vangle", Function::VectorAngle},
                {"vcross", Function::VectorCross},
                {"vdot", Function::VectorDot},
                {"vlinedist", Function::VectorLineDistance},
                {"vlinesegdist", Function::VectorLineSegmentDistance},
                {"vlineproj", Function::VectorLineProjection},
                {"vnormalize", Function::VectorNormalize},
                {"vplanedist", Function::VectorPlaneDistance},
                {"vplaneproj", Function::VectorPlaneProjection},
                {"vscale", Function::VectorScale},
                {"vscalex", Function::VectorScaleX},
                {"vscaley", Function::VectorScaleY},
                {"vscalez", Function::VectorScaleZ},
                {"minvert", Function::MatrixInvert},
                {"mrotate", Function::MatrixRotate},
                {"mrotatex", Function::MatrixRotateX},
                {"mrotatey", Function::MatrixRotateY},
                {"mrotatez", Function::MatrixRotateZ},
                {"mscale", Function::MatrixScale},
                {"mtranslate", Function::MatrixTranslate},
                {"create", Function::Create},
                {"list", Function::List},
                {"matrix", Function::Matrix},
                {"placement", Function::Placement},
                {"rotation", Function::Rotation},
                {"rotationx", Function::RotationX},
                {"rotationy", Function::RotationY},
                {"rotationz", Function::RotationZ},
                {"str", Function::Stringify},
                {"parsequant", Function::ParseQuantity},
                {"translationm", Function::TranslationMatrix},
                {"tuple", Function::Tuple},
                {"vector", Function::Vector},
                {"address", Function::Address},
                {"hiddenref", Function::HiddenReference},
                {"href", Function::HiddenReferenceAlias},
                {"not", Function::LogicalNot},
                {"average", Function::Average},
                {"count", Function::Count},
                {"max", Function::Maximum},
                {"min", Function::Minimum},
                {"stddev", Function::StandardDeviation},
                {"sum", Function::Sum},
                {"and", Function::LogicalAnd},
                {"or", Function::LogicalOr},
        });

        for (const auto &entry: entries)
        {
            if (entry.name == name)
            {
                return entry.function;
            }
        }
        return Function::None;
    }

    void FunctionExpression::appendText(std::string &text, bool persistent, int) const
    {
        if (m_name.empty())
        {
            text += functionName(m_function);
        } else
        {
            text += m_name;
        }
        text += '(';
        for (std::size_t index = 0; index < m_arguments.size(); ++index)
        {
            if (index != 0)
            {
                // 实参分隔符与 FreeCAD 的表达一致，用分号避免与小数点逗号混淆
                text += "; ";
            }
            text += m_arguments[index]->toString(persistent);
        }
        text += ')';
    }

    ExpressionPtr FunctionExpression::copyNode() const
    {
        std::vector<ExpressionPtr> arguments;
        arguments.reserve(m_arguments.size());
        for (const auto &argument: m_arguments)
        {
            arguments.push_back(argument->copy());
        }
        return std::make_unique<FunctionExpression>(resolver(), m_function, m_name, std::move(arguments));
    }

    //
    // 变量引用节点
    //

    VariableExpression::VariableExpression(IObjectResolver *resolver, Reference reference) : UnitExpression(resolver), m_reference(std::move(reference))
    {
    }

    VariableExpression::~VariableExpression() = default;

    const VariableExpression::Reference &VariableExpression::getReference() const noexcept
    {
        return m_reference;
    }

    void VariableExpression::setReference(Reference reference)
    {
        m_reference = std::move(reference);
    }

    std::string VariableExpression::name() const
    {
        return m_reference.propertyName;
    }

    std::string VariableExpression::pathText() const
    {
        if (m_reference.objectName.empty())
        {
            // 未限定对象：按局部作用域的写法显示
            return m_reference.propertyName;
        }
        if (m_reference.documentName.empty())
        {
            return m_reference.objectName + "." + m_reference.propertyName;
        }
        return "<<" + m_reference.documentName + ">>." + m_reference.objectName + "." + m_reference.propertyName;
    }

    IProperty *VariableExpression::resolveProperty() const
    {
        IObjectResolver  *objectResolver = resolver();
        const std::string path           = pathText();
        if (objectResolver == nullptr)
        {
            throw Base::NameError(std::format("表达式引用了 '{}'，但表达式没有绑定对象解析器"
                                              "（IObjectResolver）；请在构造表达式时注入宿主实现的解析器，"
                                              "或改用常量表达式",
                                              path));
        }

        IObject *object = objectResolver->resolve(m_reference.documentName, m_reference.objectName);
        if (object == nullptr)
        {
            throw Base::NameError(std::format("解析器找不到 '{}' 引用的对象（文档 '{}'、对象 '{}'）；"
                                              "请检查对象名，或先用 IObjectResolver::objectNames() 列出可用对象",
                                              path, m_reference.documentName, m_reference.objectName));
        }

        IProperty *property = object->findProperty(m_reference.propertyName);
        if (property == nullptr)
        {
            throw Base::AttributeError(std::format("对象 '{}' 上没有属性 '{}'（表达式引用 '{}'）；"
                                                   "该对象的属性有：{}",
                                                   object->name(), m_reference.propertyName, path, joinNames(object->propertyNames())));
        }
        return property;
    }

    void VariableExpression::assignValue(const Value &newValue)
    {
        if (hasComponent())
        {
            // 分量链的写回要定位到子值，当前值模型无法表达「写哪一段」；明确拒绝而不是写整值
            throw Base::AttributeError(std::format("引用 '{}' 带有分量或下标，不能整体写回；请去掉分量后只写基属性，"
                                                   "或由宿主按属性直接修改明细",
                                                   pathText()));
        }
        IProperty *property = resolveProperty();
        if (property->isReadOnly())
        {
            throw Base::AttributeError(std::format("属性 '{}' 是只读的，表达式不能写回；"
                                                   "请改写到可写属性，或在宿主侧放开该属性的读写",
                                                   pathText()));
        }
        if (!property->setValue(newValue))
        {
            throw Base::ValueError(std::format("属性 '{}' 拒绝了取值 {}；"
                                               "请检查取值的类型与量纲是否与属性类型 '{}' 匹配",
                                               pathText(), valueText(newValue), property->typeName()));
        }
    }

    ExpressionPtr VariableExpression::simplify() const
    {
        return copy();
    }

    std::string_view VariableExpression::nodeName() const
    {
        return "Variable";
    }

    Value VariableExpression::evaluateNode() const
    {
        IProperty                 *property = resolveProperty();
        const std::optional<Value> value    = property->value();
        if (!value.has_value())
        {
            throw Base::AttributeError(std::format("属性 '{}' 尚未赋值，表达式无法取值；请先给该属性赋值", pathText()));
        }
        if (!hasComponent())
        {
            return *value;
        }

        // 分量在解析到基属性之后逐段作用在属性值上：Box.Placement.Base[0] 先取向量再取 x
        Value             result  = *value;
        const std::string context = std::format("引用 '{}' 的分量访问", pathText());
        for (const auto &component: components())
        {
            result = applyComponent(result, component, context);
        }
        return result;
    }

    void VariableExpression::appendText(std::string &text, bool, int) const
    {
        text += pathText();
    }

    ExpressionPtr VariableExpression::copyNode() const
    {
        return std::make_unique<VariableExpression>(resolver(), m_reference);
    }

    bool VariableExpression::isIndexable() const
    {
        // 引用后面可以直接跟分量与下标，如 Box.Length[0]
        return true;
    }

    //
    // 文本节点
    //

    StringExpression::StringExpression(IObjectResolver *resolver, std::string text) : Expression(resolver), m_text(std::move(text))
    {
    }

    std::string StringExpression::getText() const
    {
        return m_text;
    }

    ExpressionPtr StringExpression::simplify() const
    {
        return copy();
    }

    std::string_view StringExpression::nodeName() const
    {
        return "String";
    }

    Value StringExpression::evaluateNode() const
    {
        return Value(m_text);
    }

    void StringExpression::appendText(std::string &text, bool, int) const
    {
        text += quoteText(m_text);
    }

    ExpressionPtr StringExpression::copyNode() const
    {
        return std::make_unique<StringExpression>(resolver(), m_text);
    }

    bool StringExpression::isIndexable() const
    {
        return true;
    }

    //
    // 取值节点
    //

    ValueExpression::ValueExpression(IObjectResolver *resolver, Value value) : Expression(resolver), m_value(std::move(value))
    {
    }

    const Value &ValueExpression::getValue() const noexcept
    {
        return m_value;
    }

    ExpressionPtr ValueExpression::simplify() const
    {
        return copy();
    }

    std::string_view ValueExpression::nodeName() const
    {
        return "Value";
    }

    Value ValueExpression::evaluateNode() const
    {
        return m_value;
    }

    void ValueExpression::appendText(std::string &text, bool, int) const
    {
        text += valueText(m_value);
    }

    ExpressionPtr ValueExpression::copyNode() const
    {
        return std::make_unique<ValueExpression>(resolver(), m_value);
    }

    //
    // 单元格区间节点
    //

    RangeExpression::RangeExpression(IObjectResolver *resolver, std::string begin, std::string end) : Expression(resolver), m_begin(std::move(begin)), m_end(std::move(end))
    {
    }

    RangeExpression::~RangeExpression() = default;

    std::string RangeExpression::getBegin() const
    {
        return m_begin;
    }

    std::string RangeExpression::getEnd() const
    {
        return m_end;
    }

    Range RangeExpression::getRange() const
    {
        const CellAddress begin = stringToAddress(m_begin, true);
        const CellAddress end   = stringToAddress(m_end, true);
        if (!begin.isValid() || !end.isValid())
        {
            throw EvaluationError(std::format("区间 '{}:{}' 的首尾必须是合法单元格地址（如 A1、$B$2）；"
                                              "请改成单元格地址，别名请由宿主的属性提供",
                                              m_begin, m_end));
        }
        return Range(begin, end);
    }

    ExpressionPtr RangeExpression::simplify() const
    {
        return copy();
    }

    std::string_view RangeExpression::nodeName() const
    {
        return "Range";
    }

    const RangeExpression *RangeExpression::asRangeExpression() const noexcept
    {
        return this;
    }

    Value RangeExpression::evaluateNode() const
    {
        // 区间本身没有标量取值，静默取一个单元格会让聚合结果出错
        throw EvaluationError(std::format("区间 '{}:{}' 不能单独作为取值；"
                                          "请把它作为 sum()、average()、min()、max() 等聚合函数的实参，"
                                          "或改用具体单元格引用",
                                          m_begin, m_end));
    }

    void RangeExpression::appendText(std::string &text, bool, int) const
    {
        text += m_begin;
        text += ':';
        text += m_end;
    }

    ExpressionPtr RangeExpression::copyNode() const
    {
        return std::make_unique<RangeExpression>(resolver(), m_begin, m_end);
    }

    //
    // 依赖收集：各节点把子树里的变量引用追加到同一列表，去重由 collectReferences() 统一完成
    //

    void OperatorExpression::_collectReferences(std::vector<VariableReference> &out) const
    {
        collectReferencesFrom(m_left.get(), out);
        collectReferencesFrom(m_right.get(), out);
    }

    void ConditionalExpression::_collectReferences(std::vector<VariableReference> &out) const
    {
        collectReferencesFrom(m_condition.get(), out);
        collectReferencesFrom(m_trueExpression.get(), out);
        collectReferencesFrom(m_falseExpression.get(), out);
    }

    void FunctionExpression::_collectReferences(std::vector<VariableReference> &out) const
    {
        for (const auto &argument: m_arguments)
        {
            collectReferencesFrom(argument.get(), out);
        }
    }

    void VariableExpression::_collectReferences(std::vector<VariableReference> &out) const
    {
        out.push_back(m_reference);
    }

    bool VariableExpression::supportsComponentAccess() const noexcept
    {
        // 与基类差异：引用节点能把分量作用在解析到的属性值上，但前提是有解析器，
        // 否则连基属性都拿不到；此时仍按基类统一报错，而不是误报「属性不存在」
        return resolver() != nullptr;
    }

} // namespace ExpressionEngine::Expression
