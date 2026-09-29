// 本文件钉住 AST 节点的访问器、注释通道，以及「回写文本必须解析回同一棵树」的加括号规则。

#include <gtest/gtest.h>

#include <cstddef>
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
            const std::vector<Operator> operators{Operator::Add,   Operator::Subtract, Operator::Multiply, Operator::Divide,  Operator::Modulo,    Operator::Power,
                                                  Operator::Equal, Operator::NotEqual, Operator::Less,     Operator::Greater, Operator::LessEqual, Operator::GreaterEqual};

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
            const ExpressionPtr plain     = ExpressionParser::parse(nullptr, "1 + 2");
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

            auto compact = binary(Operator::UnitScale, numberNode(2.0), std::make_unique<UnitExpression>(nullptr, Units::Quantity(1.0, Units::Unit::Length), "mm"));
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
            const ExpressionPtr node     = ExpressionParser::parse(nullptr, text);
            const auto         *variable = dynamic_cast<const VariableExpression *>(node.get());
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
            for (const std::string text: {"Length", "Box.Length", "<<Part>>.Box.Length", "<<Sheet#A1>>"})
            {
                const ExpressionPtr node    = ExpressionParser::parse(nullptr, text);
                const std::string   written = node->toString();
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

        /**
         * @brief 钉住：跨文档写法的两段都不允许为空
         * @details 模糊测试在 <<#>> 上找到的缺陷：解析器照单收下空的文档名与空的目标，得到一条
         *          路径全空的引用，而它的持久文本是空串——宿主既拿不回原文，也重解析不回来。
         *          现在按「写法本身不成立」处置：走普通解析错通道并给出列号；含 '#' 的文本仍要
         *          写成转义形式，那条路不受影响。
         */
        TEST(ExpressionNodes, DocumentReferenceRequiresBothSides)
        {
            for (const std::string text: {"<<#>>", "<<a#>>", "<<#A1>>"})
            {
                auto parsed = ExpressionParser::tryParse(nullptr, text);
                ASSERT_FALSE(parsed.has_value()) << text << " 不该被当成引用收下";
                EXPECT_FALSE(parsed.error().message.empty()) << text;
                EXPECT_TRUE(parsed.error().column.has_value()) << text;
                EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, text)), Base::ParserError) << text;
            }

            // 两段都有内容时照旧可用，文本能原样解析回来
            const ExpressionPtr cross = ExpressionParser::parse(nullptr, "<<Sheet#A1>>");
            ASSERT_NE(cross, nullptr);
            EXPECT_EQ(cross->toString(), "<<Sheet#A1>>");
            EXPECT_EQ(referenceOf("<<Sheet#A1>>"), referenceOf(cross->toString()));

            // 含 '#' 的文本走转义写法：仍是文本，不会被误判成引用
            const ExpressionPtr text = ExpressionParser::parse(nullptr, "<<a\\#b>>");
            ASSERT_NE(text, nullptr);
            EXPECT_EQ(text->toString(), "<<a\\#b>>");
        }

        /**
         * @brief 钉住：引用路径上的名字段不接受空文本记号
         * @details 模糊测试的长跑找到的第二种形状——`<<#>>` 修好之后它换了条路：`.<<>>`、
         *          `Box.<<>>` 这类「点后面跟空文本」会得到一条属性名为空的引用，
         *          其持久文本要么是空串，要么把路径少打一段（`Box.<<>>` 打成 `Box.`）。
         *          判据按名字段必须有内容统一收口，非空的引号写法不受影响。
         */
        TEST(ExpressionNodes, ReferenceNameSegmentsRejectEmptyText)
        {
            for (const std::string text: {".<<>>", "Box.<<>>", "<<D>>.<<>>.x", "<<D>>.Obj.<<>>"})
            {
                auto parsed = ExpressionParser::tryParse(nullptr, text);
                ASSERT_FALSE(parsed.has_value()) << text << " 不该收下空名字段";
                EXPECT_FALSE(parsed.error().message.empty()) << text;
                EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, text)), Base::ParserError) << text;
            }

            // 空文本作为**实参**仍然合法：收口只针对引用路径上的名字段
            const ExpressionPtr argument = ExpressionParser::parse(nullptr, "len(<<>>)");
            ASSERT_NE(argument, nullptr);
            EXPECT_EQ(argument->toString(), "len(<<>>)");

            // 非空的引号写法不受影响：属性名整段交给宿主
            const ExpressionPtr quoted = ExpressionParser::parse(nullptr, "Box.<<a.b>>");
            ASSERT_NE(quoted, nullptr);
            const std::vector<VariableReference> paths = quoted->collectReferences();
            ASSERT_EQ(paths.size(), 1U);
            EXPECT_EQ(paths.front().objectName, "Box");
            EXPECT_EQ(paths.front().propertyName, "a.b");
        }

        /**
         * @brief 钉住：任何属性名经持久文本都能带回同一条路径
         * @details 属性名允许是宿主登记的任何字符串——包括带点的 "a.b"，以及与单位符号、常量、
         *          关键字同名的 "mm"、"pi"、"True"。此前 pathText() 一律裸写，于是
         *          `Box.<<a.b>>` 打成 `Box.a.b`，重解析得到的是「属性 a 的 b 分量」；
         *          `Box.<<mm>>` 打成 `Box.mm`，重解析干脆失败。静默换路径比报错危险，
         *          因此这些名字现在都改写成 <<...>> 形式。裸写法仍然保留给确认可裸写的名字，
         *          常见形态的文本不变（"Box.Length" 仍是 "Box.Length"）。
         */
        TEST(ExpressionNodes, NameSegmentsSurviveThePersistentText)
        {
            for (const std::string name: {"Length", "a.b", "mm", "pi", "e", "True", "False", "x y", "sqrt", "A1", "$A$1", "x@y", "π", "µm", "1a"})
            {
                const std::string input = "Box.<<" + name + ">>";
                auto              first = ExpressionParser::tryParse(nullptr, input);
                ASSERT_TRUE(first.has_value() && *first != nullptr) << input << " 这种写法本来就该被接受";
                const std::vector<VariableReference> original = (*first)->collectReferences();
                ASSERT_EQ(original.size(), 1U) << input;

                const std::string written = (*first)->toString(true);
                auto              again   = ExpressionParser::tryParse(nullptr, written);
                ASSERT_TRUE(again.has_value() && *again != nullptr) << input << " 的持久文本 [" << written << "] 解析不回来";
                const std::vector<VariableReference> roundTripped = (*again)->collectReferences();
                ASSERT_EQ(roundTripped.size(), 1U) << input << " 的持久文本 [" << written << "]";
                EXPECT_EQ(roundTripped.front(), original.front()) << input << " 打成 [" << written << "] 后换了路径";
            }

            // 常见形态不变：可裸写的名字不额外加引号
            EXPECT_EQ(ExpressionParser::parse(nullptr, "Box.Length")->toString(true), "Box.Length");
            EXPECT_EQ(ExpressionParser::parse(nullptr, "Box.A1")->toString(true), "Box.A1");
            EXPECT_EQ(ExpressionParser::parse(nullptr, "<<Sheet#A1>>")->toString(true), "<<Sheet#A1>>");
            EXPECT_EQ(ExpressionParser::parse(nullptr, "<<Doc>>.Box.Length")->toString(true), "<<Doc>>.Box.Length");
        }

        /**
         * @brief 钉住：持久文本再写一次还是同一串（文本是自身不动点）
         * @details 上一条钉住的是「引用路径」逐格相同；这一条覆盖分量与整棵树的写法：
         *          `Box.Length.<<A>>` 的名字分量曾裸写成 `.A`，而单独一个 A 会被词法器读成安培
         *          单位，于是文本再解析就报「需要分量名」。判据取最省事的形态：解析→写文本→
         *          再解析→再写文本，两次文本必须相同、中间不得解析失败，且重解析出的树要
         *          与原树同形。圈子里只放名字段那类（`Box.<<a.b>>`、分量 `.A`）：单位并写与 `%`
         *          混用的文本还有歧义——`5%m m`（树是 (5 % m) * m）与 `5 % m * m`（树是
         *          5 % (m * m)）打出同一个文本，成因已定位、修法待选，见 CHANGELOG「已知边界」。
         */
        TEST(ExpressionNodes, PersistentTextIsAFixpoint)
        {
            for (const std::string input: {"Box.Length.<<A>>", "Box.Length.<<mm>>", "Box.Length.<<True>>", "Box.<<a.b>>.<<pi>>", "Ф.<<A>>.<<A>>", "(vector(1; 2; 3))[0]", "sum(A1:A10)", "sqrt(16) + abs(-7)", "1 ? 2 : 3 + 4",
                                           "Box.<<x y>>[1]", "<<Doc>>.Box.Length.<<A>>"})
            {
                auto first = ExpressionParser::tryParse(nullptr, input);
                ASSERT_TRUE(first.has_value() && *first != nullptr) << input;
                const std::string once  = (*first)->toString(true);
                auto              again = ExpressionParser::tryParse(nullptr, once);
                ASSERT_TRUE(again.has_value() && *again != nullptr) << input << " 的持久文本 [" << once << "] 解析不回来";
                EXPECT_EQ((*again)->toString(true), once) << input << " 的持久文本 [" << once << "] 不稳定";
                EXPECT_TRUE((*again)->isSame(**first)) << input << " 的持久文本 [" << once << "] 解析成了另一棵树";
            }

            // 名字段的引号规则不改裸写形态：能裸写的仍然裸写
            EXPECT_EQ(ExpressionParser::parse(nullptr, "Box.Length")->toString(true), "Box.Length");
            // 单位并写与 % 混用的文本歧义修好后恢复这一条（届时模糊判据里那两条一起放开）：
            // EXPECT_EQ(ExpressionParser::parse(nullptr, "5%m m")->toString(true), "(5 % m) * m");
        }

        /**
         * @brief 钉住：分量的拷贝构造与拷贝赋值都深拷贝子表达式，自赋值不踩内存
         */
        TEST(ExpressionNodes, ComponentsCopyTheirSubExpressions)
        {
            auto                        range = Expression::Component::rangeComponent(numberNode(1.0), numberNode(3.0), numberNode(2.0));
            const Expression::Component copied{range};
            Expression::Component       assigned;
            assigned = range;

            EXPECT_TRUE(range.isSame(copied));
            EXPECT_TRUE(range.isSame(assigned));
            // 深拷贝：子表达式各一份，宿主改一侧不会影响另一侧
            ASSERT_NE(range.index, nullptr);
            EXPECT_NE(range.index.get(), copied.index.get());
            EXPECT_NE(range.index.get(), assigned.index.get());
            EXPECT_NE(range.endIndex.get(), copied.endIndex.get());
            EXPECT_NE(range.step.get(), copied.step.get());

            // 自赋值要先接住再释放，否则子表达式会被自己清掉
            Expression::Component *self = &assigned;
            assigned                    = *self;
            EXPECT_TRUE(assigned.isSame(range));
            EXPECT_NE(assigned.index, nullptr);
        }

        /**
         * @brief 钉住：区间节点保留首尾文本，取出的区间已按左上到右下整理
         */
        TEST(ExpressionNodes, RangeNodeKeepsEndpointText)
        {
            const auto range = std::make_unique<RangeExpression>(nullptr, "A1", "B2");
            EXPECT_EQ(range->nodeName(), "Range");
            EXPECT_EQ(range->getBegin(), "A1");
            EXPECT_EQ(range->getEnd(), "B2");
            EXPECT_EQ(range->toString(), "A1:B2");
            EXPECT_EQ(range->getRange().rangeText(), "A1:B2");
            EXPECT_EQ(range->getRange().size(), 4);

            // 反向拖选：文本按原写法回写，取出的区间整理成正向
            const auto reversed = std::make_unique<RangeExpression>(nullptr, "B2", "A1");
            EXPECT_EQ(reversed->toString(), "B2:A1");
            EXPECT_EQ(reversed->getRange().rangeText(), "A1:B2");
        }

        /// 造一段「1 + 1 + …」的文本，terms 是项数
        std::string additiveChainText(const std::size_t terms)
        {
            std::string text = "1";
            for (std::size_t term = 1; term < terms; ++term)
            {
                text += " + 1";
            }
            return text;
        }

        /// 造一段「sum(1, 1, …)」的文本：项数相同，但实参是兄弟节点，树只有一层深
        std::string sumOfOnesText(const std::size_t terms)
        {
            std::string text = "sum(";
            for (std::size_t term = 0; term < terms; ++term)
            {
                if (term > 0)
                {
                    text += ", ";
                }
                text += "1";
            }
            text += ')';
            return text;
        }

        /// 手工接一条左结合加法链，terms 是项数；深度记账不能只认解析器
        ExpressionPtr additiveChainNodes(const std::size_t terms)
        {
            ExpressionPtr chain = numberNode(1.0);
            for (std::size_t term = 1; term < terms; ++term)
            {
                chain = binary(Operator::Add, std::move(chain), numberNode(1.0));
            }
            return chain;
        }

        /// 造一个只关心结构的变量引用节点，用例不接解析器
        ExpressionPtr makeListReference()
        {
            return std::make_unique<VariableExpression>(nullptr, VariableExpression::Reference{.documentName = "", .objectName = "", .propertyName = "List"});
        }

        /**
         * @brief 钉住：深度取「最深的一支再加一层」，兄弟节点不叠层
         * @details 括号不产生节点，实参与区间端点是兄弟，因此宽树再宽也只有一层；
         *          把这两类混为一谈的话，深度上限就会拒掉根本不危险的表达式。
         */
        TEST(ExpressionNodes, AstDepthCountsTheDeepestBranchOnly)
        {
            EXPECT_EQ(ExpressionParser::parse(nullptr, "1")->astDepth(), 0);
            EXPECT_EQ(ExpressionParser::parse(nullptr, "(((1)))")->astDepth(), 0);
            EXPECT_EQ(ExpressionParser::parse(nullptr, "1 + 2")->astDepth(), 1);
            EXPECT_EQ(ExpressionParser::parse(nullptr, "1 + 2 + 3")->astDepth(), 2);
            // 乘法挂在加法的右支上，两层各占一层；同级但不同支的才不叠层
            EXPECT_EQ(ExpressionParser::parse(nullptr, "1 + 2 * 3")->astDepth(), 2);
            EXPECT_EQ(ExpressionParser::parse(nullptr, "1 * 2 + 3 * 4")->astDepth(), 2);
            EXPECT_EQ(ExpressionParser::parse(nullptr, "sum(1, 2, 3)")->astDepth(), 1);
            // 分量里的下标也算一支
            EXPECT_EQ(ExpressionParser::parse(nullptr, "List[1 + 2].Name")->astDepth(), 2);
            // 深拷贝与原树同形，深度照搬；分量里的表达式只有基类记着，副本必须一起带上
            EXPECT_EQ(ExpressionParser::parse(nullptr, "1 + 2 + 3")->copy()->astDepth(), 2);
            EXPECT_EQ(ExpressionParser::parse(nullptr, "List[1 + 2].Name")->copy()->astDepth(), 2);
        }

        /**
         * @brief 钉住：顶到上限的加法链照常求值，再长一项就在构造期报错
         */
        TEST(ExpressionNodes, OperatorChainIsCutOffAtTheDepthLimit)
        {
            const std::size_t limit = Expression::maxAstDepth;

            // 上限项数 = limit-1 层，求值与文本往返都必须成立
            EXPECT_DOUBLE_EQ(evaluateText(additiveChainText(limit)), static_cast<double>(limit));
            const auto edge = ExpressionParser::parse(nullptr, additiveChainText(limit + 1));
            ASSERT_NE(edge, nullptr);
            EXPECT_EQ(edge->astDepth(), limit);

            // 再多一项就过线，两条通道给出同一份可行动的文案
            const std::string overLimit = additiveChainText(limit + 2);
            const auto        failed    = ExpressionParser::tryParse(nullptr, overLimit);
            ASSERT_FALSE(failed.has_value());
            EXPECT_NE(failed.error().message.find("超过 " + std::to_string(limit) + " 层上限"), std::string::npos);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, overLimit)), Base::ParserError);
        }

        /**
         * @brief 钉住：一长串同类运算在构造期就被拒绝，进程不会走到不可捕获的栈溢出
         * @details 解析器自身的递归由括号限额管着，但左结合长链是在同一层递归里循环拼出来的，
         *          只有构造期的深度记账拦得住——过去这种输入直接把测试进程打挂。
         */
        TEST(ExpressionNodes, PathologicalChainFailsCleanlyInsteadOfCrashing)
        {
            const std::string huge = additiveChainText(20000);

            const auto failed = ExpressionParser::tryParse(nullptr, huge);
            ASSERT_FALSE(failed.has_value());
            EXPECT_NE(failed.error().message.find("层上限"), std::string::npos);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, huge)), Base::ParserError);

            // 文案给的出路必须真的能用：同样多的项改用聚合实参，取值不变
            EXPECT_DOUBLE_EQ(evaluateText(sumOfOnesText(20000)), 20000.0);
            const auto aggregated = ExpressionParser::parse(nullptr, sumOfOnesText(20000));
            ASSERT_NE(aggregated, nullptr);
            EXPECT_EQ(aggregated->astDepth(), 1);
        }

        /**
         * @brief 钉住：分量里的子表达式同样计入深度，超限时分量不会挂上半截
         */
        TEST(ExpressionNodes, ComponentExpressionsCountTowardDepth)
        {
            // 下标本身 limit 层深（构造得出来），加上引用节点这一层就过线
            const ExpressionPtr deepestIndex = additiveChainNodes(Expression::maxAstDepth + 1);
            EXPECT_EQ(deepestIndex->astDepth(), Expression::maxAstDepth);

            auto overLimit = makeListReference();
            EXPECT_THROW(overLimit->addComponent(Expression::Component::arrayIndex(additiveChainNodes(Expression::maxAstDepth + 1))), Base::ParserError);
            EXPECT_FALSE(overLimit->hasComponent());

            // 少一层就放得下，且把本节点顶到上限
            auto inside = makeListReference();
            inside->addComponent(Expression::Component::arrayIndex(additiveChainNodes(Expression::maxAstDepth)));
            ASSERT_TRUE(inside->hasComponent());
            EXPECT_EQ(inside->astDepth(), Expression::maxAstDepth);
        }

        /**
         * @brief 钉住：宿主手工拼的树也受同一上限管，换操作数同样记账
         */
        TEST(ExpressionNodes, HandBuiltChainIsAlsoCutOff)
        {
            EXPECT_NO_THROW(static_cast<void>(additiveChainNodes(Expression::maxAstDepth + 1)));
            EXPECT_THROW(static_cast<void>(additiveChainNodes(Expression::maxAstDepth + 2)), Base::ParserError);

            auto sum = std::make_unique<OperatorExpression>(nullptr, numberNode(1.0), Operator::Add, numberNode(2.0));
            EXPECT_THROW(sum->setRight(additiveChainNodes(Expression::maxAstDepth + 1)), Base::ParserError);
            EXPECT_THROW(sum->setLeft(additiveChainNodes(Expression::maxAstDepth + 1)), Base::ParserError);
            // 抛出时操作数保持原样，本节点深度不变
            EXPECT_EQ(sum->toString(), "1 + 2");
            EXPECT_EQ(sum->astDepth(), 1);
        }

    } // namespace
} // namespace ExpressionEngine::Expression
