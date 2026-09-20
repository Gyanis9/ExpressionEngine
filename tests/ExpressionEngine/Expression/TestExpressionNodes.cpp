// 本文件钉住 AST 节点的访问器、注释通道，以及「回写文本必须解析回同一棵树」的加括号规则。

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Expression/Expression.h>
#include <ExpressionEngine/Expression/ExpressionParser.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Expression
{
    namespace
    {

        using Operator = OperatorExpression::Operator;

        /// 造一个数值节点
        ExpressionPtr numberNode(const double value)
        {
            return std::make_unique<NumberExpression>(nullptr, Units::Quantity(value));
        }

        /// 造一个二元运算节点
        ExpressionPtr binary(const Operator operation, ExpressionPtr left, ExpressionPtr right)
        {
            return std::make_unique<OperatorExpression>(nullptr, std::move(left), operation, std::move(right));
        }

        /// 解析文本再回写成文本：往返之后含义必须不变
        std::string roundTrip(const std::string &text)
        {
            return ExpressionParser::parse(nullptr, text)->toString();
        }

        /// 解析并求值，取无量纲数值
        double evaluateText(const std::string &text)
        {
            const Value value = ExpressionParser::parse(nullptr, text)->evaluate();
            return std::get<Units::Quantity>(value).getValue();
        }

        /**
         * @brief 钉住：幂是右结合，左结合的写法必须保住括号
         * @details (2^3)^4 是 4096，而 2^3^4 会解析成 2^(3^4)，回写丢了括号就等于换了值。
         */
        TEST(ExpressionNodes, LeftGroupedPowerKeepsItsParentheses)
        {
            EXPECT_EQ(roundTrip("(2^3)^4"), "(2 ^ 3) ^ 4");
            EXPECT_DOUBLE_EQ(evaluateText("(2^3)^4"), 4096.0);
            EXPECT_DOUBLE_EQ(evaluateText(roundTrip("(2^3)^4")), 4096.0);

            EXPECT_EQ(roundTrip("2^(3^4)"), "2 ^ (3 ^ 4)");
            EXPECT_DOUBLE_EQ(evaluateText(roundTrip("2^(3^4)")), evaluateText("2^81"));
        }

        /**
         * @brief 钉住：可自由换括号的运算不被多包，不能换括号的必须包
         */
        TEST(ExpressionNodes, ParenthesesFollowRegroupingSafety)
        {
            // 加法与乘法可交换可重组合，两侧同类运算都不用额外括号
            EXPECT_EQ(roundTrip("(8 - 3) - 2"), "8 - 3 - 2");
            EXPECT_EQ(roundTrip("(8 / 4) / 2"), "8 / 4 / 2");
            EXPECT_EQ(roundTrip("1 + (2 + 3)"), "1 + 2 + 3");
            EXPECT_EQ(roundTrip("2 * (3 * 4)"), "2 * 3 * 4");

            // 换了分组就换结果的写法必须带括号回来
            EXPECT_EQ(roundTrip("8 - (3 - 2)"), "8 - (3 - 2)");
            EXPECT_EQ(roundTrip("8 / (4 / 2)"), "8 / (4 / 2)");
            EXPECT_DOUBLE_EQ(evaluateText(roundTrip("8 - (3 - 2)")), 7.0);

            // 低优先级的子式一律包：乘法里的加减、幂里的乘除
            EXPECT_EQ(roundTrip("(1 + 2) * 3"), "(1 + 2) * 3");
            EXPECT_EQ(roundTrip("2 * 3 + 4"), "2 * 3 + 4");
            EXPECT_EQ(roundTrip("2^(3 * 4)"), "2 ^ (3 * 4)");
        }

        /**
         * @brief 钉住：二元运算符文本与运算符互转，一元写法则故意与二元共用符号
         */
        TEST(ExpressionNodes, OperatorTextRoundTrips)
        {
            const std::vector<Operator> operators{Operator::Add,     Operator::Subtract, Operator::Multiply, Operator::Divide, Operator::Modulo, Operator::Power,
                                                  Operator::Equal,   Operator::NotEqual, Operator::Less,     Operator::Greater,
                                                  Operator::LessEqual, Operator::GreaterEqual};

            for (const Operator operation: operators)
            {
                const std::string_view text = OperatorExpression::operatorText(operation);
                EXPECT_FALSE(text.empty()) << "运算符缺少文本写法";
                EXPECT_EQ(OperatorExpression::operatorFromText(text), operation) << text;
            }

            // 一元取负与单位后置的写法无法从文本单独还原：同一种符号要由解析器按上下文判定
            EXPECT_EQ(OperatorExpression::operatorText(Operator::Negate), "-");
            EXPECT_EQ(OperatorExpression::operatorFromText("-"), Operator::Subtract);
            EXPECT_EQ(OperatorExpression::operatorText(Operator::Positive), "+");
            EXPECT_EQ(OperatorExpression::operatorFromText("+"), Operator::Add);
            EXPECT_EQ(OperatorExpression::operatorText(Operator::UnitScale), " ");

            // 认不出的文本回落成 None，而不是硬猜一个运算符
            EXPECT_EQ(OperatorExpression::operatorFromText("~"), Operator::None);
            EXPECT_EQ(OperatorExpression::operatorFromText(""), Operator::None);
            EXPECT_EQ(OperatorExpression::operatorText(Operator::None), "?");
        }

        /**
         * @brief 钉住：运算符节点的种类名、操作数读写与一元节点的右操作数为空
         */
        TEST(ExpressionNodes, OperatorNodeAccessors)
        {
            auto sum = std::make_unique<OperatorExpression>(nullptr, numberNode(1.0), Operator::Add, numberNode(2.0));
            EXPECT_EQ(sum->nodeName(), "Operator");
            EXPECT_EQ(sum->getOperator(), Operator::Add);
            ASSERT_NE(sum->getLeft(), nullptr);
            ASSERT_NE(sum->getRight(), nullptr);
            EXPECT_EQ(sum->toString(), "1 + 2");
            EXPECT_TRUE(sum->asOperatorExpression() == sum.get());

            // 换掉右操作数之后文本与求值都要跟着变
            sum->setRight(numberNode(10.0));
            EXPECT_EQ(sum->toString(), "1 + 10");

            auto replaced = binary(Operator::Multiply, numberNode(3.0), numberNode(4.0));
            sum->setLeft(std::move(replaced));
            EXPECT_EQ(sum->toString(), "3 * 4 + 10");

            // 一元运算没有右操作数，比较与可交换性都按运算符本身回答
            auto negated = std::make_unique<OperatorExpression>(nullptr, numberNode(2.0), Operator::Negate, nullptr);
            EXPECT_EQ(negated->getRight(), nullptr);
            EXPECT_EQ(negated->toString(), "-2");

            const OperatorExpression add(nullptr, numberNode(1.0), Operator::Add, numberNode(2.0));
            const OperatorExpression subtract(nullptr, numberNode(1.0), Operator::Subtract, numberNode(2.0));
            const OperatorExpression power(nullptr, numberNode(2.0), Operator::Power, numberNode(3.0));
            EXPECT_TRUE(add.isCommutative());
            EXPECT_FALSE(subtract.isCommutative());
            // 幂是右结合，其余二元运算都按左结合回写
            EXPECT_FALSE(power.isLeftAssociative());
            EXPECT_TRUE(add.isLeftAssociative());
            EXPECT_TRUE(subtract.isLeftAssociative());
        }

        /**
         * @brief 钉住：注释跟着拷贝走，且只在要求检查注释时参与相等判定
         */
        TEST(ExpressionNodes, CommentIsMetadataNotText)
        {
            const ExpressionPtr plain   = ExpressionParser::parse(nullptr, "1 + 2");
            const ExpressionPtr commented = ExpressionParser::parse(nullptr, "1 + 2");
            EXPECT_TRUE(plain->comment().empty());

            // 注释不属于表达式文本：回写里没有它
            commented->setComment("边长余量");
            EXPECT_EQ(commented->comment(), "边长余量");
            EXPECT_EQ(commented->toString(), plain->toString());

            EXPECT_FALSE(commented->isSame(*plain));
            EXPECT_TRUE(commented->isSame(*plain, false));

            // 深拷贝要带上注释，否则宿主复制表达式时元数据会丢
            const ExpressionPtr copy = commented->copy();
            EXPECT_EQ(copy->comment(), "边长余量");
            EXPECT_TRUE(copy->isSame(*commented));
        }

        /**
         * @brief 钉住：单位节点的数值、单位文本与倍率三个视角
         * @details 单位节点自身只写单位符号，倍率由父节点的左操作数承担，
         *          所以 "2 mm" 的文本要经 UnitScale 才拼得出来。
         */
        TEST(ExpressionNodes, UnitNodeAccessors)
        {
            auto unit = std::make_unique<UnitExpression>(nullptr, Units::Quantity(2.0, Units::Unit::Length), "mm");
            EXPECT_EQ(unit->nodeName(), "Unit");
            EXPECT_DOUBLE_EQ(unit->getQuantity().getValue(), 2.0);
            EXPECT_DOUBLE_EQ(unit->getScaler(), 2.0);
            EXPECT_EQ(unit->getUnitText(), "mm");
            EXPECT_EQ(unit->toString(), "mm");

            unit->setQuantity(Units::Quantity(3.0, Units::Unit::Length));
            EXPECT_DOUBLE_EQ(unit->getScaler(), 3.0);
            unit->setUnit(Units::Quantity(4.0, Units::Unit::Length));
            EXPECT_DOUBLE_EQ(unit->getQuantity().getValue(), 4.0);

            // 解析器把 "2 mm" 建成普通乘法（与用户显式写 * 同义），紧凑写法只属于手工建树
            const ExpressionPtr parsedScale = ExpressionParser::parse(nullptr, "2 mm");
            EXPECT_EQ(parsedScale->toString(), "2 * mm");
            const auto *multiply = parsedScale->asOperatorExpression();
            ASSERT_NE(multiply, nullptr);
            EXPECT_EQ(multiply->getOperator(), Operator::Multiply);

            auto compact = binary(Operator::UnitScale, numberNode(2.0),
                                  std::make_unique<UnitExpression>(nullptr, Units::Quantity(1.0, Units::Unit::Length), "mm"));
            EXPECT_EQ(compact->toString(), "2 mm");
            const auto *unitOperand = dynamic_cast<const UnitExpression *>(compact->asOperatorExpression()->getRight());
            ASSERT_NE(unitOperand, nullptr);
            EXPECT_EQ(unitOperand->getUnitText(), "mm");
        }

        /**
         * @brief 钉住：叶子与函数节点的取值入口，以及分量存在的判定
         */
        TEST(ExpressionNodes, LeafAndFunctionAccessors)
        {
            EXPECT_EQ(std::string(ConstantExpression(nullptr, "pi", Units::Quantity(3.0)).nodeName()), "Constant");
            EXPECT_EQ(numberNode(1.0)->nodeName(), "Number");
            EXPECT_TRUE(numberNode(1.0)->isConstantNumeric());
            EXPECT_FALSE(ExpressionParser::parse(nullptr, "Box.Length")->isConstantNumeric());

            EXPECT_EQ(StringExpression(nullptr, "abc").getText(), "abc");

            std::vector<ExpressionPtr> arguments;
            arguments.push_back(numberNode(1.0));
            arguments.push_back(numberNode(2.0));
            auto sum = std::make_unique<FunctionExpression>(nullptr, FunctionExpression::Function::Sum, "sum", std::move(arguments));
            EXPECT_EQ(sum->getFunction(), FunctionExpression::Function::Sum);
            EXPECT_EQ(sum->getArguments().size(), 2U);
            EXPECT_EQ(sum->toString(), "sum(1; 2)");

            // 分量存在性只在带 .名字 或 [下标] 时为真
            EXPECT_FALSE(sum->hasComponent());
            EXPECT_TRUE(ExpressionParser::parse(nullptr, "list(1; 2)[0]")->hasComponent());
        }

        /// 取引用节点的路径三要素；根节点不是引用时让用例失败
        VariableExpression::Reference referenceOf(const std::string &text)
        {
            const ExpressionPtr node = ExpressionParser::parse(nullptr, text);
            const auto *variable = dynamic_cast<const VariableExpression *>(node.get());
            if (variable == nullptr)
            {
                ADD_FAILURE() << text << " 的根节点不是变量引用，而是 " << node->nodeName();
                return {};
            }
            return variable->getReference();
        }

        /**
         * @brief 钉住：四种引用写法分别落到文档名、对象名与属性名的哪一格
         */
        TEST(ExpressionNodes, ReferenceFormsFillTheThreeFields)
        {
            // 裸名字与显式的当前对象写法都只给属性名，对象由宿主的当前对象承担
            const VariableReference bare = referenceOf("Length");
            EXPECT_TRUE(bare.documentName.empty());
            EXPECT_TRUE(bare.objectName.empty());
            EXPECT_EQ(bare.propertyName, "Length");
            EXPECT_EQ(referenceOf(".Length").propertyName, "Length");

            const VariableReference qualified = referenceOf("Box.Length");
            EXPECT_TRUE(qualified.documentName.empty());
            EXPECT_EQ(qualified.objectName, "Box");
            EXPECT_EQ(qualified.propertyName, "Length");

            const VariableReference documented = referenceOf("<<Part>>.Box.Length");
            EXPECT_EQ(documented.documentName, "Part");
            EXPECT_EQ(documented.objectName, "Box");
            EXPECT_EQ(documented.propertyName, "Length");

            // 带 # 的写法一次给出文档与目标，对象名留空
            const VariableReference crossDocument = referenceOf("<<Sheet#A1>>");
            EXPECT_EQ(crossDocument.documentName, "Sheet");
            EXPECT_TRUE(crossDocument.objectName.empty());
            EXPECT_EQ(crossDocument.propertyName, "A1");

            // 引号文本可以落在对象名与属性名上，带点的名字整段交给宿主去查
            const VariableReference quotedObject = referenceOf("<<Doc>>.<<a.b>>.Length");
            EXPECT_EQ(quotedObject.documentName, "Doc");
            EXPECT_EQ(quotedObject.objectName, "a.b");
            EXPECT_EQ(quotedObject.propertyName, "Length");

            const VariableReference quotedProperty = referenceOf("Box.<<a.b>>");
            EXPECT_EQ(quotedProperty.objectName, "Box");
            EXPECT_EQ(quotedProperty.propertyName, "a.b");
        }

        /**
         * @brief 钉住：引用的文本写法能解析回同一条路径，改路径要经 setReference
         */
        TEST(ExpressionNodes, ReferenceTextRoundTrips)
        {
            for (const std::string &text: {"Length", "Box.Length", "<<Part>>.Box.Length", "<<Sheet#A1>>"})
            {
                const ExpressionPtr node = ExpressionParser::parse(nullptr, text);
                const std::string written = node->toString();
                EXPECT_EQ(referenceOf(written), referenceOf(text)) << written << " 没能解析回原路径";
            }

            auto variable = std::make_unique<VariableExpression>(nullptr, VariableReference{"", "", "Length"});
            EXPECT_EQ(variable->pathText(), "Length");
            EXPECT_EQ(variable->name(), "Length");

            variable->setReference(VariableReference{"Doc", "Box", "Length"});
            EXPECT_EQ(variable->pathText(), "<<Doc>>.Box.Length");
            EXPECT_EQ(variable->toString(), "<<Doc>>.Box.Length");
            EXPECT_EQ(variable->copy()->toString(), "<<Doc>>.Box.Length");
        }

    } // namespace
}     // namespace ExpressionEngine::Expression
