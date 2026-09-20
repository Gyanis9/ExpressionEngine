// 本文件覆盖表达式 AST 的行为：直接构造节点后求值、化简与文本化，并覆盖拒绝面。
// 用例不依赖解析器：节点全部手工构造。

#include <gtest/gtest.h>

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
#include <ExpressionEngine/Base/Rotation.h>
#include <ExpressionEngine/Base/Vector3D.h>
#include <ExpressionEngine/Expression/Expression.h>
#include <ExpressionEngine/Expression/PropertyModel.h>
#include <ExpressionEngine/Expression/Range.h>
#include <ExpressionEngine/Expression/Value.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Expression
{
    namespace
    {

        using Function = FunctionExpression::Function;
        using Operator = OperatorExpression::Operator;

        /// 造一个纯数节点
        ExpressionPtr number(double value)
        {
            return std::make_unique<NumberExpression>(nullptr, Units::Quantity(value));
        }

        /// 造一个带单位的数值节点
        ExpressionPtr quantity(double value, const Units::Unit &unit)
        {
            return std::make_unique<NumberExpression>(nullptr, Units::Quantity(value, unit));
        }

        /// 造一个向量取值节点
        ExpressionPtr vectorNode(double x, double y, double z)
        {
            return std::make_unique<ValueExpression>(nullptr, Value(Base::Vector3d(x, y, z)));
        }

        /// 造一个取值节点
        ExpressionPtr valueNode(const Value &value)
        {
            return std::make_unique<ValueExpression>(nullptr, value);
        }

        /// 造一个二元运算节点
        ExpressionPtr binary(Operator operation, ExpressionPtr left, ExpressionPtr right)
        {
            return std::make_unique<OperatorExpression>(nullptr, std::move(left), operation, std::move(right));
        }

        /// 造一个函数调用节点；实参逐个 move 进参数表（unique_ptr 无法从初始化列表拷贝）
        template<class... Arguments>
        ExpressionPtr function(const Function functionKind, Arguments &&... arguments)
        {
            std::vector<ExpressionPtr> parameterList;
            parameterList.reserve(sizeof...(Arguments));
            (parameterList.push_back(std::forward<Arguments>(arguments)), ...);

            const std::string name(FunctionExpression::functionName(functionKind));
            return std::make_unique<FunctionExpression>(nullptr, functionKind, name, std::move(parameterList));
        }

        /// 取出数量；不是数量时让用例失败
        Units::Quantity quantityOf(const Value &value)
        {
            const auto *quantityValue = std::get_if<Units::Quantity>(&value);
            if (quantityValue == nullptr)
            {
                ADD_FAILURE() << "期望数量，实际是 " << std::string(valueTypeName(value));
                return {};
            }
            return *quantityValue;
        }

        /// 取出纯数；不是纯数时让用例失败
        double doubleOf(const Value &value)
        {
            const auto *numberValue = std::get_if<double>(&value);
            if (numberValue == nullptr)
            {
                ADD_FAILURE() << "期望纯数，实际是 " << std::string(valueTypeName(value));
                return 0.0;
            }
            return *numberValue;
        }

        /// 取出布尔；不是布尔时让用例失败
        bool boolOf(const Value &value)
        {
            const auto *booleanValue = std::get_if<bool>(&value);
            if (booleanValue == nullptr)
            {
                ADD_FAILURE() << "期望布尔，实际是 " << std::string(valueTypeName(value));
                return false;
            }
            return *booleanValue;
        }

        /// 取出文本；不是文本时让用例失败
        std::string textOf(const Value &value)
        {
            const auto *textValue = std::get_if<std::string>(&value);
            if (textValue == nullptr)
            {
                ADD_FAILURE() << "期望文本，实际是 " << std::string(valueTypeName(value));
                return {};
            }
            return *textValue;
        }

        /// 取出向量；不是向量时让用例失败
        Base::Vector3d vectorOf(const Value &value)
        {
            const auto *vectorValue = std::get_if<Base::Vector3d>(&value);
            if (vectorValue == nullptr)
            {
                ADD_FAILURE() << "期望向量，实际是 " << std::string(valueTypeName(value));
                return Base::Vector3d();
            }
            return *vectorValue;
        }

        /// 取出旋转；不是旋转时让用例失败
        Base::Rotation rotationOf(const Value &value)
        {
            const auto *rotationValue = std::get_if<Base::Rotation>(&value);
            if (rotationValue == nullptr)
            {
                ADD_FAILURE() << "期望旋转，实际是 " << std::string(valueTypeName(value));
                return {};
            }
            return *rotationValue;
        }

        /// 取出位姿；不是位姿时让用例失败
        Base::Placement placementOf(const Value &value)
        {
            const auto *placementValue = std::get_if<Base::Placement>(&value);
            if (placementValue == nullptr)
            {
                ADD_FAILURE() << "期望位姿，实际是 " << std::string(valueTypeName(value));
                return {};
            }
            return *placementValue;
        }

        /// 取出矩阵；不是矩阵时让用例失败
        Base::Matrix4D matrixOf(const Value &value)
        {
            const auto *matrixValue = std::get_if<Base::Matrix4D>(&value);
            if (matrixValue == nullptr)
            {
                ADD_FAILURE() << "期望矩阵，实际是 " << std::string(valueTypeName(value));
                return {};
            }
            return *matrixValue;
        }

        /// 测试用属性：值可读写，可设为只读
        class FakeProperty : public IProperty
        {
        public:
            /**
             * @brief 构造属性
             * @param propertyName 属性名
             * @param propertyTypeName 属性类型名
             * @param initialValue 初值，空表示尚未赋值
             */
            FakeProperty(std::string propertyName, std::string propertyTypeName, std::optional<Value> initialValue) :
                m_name(std::move(propertyName)), m_typeName(std::move(propertyTypeName)), m_value(std::move(initialValue))
            {
            }

            /// 覆写：取属性名
            [[nodiscard]] std::string_view name() const override
            {
                return m_name;
            }

            /// 覆写：取属性类型名
            [[nodiscard]] std::string_view typeName() const override
            {
                return m_typeName;
            }

            /// 覆写：读属性值
            [[nodiscard]] std::optional<Value> value() const override
            {
                return m_value;
            }

            /// 覆写：写属性值；只读时拒绝
            [[nodiscard]] bool setValue(const Value &newValue) override
            {
                if (m_readOnly)
                {
                    return false;
                }
                m_value = newValue;
                return true;
            }

            /// 覆写：是否只读
            [[nodiscard]] bool isReadOnly() const override
            {
                return m_readOnly;
            }

            /// 设置只读开关，供拒绝面用例使用
            void setReadOnly(bool readOnly)
            {
                m_readOnly = readOnly;
            }

        private:
            std::string          m_name;            ///< 属性名
            std::string          m_typeName;        ///< 属性类型名
            std::optional<Value> m_value;           ///< 当前取值
            bool                 m_readOnly{false}; ///< 是否只读
        };

        /// 测试用宿主对象
        class FakeObject : public IObject
        {
        public:
            /**
             * @brief 构造对象
             * @param objectName 对象名
             * @param documentName 所属文档名
             */
            explicit FakeObject(std::string objectName, std::string documentName = std::string()) :
                m_name(std::move(objectName)), m_documentName(std::move(documentName))
            {
            }

            /// 覆写：取对象名
            [[nodiscard]] std::string_view name() const override
            {
                return m_name;
            }

            /// 覆写：取标签
            [[nodiscard]] std::string_view label() const override
            {
                return m_name;
            }

            /// 覆写：按名取属性
            [[nodiscard]] IProperty *findProperty(std::string_view propertyName) override
            {
                for (auto &property: m_properties)
                {
                    if (property.name() == propertyName)
                    {
                        return &property;
                    }
                }
                return nullptr;
            }

            /// 覆写：取全部属性名
            [[nodiscard]] std::vector<std::string> propertyNames() const override
            {
                std::vector<std::string> names;
                names.reserve(m_properties.size());
                for (const auto &property: m_properties)
                {
                    names.emplace_back(property.name());
                }
                return names;
            }

            /// 覆写：取所属文档名
            [[nodiscard]] std::string_view documentName() const override
            {
                return m_documentName;
            }

            /// 追加一个属性
            void addProperty(std::string propertyName, std::string propertyTypeName, std::optional<Value> initialValue = std::nullopt)
            {
                m_properties.emplace_back(std::move(propertyName), std::move(propertyTypeName), std::move(initialValue));
            }

        private:
            std::string               m_name;         ///< 对象名
            std::string               m_documentName; ///< 所属文档名
            std::vector<FakeProperty> m_properties;   ///< 属性表
        };

        /// 测试用解析器：空文档名不限文档，空对象名表示当前对象
        class FakeResolver : public IObjectResolver
        {
        public:
            /// 覆写：按文档名与对象名解析对象
            [[nodiscard]] IObject *resolve(std::string_view documentName, std::string_view objectName) override
            {
                if (objectName.empty())
                {
                    // 与 PropertyModel.h 的约定一致：空对象名表示表达式所属的当前对象
                    return m_currentObject;
                }
                for (auto &object: m_objects)
                {
                    if (object.name() != objectName)
                    {
                        continue;
                    }
                    if (!documentName.empty() && object.documentName() != documentName)
                    {
                        continue;
                    }
                    return &object;
                }
                return nullptr;
            }

            /// 覆写：列出对象名
            [[nodiscard]] std::vector<std::string> objectNames(std::string_view) const override
            {
                std::vector<std::string> names;
                names.reserve(m_objects.size());
                for (const auto &object: m_objects)
                {
                    names.emplace_back(object.name());
                }
                return names;
            }

            /// 追加一个对象
            FakeObject &addObject(std::string objectName, std::string documentName = std::string())
            {
                m_objects.emplace_back(std::move(objectName), std::move(documentName));
                return m_objects.back();
            }

            /// 设置局部作用域里的当前对象
            void setCurrentObject(FakeObject *object)
            {
                m_currentObject = object;
            }

        private:
            std::vector<FakeObject> m_objects;                ///< 对象表
            FakeObject *            m_currentObject{nullptr}; ///< 当前对象，供未限定名的引用使用
        };

        /**
         * @brief 钉住：节点文本与优先级——低优先级子节点补括号，文本能原样解析回同一棵树
         */
        TEST(ExpressionTest, NodeTextKeepsPrecedence)
        {
            EXPECT_EQ(number(2.5)->toString(), "2.5");
            EXPECT_EQ(number(2.5)->priority(), 20);
            EXPECT_EQ(number(2.5)->nodeName(), "Number");

            auto sum = binary(Operator::Add, number(2.0), number(3.0));
            EXPECT_EQ(sum->toString(), "2 + 3");
            EXPECT_EQ(sum->priority(), 3);
            // checkPriority 打开时，低优先级节点自身补括号
            EXPECT_EQ(sum->toString(false, true), "(2 + 3)");

            auto product = binary(Operator::Multiply, std::move(sum), number(4.0));
            EXPECT_EQ(product->toString(), "(2 + 3) * 4");
            EXPECT_EQ(product->priority(), 4);

            // 减法左结合：右操作数同为减法时必须补括号
            auto nestedSubtract = binary(Operator::Subtract, number(10.0), binary(Operator::Subtract, number(3.0), number(1.0)));
            EXPECT_EQ(nestedSubtract->toString(), "10 - (3 - 1)");
            // 同优先级但运算符不同，右侧同样补括号
            auto mixedPriority = binary(Operator::Subtract, number(10.0), binary(Operator::Add, number(3.0), number(1.0)));
            EXPECT_EQ(mixedPriority->toString(), "10 - (3 + 1)");

            // 一元运算符不写右操作数
            auto negate = std::make_unique<OperatorExpression>(nullptr, number(5.0), Operator::Negate, nullptr);
            EXPECT_EQ(negate->toString(), "-5");
            EXPECT_EQ(negate->priority(), 6);
        }

        /**
         * @brief 钉住：四则与幂的求值结果，以及除零这类退化输入必须报错
         */
        TEST(ExpressionTest, ArithmeticEvaluation)
        {
            EXPECT_DOUBLE_EQ(quantityOf(binary(Operator::Add, number(2.0), number(3.0))->evaluate()).getValue(), 5.0);
            EXPECT_DOUBLE_EQ(quantityOf(binary(Operator::Subtract, number(2.0), number(3.0))->evaluate()).getValue(), -1.0);
            EXPECT_DOUBLE_EQ(quantityOf(binary(Operator::Multiply, number(2.0), number(3.0))->evaluate()).getValue(), 6.0);
            EXPECT_DOUBLE_EQ(quantityOf(binary(Operator::Divide, number(7.0), number(2.0))->evaluate()).getValue(), 3.5);
            EXPECT_DOUBLE_EQ(quantityOf(binary(Operator::Modulo, number(7.0), number(2.0))->evaluate()).getValue(), 1.0);
            EXPECT_DOUBLE_EQ(quantityOf(binary(Operator::Power, number(2.0), number(10.0))->evaluate()).getValue(), 1024.0);

            // 树形决定结合顺序：(2 + 3) * 4 = 20
            auto sum = binary(Operator::Add, number(2.0), number(3.0));
            EXPECT_DOUBLE_EQ(quantityOf(binary(Operator::Multiply, std::move(sum), number(4.0))->evaluate()).getValue(), 20.0);

            // 一元运算
            auto negate = std::make_unique<OperatorExpression>(nullptr, number(5.0), Operator::Negate, nullptr);
            EXPECT_DOUBLE_EQ(quantityOf(negate->evaluate()).getValue(), -5.0);

            // 除数为零：报错而不是给出 inf
            EXPECT_THROW(static_cast<void>(binary(Operator::Divide, number(1.0), number(0.0))->evaluate()), Base::ValueError);
            EXPECT_THROW(static_cast<void>(binary(Operator::Modulo, number(1.0), number(0.0))->evaluate()), Base::ValueError);

            // 类型不符：几何值不能参与加法
            EXPECT_THROW(static_cast<void>(binary(Operator::Add, number(1.0), vectorNode(1.0, 2.0, 3.0))->evaluate()), Base::TypeError);

            // 文本相加是拼接
            auto textSum = binary(Operator::Add, std::make_unique<StringExpression>(nullptr, "零件 "), std::make_unique<StringExpression>(nullptr, "A"));
            EXPECT_EQ(textOf(textSum->evaluate()), "零件 A");
            // 文本与数量相加报错
            EXPECT_THROW(static_cast<void>(binary(Operator::Add, std::make_unique<StringExpression>(nullptr, "零件"), number(1.0))->evaluate()), Base::TypeError);

            // 构造期拒绝缺操作数与一元运算带右操作数
            EXPECT_THROW(static_cast<void>(std::make_unique<OperatorExpression>(nullptr, nullptr, Operator::Add, number(1.0))), EvaluationError);
            EXPECT_THROW(static_cast<void>(std::make_unique<OperatorExpression>(nullptr, number(1.0), Operator::Add, nullptr)), EvaluationError);
            EXPECT_THROW(static_cast<void>(std::make_unique<OperatorExpression>(nullptr, number(1.0), Operator::Negate, number(2.0))), EvaluationError);
        }

        /**
         * @brief 钉住：单位传播——数值与单位相乘、乘除合并量纲、整数幂作用于量纲
         */
        TEST(ExpressionTest, UnitPropagation)
        {
            // 2 mm：数值节点与单位节点相乘
            auto twoMillimetres = std::make_unique<OperatorExpression>(nullptr, number(2.0), Operator::UnitScale,
                                                                       std::make_unique<UnitExpression>(nullptr, Units::Quantity(1.0, Units::Unit::Length), "mm"));
            EXPECT_EQ(twoMillimetres->toString(), "2 mm");
            const Units::Quantity length = quantityOf(twoMillimetres->evaluate());
            EXPECT_DOUBLE_EQ(length.getValue(), 2.0);
            EXPECT_EQ(length.getUnit(), Units::Unit::Length);

            // 2 mm * 3 mm = 6 mm^2
            const Units::Quantity area = quantityOf(binary(Operator::Multiply, quantity(2.0, Units::Unit::Length), quantity(3.0, Units::Unit::Length))->evaluate());
            EXPECT_EQ(area.getUnit(), Units::Unit::Area);
            EXPECT_DOUBLE_EQ(area.getValue(), 6.0);

            // 面积除以长度得到长度
            const Units::Quantity quotient = quantityOf(binary(Operator::Divide, quantity(6.0, Units::Unit::Area), quantity(2.0, Units::Unit::Length))->evaluate());
            EXPECT_EQ(quotient.getUnit(), Units::Unit::Length);
            EXPECT_DOUBLE_EQ(quotient.getValue(), 3.0);

            // 带量纲的底数按整数指数取幂，量纲随之取幂
            const Units::Quantity volume = quantityOf(binary(Operator::Power, quantity(2.0, Units::Unit::Length), number(3.0))->evaluate());
            EXPECT_EQ(volume.getUnit(), Units::Unit::Volume);
            EXPECT_DOUBLE_EQ(volume.getValue(), 8.0);

            // 分数指数只要让各量纲指数落在整数格点上就合法：面积开平方是长度
            const Units::Quantity side = quantityOf(binary(Operator::Power, quantity(4.0, Units::Unit::Area), number(0.5))->evaluate());
            EXPECT_EQ(side.getUnit(), Units::Unit::Length);
            EXPECT_DOUBLE_EQ(side.getValue(), 2.0);

            // 落在非整数格点上时报错：长度指数 1 × 0.5 = 0.5 无法表示
            EXPECT_THROW(static_cast<void>(binary(Operator::Power, quantity(4.0, Units::Unit::Length), number(0.5))->evaluate()), Base::UnitsMismatchError);
            // 指数带量纲报错
            EXPECT_THROW(static_cast<void>(binary(Operator::Power, quantity(2.0, Units::Unit::Length), quantity(2.0, Units::Unit::Length))->evaluate()), Base::UnitsMismatchError);

            // 加减要求同量纲
            EXPECT_THROW(static_cast<void>(binary(Operator::Add, quantity(1.0, Units::Unit::Length), quantity(1.0, Units::Unit::TimeSpan))->evaluate()), Base::UnitsMismatchError);
            // 长度量与纯数不能相加
            EXPECT_THROW(static_cast<void>(binary(Operator::Add, quantity(1.0, Units::Unit::Length), number(1.0))->evaluate()), Base::UnitsMismatchError);

            // 取余：同量纲可算，异量纲报错
            EXPECT_DOUBLE_EQ(quantityOf(binary(Operator::Modulo, quantity(7.0, Units::Unit::Length), quantity(2.0, Units::Unit::Length))->evaluate()).getValue(), 1.0);
            EXPECT_THROW(static_cast<void>(binary(Operator::Modulo, quantity(7.0, Units::Unit::Length), quantity(2.0, Units::Unit::TimeSpan))->evaluate()),
                         Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：比较运算——== 量纲不同给 false，大小比较量纲不同报错，几何值只有相等判定
         */
        TEST(ExpressionTest, ComparisonSemantics)
        {
            EXPECT_TRUE(boolOf(binary(Operator::Less, number(1.0), number(2.0))->evaluate()));
            EXPECT_FALSE(boolOf(binary(Operator::Greater, number(1.0), number(2.0))->evaluate()));
            EXPECT_TRUE(boolOf(binary(Operator::LessEqual, number(2.0), number(2.0))->evaluate()));
            EXPECT_TRUE(boolOf(binary(Operator::GreaterEqual, number(2.0), number(2.0))->evaluate()));

            EXPECT_TRUE(boolOf(binary(Operator::Equal, quantity(1.0, Units::Unit::Length), quantity(1.0, Units::Unit::Length))->evaluate()));
            // 量纲不同时相等判定给 false，供 == 复用
            EXPECT_FALSE(boolOf(binary(Operator::Equal, quantity(1.0, Units::Unit::Length), quantity(1.0, Units::Unit::TimeSpan))->evaluate()));
            EXPECT_TRUE(boolOf(binary(Operator::NotEqual, quantity(1.0, Units::Unit::Length), quantity(1.0, Units::Unit::TimeSpan))->evaluate()));
            // 但大小比较量纲不同必须报错
            EXPECT_THROW(static_cast<void>(binary(Operator::Less, quantity(1.0, Units::Unit::Length), quantity(1.0, Units::Unit::TimeSpan))->evaluate()), Base::UnitsMismatchError);

            // 几何值按容差相等，不参与大小比较
            EXPECT_TRUE(boolOf(binary(Operator::Equal, vectorNode(1.0, 0.0, 0.0), vectorNode(1.0, 0.0, 0.0))->evaluate()));
            EXPECT_FALSE(boolOf(binary(Operator::Equal, vectorNode(1.0, 0.0, 0.0), vectorNode(2.0, 0.0, 0.0))->evaluate()));
            EXPECT_THROW(static_cast<void>(binary(Operator::Less, vectorNode(1.0, 0.0, 0.0), vectorNode(2.0, 0.0, 0.0))->evaluate()), Base::TypeError);
        }

        /**
         * @brief 钉住：三元运算按条件取分支，条件不能判定真假时报错，常量条件可被化简折叠
         */
        TEST(ExpressionTest, ConditionalSemantics)
        {
            auto conditional = std::make_unique<ConditionalExpression>(nullptr, binary(Operator::Less, number(1.0), number(2.0)),
                                                                       std::make_unique<StringExpression>(nullptr, "yes"), std::make_unique<StringExpression>(nullptr, "no"));
            EXPECT_EQ(textOf(conditional->evaluate()), "yes");
            EXPECT_EQ(conditional->priority(), 2);
            EXPECT_EQ(conditional->toString(), "1 < 2 ? <<yes>> : <<no>>");

            auto falseBranch = std::make_unique<ConditionalExpression>(nullptr, number(0.0), number(1.0), number(2.0));
            EXPECT_DOUBLE_EQ(quantityOf(falseBranch->evaluate()).getValue(), 2.0);

            // 分支优先级不高于条件运算符时补括号
            auto nested = std::make_unique<ConditionalExpression>(nullptr, number(1.0), binary(Operator::Less, number(2.0), number(3.0)), number(4.0));
            EXPECT_EQ(nested->toString(), "1 ? (2 < 3) : 4");

            // 条件不能判定真假时报错，而不是沿用 Python 的真值规则
            auto textCondition = std::make_unique<ConditionalExpression>(nullptr, std::make_unique<StringExpression>(nullptr, "x"), number(1.0), number(2.0));
            EXPECT_THROW(static_cast<void>(textCondition->evaluate()), Base::TypeError);

            // 条件化简后是常量：simplify 直接返回被选中分支的化简结果
            auto constantCondition = std::make_unique<ConditionalExpression>(nullptr, number(0.0), std::make_unique<StringExpression>(nullptr, "yes"),
                                                                             std::make_unique<StringExpression>(nullptr, "no"));
            ExpressionPtr simplified = constantCondition->simplify();
            EXPECT_EQ(simplified->nodeName(), "String");
            EXPECT_EQ(simplified->toString(), "<<no>>");

            // 构造期拒绝缺分支
            EXPECT_THROW(static_cast<void>(std::make_unique<ConditionalExpression>(nullptr, number(1.0), number(2.0), nullptr)), EvaluationError);
        }

        /**
         * @brief 钉住：标量函数的角度与量纲口径、参数个数校验与文本函数
         */
        TEST(ExpressionTest, ScalarFunctions)
        {
            // 三角函数按角度输入（库内角度量以度存储）
            EXPECT_NEAR(quantityOf(function(Function::Sine, number(30.0))->evaluate()).getValue(), 0.5, 1e-12);
            EXPECT_NEAR(quantityOf(function(Function::Cosine, quantity(60.0, Units::Unit::Angle))->evaluate()).getValue(), 0.5, 1e-12);
            // 反三角函数输出角度量：asin(1) = 90 度
            const Units::Quantity arcsine = quantityOf(function(Function::ArcSine, number(1.0))->evaluate());
            EXPECT_EQ(arcsine.getUnit(), Units::Unit::Angle);
            EXPECT_NEAR(arcsine.getValue(), 90.0, 1e-9);

            // 取整与绝对值保持量纲
            const Units::Quantity rounded = quantityOf(function(Function::Round, quantity(2.6, Units::Unit::Length))->evaluate());
            EXPECT_EQ(rounded.getUnit(), Units::Unit::Length);
            EXPECT_DOUBLE_EQ(rounded.getValue(), 3.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Truncate, quantity(2.6, Units::Unit::Length))->evaluate()).getValue(), 2.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Floor, quantity(-2.5, Units::Unit::Length))->evaluate()).getValue(), -3.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Ceiling, quantity(2.1, Units::Unit::Length))->evaluate()).getValue(), 3.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Absolute, quantity(-3.0, Units::Unit::Length))->evaluate()).getValue(), 3.0);

            // 平方根取量纲的平方根：sqrt(4 mm^2) = 2 mm
            const Units::Quantity root = quantityOf(function(Function::SquareRoot, quantity(4.0, Units::Unit::Area))->evaluate());
            EXPECT_EQ(root.getUnit(), Units::Unit::Length);
            EXPECT_DOUBLE_EQ(root.getValue(), 2.0);
            // 立方根同理
            EXPECT_EQ(quantityOf(function(Function::CubeRoot, quantity(27.0, Units::Unit::Volume))->evaluate()).getUnit(), Units::Unit::Length);

            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Power, number(2.0), number(10.0))->evaluate()).getValue(), 1024.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Modulo, number(7.0), number(2.0))->evaluate()).getValue(), 1.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Hypotenuse, number(3.0), number(4.0))->evaluate()).getValue(), 5.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Cathetus, number(5.0), number(4.0))->evaluate()).getValue(), 3.0);
            EXPECT_NEAR(quantityOf(function(Function::Logarithm, number(std::numbers::e))->evaluate()).getValue(), 1.0, 1e-12);
            // 对数的参数必须是纯数
            EXPECT_THROW(static_cast<void>(function(Function::Logarithm, quantity(10.0, Units::Unit::Length))->evaluate()), Base::UnitsMismatchError);

            const Units::Quantity angle = quantityOf(function(Function::ArcTangent2, number(1.0), number(1.0))->evaluate());
            EXPECT_EQ(angle.getUnit(), Units::Unit::Angle);
            EXPECT_NEAR(angle.getValue(), 45.0, 1e-9);
            // atan2 要求两侧单位一致
            EXPECT_THROW(static_cast<void>(function(Function::ArcTangent2, quantity(1.0, Units::Unit::Length), quantity(1.0, Units::Unit::TimeSpan))->evaluate()),
                         Base::UnitsMismatchError);

            // 逻辑非只看数值
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::LogicalNot, number(0.0))->evaluate()).getValue(), 1.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::LogicalNot, number(2.0))->evaluate()).getValue(), 0.0);

            // 文本函数：str 排版取值，parsequant 解析数量文本
            EXPECT_EQ(textOf(function(Function::Stringify, quantity(2.0, Units::Unit::Length))->evaluate()).find('2'), static_cast<std::size_t>(0));
            const Units::Quantity parsed = quantityOf(function(Function::ParseQuantity, std::make_unique<StringExpression>(nullptr, "1.5 mm"))->evaluate());
            EXPECT_DOUBLE_EQ(parsed.getValue(), 1.5);
            EXPECT_EQ(parsed.getUnit(), Units::Unit::Length);
            EXPECT_THROW(static_cast<void>(function(Function::ParseQuantity, std::make_unique<StringExpression>(nullptr, "abc"))->evaluate()), Base::ParserError);

            // 单元格地址：绝对引用写法，越界报错
            EXPECT_EQ(textOf(function(Function::Address, number(1.0), number(1.0))->evaluate()), "$A$1");
            EXPECT_THROW(static_cast<void>(function(Function::Address, number(1.0), number(1.0), number(9.0))->evaluate()), Base::ValueError);

            // 参数个数错：构造期即报错
            EXPECT_THROW(static_cast<void>(function(Function::SquareRoot, number(4.0), number(4.0))), EvaluationError);
            EXPECT_THROW(static_cast<void>(function(Function::Sum)), EvaluationError);
            EXPECT_THROW(static_cast<void>(function(Function::Sine)), EvaluationError);
            // 依赖宿主对象工厂的函数不可用
            EXPECT_THROW(static_cast<void>(function(Function::Create, number(1.0))), EvaluationError);
            EXPECT_THROW(static_cast<void>(function(Function::Tuple, number(1.0))), EvaluationError);
            // 哨兵值不是函数
            EXPECT_THROW(static_cast<void>(std::make_unique<FunctionExpression>(nullptr, Function::None, std::string(), std::vector<ExpressionPtr>())), Base::ParserError);
        }

        /**
         * @brief 钉住：向量函数的取值与量纲，以及零向量、错类型等拒绝面
         */
        TEST(ExpressionTest, VectorFunctions)
        {
            EXPECT_TRUE(vectorOf(function(Function::Vector, number(1.0), number(2.0), number(3.0))->evaluate()).isEqual(Base::Vector3d(1.0, 2.0, 3.0), 1e-12));

            const Base::Vector3d normalized = vectorOf(function(Function::VectorNormalize, vectorNode(3.0, 0.0, 0.0))->evaluate());
            EXPECT_NEAR(normalized.x, 1.0, 1e-12);
            EXPECT_NEAR(normalized.y, 0.0, 1e-12);
            // 零向量没有方向
            EXPECT_THROW(static_cast<void>(function(Function::VectorNormalize, vectorNode(0.0, 0.0, 0.0))->evaluate()), Base::ValueError);

            // 点积是无量纲纯数
            EXPECT_DOUBLE_EQ(doubleOf(function(Function::VectorDot, vectorNode(1.0, 0.0, 0.0), vectorNode(2.0, 3.0, 4.0))->evaluate()), 2.0);
            // 叉积
            const Base::Vector3d cross = vectorOf(function(Function::VectorCross, vectorNode(1.0, 0.0, 0.0), vectorNode(0.0, 1.0, 0.0))->evaluate());
            EXPECT_NEAR(cross.z, 1.0, 1e-12);
            // 夹角带角度单位
            const Units::Quantity angle = quantityOf(function(Function::VectorAngle, vectorNode(1.0, 0.0, 0.0), vectorNode(0.0, 1.0, 0.0))->evaluate());
            EXPECT_EQ(angle.getUnit(), Units::Unit::Angle);
            EXPECT_NEAR(angle.getValue(), 90.0, 1e-9);

            // 缩放：分量要求长度量或纯数
            const Base::Vector3d scaled = vectorOf(function(Function::VectorScale, vectorNode(1.0, 1.0, 1.0), number(2.0), number(3.0), number(4.0))->evaluate());
            EXPECT_NEAR(scaled.x, 2.0, 1e-12);
            EXPECT_NEAR(scaled.y, 3.0, 1e-12);
            EXPECT_NEAR(scaled.z, 4.0, 1e-12);
            EXPECT_NEAR(vectorOf(function(Function::VectorScaleY, vectorNode(1.0, 1.0, 1.0), number(5.0))->evaluate()).y, 5.0, 1e-12);
            EXPECT_THROW(static_cast<void>(function(Function::VectorScaleX, vectorNode(1.0, 1.0, 1.0), quantity(5.0, Units::Unit::TimeSpan))->evaluate()),
                         Base::UnitsMismatchError);

            // 点到直线的距离带长度单位
            const Units::Quantity distance =
                    quantityOf(function(Function::VectorLineDistance, vectorNode(0.0, 1.0, 0.0), vectorNode(0.0, 0.0, 0.0), vectorNode(1.0, 0.0, 0.0))->evaluate());
            EXPECT_EQ(distance.getUnit(), Units::Unit::Length);
            EXPECT_NEAR(distance.getValue(), 1.0, 1e-12);
            // 点在线上的投影
            const Base::Vector3d projection =
                    vectorOf(function(Function::VectorLineProjection, vectorNode(1.0, 2.0, 0.0), vectorNode(0.0, 0.0, 0.0), vectorNode(1.0, 0.0, 0.0))->evaluate());
            EXPECT_NEAR(projection.x, 1.0, 1e-12);
            EXPECT_NEAR(projection.y, 0.0, 1e-12);

            // 参数类型不符：向量函数不接受纯数
            EXPECT_THROW(static_cast<void>(function(Function::VectorDot, number(1.0), vectorNode(1.0, 0.0, 0.0))->evaluate()), Base::TypeError);
        }

        /**
         * @brief 钉住：矩阵、旋转与位姿函数按 Base 几何类型直接计算，参数不符时报错
         */
        TEST(ExpressionTest, MatrixAndPlacementFunctions)
        {
            // 16 个分量按行优先拼出单位矩阵
            EXPECT_TRUE(matrixOf(function(Function::Matrix, number(1.0), number(0.0), number(0.0), number(0.0), number(0.0), number(1.0), number(0.0), number(0.0), number(0.0),
                            number(0.0), number(1.0), number(0.0), number(0.0), number(0.0), number(0.0), number(1.0))
                        ->evaluate())
                    .isUnity());
            // 实参个数只能是 1 到 16 个：0 个与 3 个都在构造期或求值期被拒
            EXPECT_THROW(static_cast<void>(function(Function::Matrix)->evaluate()), EvaluationError);
            EXPECT_THROW(static_cast<void>(function(Function::Matrix, number(1.0), number(0.0), number(0.0))->evaluate()), EvaluationError);

            // translationm 造平移矩阵
            const Base::Matrix4D translation = matrixOf(function(Function::TranslationMatrix, vectorNode(1.0, 2.0, 3.0))->evaluate());
            EXPECT_NEAR(translation[0][3], 1.0, 1e-12);
            EXPECT_NEAR(translation[1][3], 2.0, 1e-12);
            EXPECT_NEAR(translation[2][3], 3.0, 1e-12);
            // 三个分量形式要求长度量或纯数
            EXPECT_THROW(static_cast<void>(function(Function::TranslationMatrix, quantity(1.0, Units::Unit::TimeSpan), number(2.0), number(3.0))->evaluate()),
                         Base::UnitsMismatchError);

            // mrotatez：绕 Z 轴转 90 度，X 轴映射到 Y 轴
            auto                  rotateZ          = function(Function::MatrixRotateZ, valueNode(Value(Base::Placement())), number(90.0));
            const Base::Placement rotatedPlacement = placementOf(rotateZ->evaluate());
            const Base::Vector3d  mapped           = rotatedPlacement.toMatrix() * Base::Vector3d(1.0, 0.0, 0.0);
            EXPECT_NEAR(mapped.x, 0.0, 1e-12);
            EXPECT_NEAR(mapped.y, 1.0, 1e-12);
            // 旋转角度必须是无量纲或角度量
            EXPECT_THROW(static_cast<void>(function(Function::MatrixRotateZ, valueNode(Value(Base::Placement())), quantity(1.0, Units::Unit::Length))->evaluate()),
                         Base::UnitsMismatchError);
            // 第一个参数必须是矩阵、位姿或旋转
            EXPECT_THROW(static_cast<void>(function(Function::MatrixRotateZ, number(1.0), number(90.0))->evaluate()), Base::TypeError);

            // mscale：两个参数时第二个参数是向量
            const Base::Matrix4D scaledMatrix =
                    matrixOf(function(Function::MatrixScale, valueNode(Value(Base::Matrix4D())), function(Function::Vector, number(2.0), number(2.0), number(2.0)))->evaluate());
            const Base::Vector3d scaledPoint = scaledMatrix * Base::Vector3d(1.0, 1.0, 1.0);
            EXPECT_NEAR(scaledPoint.x, 2.0, 1e-12);

            // minvert：位姿求逆得到反向平移
            const Base::Placement inverted =
                    placementOf(function(Function::MatrixInvert, valueNode(Value(Base::Placement(Base::Vector3d(1.0, 2.0, 3.0), Base::Rotation()))))->evaluate());
            EXPECT_NEAR(inverted.getPosition().x, -1.0, 1e-12);
            // 奇异矩阵不可逆
            Base::Matrix4D singular;
            singular.scale(0.0);
            EXPECT_THROW(static_cast<void>(function(Function::MatrixInvert, valueNode(Value(singular)))->evaluate()), Base::ValueError);
            EXPECT_THROW(static_cast<void>(function(Function::MatrixInvert, number(1.0))->evaluate()), Base::TypeError);

            // rotation：轴加角度（按度）
            const Base::Rotation rotation = rotationOf(function(Function::Rotation, vectorNode(0.0, 0.0, 1.0), number(90.0))->evaluate());
            Base::Matrix4D       rotationMatrix;
            rotation.getValue(rotationMatrix);
            const Base::Vector3d rotatedPoint = rotationMatrix * Base::Vector3d(1.0, 0.0, 0.0);
            EXPECT_NEAR(rotatedPoint.y, 1.0, 1e-12);
            // 三个数值按 yaw/pitch/roll（弧度）
            EXPECT_TRUE(rotationOf(function(Function::Rotation, number(0.0), number(0.0), number(0.0))->evaluate()).isIdentity());
            // 第一个参数必须是轴向量
            EXPECT_THROW(static_cast<void>(function(Function::Rotation, number(1.0), number(2.0))->evaluate()), Base::TypeError);

            // placement(位置; 旋转)
            const Base::Placement placement =
                    placementOf(function(Function::Placement, vectorNode(1.0, 2.0, 3.0), function(Function::Rotation, vectorNode(0.0, 0.0, 1.0), number(0.0)))->evaluate());
            EXPECT_NEAR(placement.getPosition().y, 2.0, 1e-12);
            // 单参数形式需要矩阵或位姿
            EXPECT_THROW(static_cast<void>(function(Function::Placement, number(1.0))->evaluate()), Base::TypeError);
        }

        /**
         * @brief 钉住：聚合函数的取值口径与拒绝面——类型不符报错而不是静默跳过
         */
        TEST(ExpressionTest, AggregateFunctions)
        {
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Sum, number(1.0), number(2.0), number(3.0))->evaluate()).getValue(), 6.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Average, number(1.0), number(2.0), number(3.0))->evaluate()).getValue(), 2.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Minimum, number(3.0), number(1.0))->evaluate()).getValue(), 1.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Maximum, number(3.0), number(1.0))->evaluate()).getValue(), 3.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::Count, number(3.0), number(1.0))->evaluate()).getValue(), 2.0);

            // 逻辑聚合只看数值
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::LogicalAnd, number(1.0), number(0.0))->evaluate()).getValue(), 0.0);
            EXPECT_DOUBLE_EQ(quantityOf(function(Function::LogicalOr, number(0.0), number(2.0))->evaluate()).getValue(), 1.0);

            // 求和保持量纲，混量纲报错
            const Units::Quantity total = quantityOf(function(Function::Sum, quantity(1.0, Units::Unit::Length), quantity(2.0, Units::Unit::Length))->evaluate());
            EXPECT_EQ(total.getUnit(), Units::Unit::Length);
            EXPECT_DOUBLE_EQ(total.getValue(), 3.0);
            EXPECT_THROW(static_cast<void>(function(Function::Sum, quantity(1.0, Units::Unit::Length), quantity(2.0, Units::Unit::TimeSpan))->evaluate()),
                         Base::UnitsMismatchError);

            // 样本标准差：长度量的标准差仍是长度
            const Units::Quantity deviation = quantityOf(function(Function::StandardDeviation, quantity(1.0, Units::Unit::Length), quantity(3.0, Units::Unit::Length))->evaluate());
            EXPECT_EQ(deviation.getUnit(), Units::Unit::Length);
            EXPECT_NEAR(deviation.getValue(), std::sqrt(2.0), 1e-12);
            // 单样本上样本标准差没有定义
            EXPECT_THROW(static_cast<void>(function(Function::StandardDeviation, number(1.0))->evaluate()), EvaluationError);

            // 文本参与聚合是类型错，不静默跳过
            EXPECT_THROW(static_cast<void>(function(Function::Sum, std::make_unique<StringExpression>(nullptr, "abc"))->evaluate()), Base::TypeError);
        }

        /**
         * @brief 钉住：聚合函数按单元格地址读取区间，空单元格跳过，缺解析器或非法地址报错
         */
        TEST(ExpressionTest, RangeAggregateReadsCells)
        {
            FakeResolver resolver;
            FakeObject & sheet = resolver.addObject("Sheet", "Doc");
            sheet.addProperty("A1", "Length", Value(Units::Quantity(1.0, Units::Unit::Length)));
            sheet.addProperty("A2", "Length", Value(Units::Quantity(2.0, Units::Unit::Length)));
            resolver.setCurrentObject(&sheet);

            auto range = std::make_unique<RangeExpression>(&resolver, "A1", "A2");
            EXPECT_EQ(range->toString(), "A1:A2");
            EXPECT_EQ(range->asRangeExpression(), range.get());
            // 区间本身没有标量取值
            EXPECT_THROW(static_cast<void>(range->evaluate()), EvaluationError);

            std::vector<ExpressionPtr> arguments;
            arguments.push_back(std::move(range));
            auto sum = std::make_unique<FunctionExpression>(&resolver, Function::Sum, "sum", std::move(arguments));
            EXPECT_DOUBLE_EQ(quantityOf(sum->evaluate()).getValue(), 3.0);

            // 区间里的空单元格跳过
            std::vector<ExpressionPtr> partialArguments;
            partialArguments.push_back(std::make_unique<RangeExpression>(&resolver, "A1", "A3"));
            auto partialSum = std::make_unique<FunctionExpression>(&resolver, Function::Sum, "sum", std::move(partialArguments));
            EXPECT_DOUBLE_EQ(quantityOf(partialSum->evaluate()).getValue(), 3.0);

            // 没有解析器时无法定位单元格
            std::vector<ExpressionPtr> unboundArguments;
            unboundArguments.push_back(std::make_unique<RangeExpression>(nullptr, "A1", "A2"));
            auto unboundSum = std::make_unique<FunctionExpression>(nullptr, Function::Sum, "sum", std::move(unboundArguments));
            EXPECT_THROW(static_cast<void>(unboundSum->evaluate()), Base::NameError);

            // 非法单元格地址在取区间时报错
            auto invalidRange = std::make_unique<RangeExpression>(&resolver, "?", "A2");
            EXPECT_THROW(static_cast<void>(invalidRange->getRange()), EvaluationError);
        }

        /**
         * @brief 钉住：变量引用按文档名与对象名解析属性，缺失解析器、对象、属性、取值都报错
         */
        TEST(ExpressionTest, VariableResolutionAndWriteBack)
        {
            FakeResolver resolver;
            FakeObject & box = resolver.addObject("Box", "Doc");
            box.addProperty("Length", "Length", Value(Units::Quantity(2.0, Units::Unit::Length)));

            VariableExpression::Reference reference;
            reference.objectName   = "Box";
            reference.propertyName = "Length";
            auto variable          = std::make_unique<VariableExpression>(&resolver, reference);
            EXPECT_EQ(variable->pathText(), "Box.Length");
            EXPECT_EQ(variable->name(), "Length");
            EXPECT_EQ(variable->toString(), "Box.Length");
            EXPECT_DOUBLE_EQ(quantityOf(variable->evaluate()).getValue(), 2.0);
            // 化简保持引用不变
            EXPECT_EQ(variable->simplify()->toString(), "Box.Length");

            // 写回宿主属性
            variable->assignValue(Value(Units::Quantity(5.0, Units::Unit::Length)));
            EXPECT_DOUBLE_EQ(quantityOf(variable->evaluate()).getValue(), 5.0);

            // 只读属性拒绝写入
            auto *lengthProperty = dynamic_cast<FakeProperty *>(box.findProperty("Length"));
            ASSERT_NE(lengthProperty, nullptr);
            lengthProperty->setReadOnly(true);
            EXPECT_THROW(variable->assignValue(Value(Units::Quantity(1.0, Units::Unit::Length))), Base::AttributeError);

            // 属性不存在：报错时列出该对象的属性
            VariableExpression::Reference missingProperty;
            missingProperty.objectName   = "Box";
            missingProperty.propertyName = "Width";
            auto missing                 = std::make_unique<VariableExpression>(&resolver, missingProperty);
            EXPECT_THROW(static_cast<void>(missing->evaluate()), Base::AttributeError);
            EXPECT_THROW(missing->assignValue(Value(1.0)), Base::AttributeError);

            // 对象不存在
            VariableExpression::Reference missingObject;
            missingObject.objectName   = "Cylinder";
            missingObject.propertyName = "Length";
            auto unboundObject         = std::make_unique<VariableExpression>(&resolver, missingObject);
            EXPECT_THROW(static_cast<void>(unboundObject->evaluate()), Base::NameError);

            // 没有解析器时引用无法解析
            VariableExpression::Reference local;
            local.propertyName = "Length";
            auto unbound       = std::make_unique<VariableExpression>(nullptr, local);
            EXPECT_EQ(unbound->pathText(), "Length");
            EXPECT_THROW(static_cast<void>(unbound->evaluate()), Base::NameError);
            EXPECT_THROW(unbound->assignValue(Value(1.0)), Base::NameError);

            // 属性尚未赋值
            box.addProperty("Empty", "Length", std::nullopt);
            VariableExpression::Reference empty;
            empty.objectName   = "Box";
            empty.propertyName = "Empty";
            auto emptyVariable = std::make_unique<VariableExpression>(&resolver, empty);
            EXPECT_THROW(static_cast<void>(emptyVariable->evaluate()), Base::AttributeError);

            // 文档名限定的引用走同一条解析路径
            VariableExpression::Reference qualified;
            qualified.documentName = "Doc";
            qualified.objectName   = "Box";
            qualified.propertyName = "Length";
            auto qualifiedVariable = std::make_unique<VariableExpression>(&resolver, qualified);
            EXPECT_EQ(qualifiedVariable->pathText(), "<<Doc>>.Box.Length");
            EXPECT_DOUBLE_EQ(quantityOf(qualifiedVariable->evaluate()).getValue(), 5.0);
        }

        /**
         * @brief 钉住：分量随节点一起保存、打印与拷贝，求值时按值语义作用在取值上
         */
        TEST(ExpressionTest, ComponentsApplyToEvaluatedValues)
        {
            auto expression = std::make_unique<NumberExpression>(nullptr, Units::Quantity(2.0));
            expression->addComponent(Expression::Component("Length"));
            EXPECT_TRUE(expression->hasComponent());
            EXPECT_EQ(expression->components().size(), static_cast<std::size_t>(1));
            EXPECT_EQ(expression->toString(), "(2).Length");
            // 名字分量指向对象子属性，值层面无法解析：报属性错而不是静默取整体值
            EXPECT_THROW(static_cast<void>(expression->evaluate()), Base::AttributeError);

            // 拷贝会带上分量，相等判定把分量算在内
            ExpressionPtr copied = expression->copy();
            EXPECT_TRUE(copied->hasComponent());
            EXPECT_EQ(copied->toString(), "(2).Length");
            EXPECT_TRUE(expression->isSame(*copied));
            copied->addComponent(Expression::Component("Width"));
            EXPECT_FALSE(expression->isSame(*copied));

            // 变量引用可以索引，下标分量按索引写法排版
            auto variable = std::make_unique<VariableExpression>(nullptr, VariableExpression::Reference());
            variable->addComponent(Expression::Component::arrayIndex(std::make_unique<NumberExpression>(nullptr, Units::Quantity(0.0))));
            EXPECT_EQ(variable->toString(), "[0]");
            // 没有解析器时先报「解析不到对象」，而不是把分量说成属性不存在
            EXPECT_THROW(static_cast<void>(variable->evaluate()), Base::NameError);

            // 名字、映射键与区间的文本写法
            auto keyed = std::make_unique<NumberExpression>(nullptr, Units::Quantity(1.0));
            keyed->addComponent(Expression::Component::mapKey("Length"));
            EXPECT_EQ(keyed->toString(), "(1)[<<Length>>]");
            auto ranged = std::make_unique<NumberExpression>(nullptr, Units::Quantity(1.0));
            ranged->addComponent(Expression::Component::rangeComponent(std::make_unique<NumberExpression>(nullptr, Units::Quantity(1.0)),
                                                                       std::make_unique<NumberExpression>(nullptr, Units::Quantity(3.0))));
            EXPECT_EQ(ranged->toString(), "(1)[1:3]");
        }

        /**
         * @brief 钉住：化简只折叠常量，含引用的表达式保持结构；evaluateToConstantNode 把取值包成常量节点
         */
        TEST(ExpressionTest, SimplifyFoldsConstantsOnly)
        {
            auto          sum    = binary(Operator::Add, number(2.0), number(3.0));
            ExpressionPtr folded = sum->simplify();
            EXPECT_EQ(folded->nodeName(), "Number");
            EXPECT_EQ(folded->toString(), "5");

            FakeResolver resolver;
            FakeObject & box = resolver.addObject("Box", "Doc");
            box.addProperty("Length", "Length", Value(Units::Quantity(2.0, Units::Unit::Length)));
            auto variable = std::make_unique<VariableExpression>(&resolver, VariableExpression::Reference{.documentName = "", .objectName = "Box", .propertyName = "Length"});

            auto          product    = binary(Operator::Multiply, number(2.0), std::move(variable));
            ExpressionPtr simplified = product->simplify();
            EXPECT_EQ(simplified->nodeName(), "Operator");
            EXPECT_EQ(simplified->toString(), "2 * Box.Length");
            EXPECT_DOUBLE_EQ(quantityOf(simplified->evaluate()).getValue(), 4.0);

            // 函数实参全为常量时折叠求值
            EXPECT_EQ(function(Function::Sine, number(30.0))->simplify()->nodeName(), "Number");
            // 含引用的函数保持结构
            EXPECT_EQ(function(Function::Absolute,
                          std::make_unique<VariableExpression>(&resolver, VariableExpression::Reference{.documentName = "", .objectName = "Box", .propertyName = "Length"}))
                      ->simplify()
                      ->nodeName(),
                      "Function");

            // evaluateToConstantNode 把取值包成常量节点
            EXPECT_EQ(number(4.0)->evaluateToConstantNode()->nodeName(), "Number");
            EXPECT_EQ(vectorNode(1.0, 0.0, 0.0)->evaluateToConstantNode()->nodeName(), "Value");
        }

        /**
         * @brief 钉住：取值包装——数值、文本、布尔各有专用节点，几何值用取值节点承载
         */
        TEST(ExpressionTest, ValueExpressionWrapsEvaluatedValues)
        {
            EXPECT_EQ(makeValueExpression(nullptr, Value(2.5))->nodeName(), "Number");
            EXPECT_EQ(makeValueExpression(nullptr, Value(std::string("a")))->nodeName(), "String");
            EXPECT_EQ(makeValueExpression(nullptr, Value(Units::Quantity(2.0, Units::Unit::Length)))->nodeName(), "Number");

            ExpressionPtr constant = makeValueExpression(nullptr, Value(true));
            EXPECT_EQ(constant->nodeName(), "Constant");
            EXPECT_TRUE(boolOf(constant->evaluate()));
            // True 与 False 是布尔常量，不按数值参与运算
            EXPECT_FALSE(dynamic_cast<const ConstantExpression &>(*constant).isNumber());

            const ExpressionPtr wrapped = makeValueExpression(nullptr, Value(Base::Vector3d(1.0, 2.0, 3.0)));
            EXPECT_EQ(wrapped->nodeName(), "Value");
            // AST 文本写成可重新解析的构造调用；面向用户的紧凑写法仍是 (1, 2, 3)
            EXPECT_EQ(wrapped->toString(), "vector(1; 2; 3)");
            EXPECT_EQ(toString(Value(Base::Vector3d(1.0, 2.0, 3.0))), "(1, 2, 3)");
            EXPECT_TRUE(vectorOf(wrapped->evaluate()).isEqual(Base::Vector3d(1.0, 2.0, 3.0), 1e-12));
        }

    } // namespace
}     // namespace ExpressionEngine::Expression
