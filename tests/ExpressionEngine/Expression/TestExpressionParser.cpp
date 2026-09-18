// 本文件钉住表达式解析器的优先级、歧义处理与拒绝面。

#include <gtest/gtest.h>

#include <string>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Expression/ExpressionParser.h>

namespace ExpressionEngine::Expression
{
    namespace
    {

        /// 解析并求值，返回数量结果；异常与非数量结果都由用例报告，便于定位是哪个表达式出问题
        Units::Quantity quantityOf(const std::string &text)
        {
            try
            {
                const ExpressionPtr expression    = ExpressionParser::parse(nullptr, text);
                const Value         value         = expression->evaluate();
                const auto         *quantityValue = std::get_if<Units::Quantity>(&value);
                if (quantityValue == nullptr)
                {
                    ADD_FAILURE() << "表达式 " << text << " 的结果不是数量";
                    return {};
                }
                return *quantityValue;
            } catch (const Base::Exception &error)
            {
                ADD_FAILURE() << "表达式 " << text << " 抛出 " << error.message();
                return {};
            }
        }

        /**
         * @brief 钉住：除号后紧跟单位时先算数值除法，「1/2 mm」是 0.5 毫米
         */
        TEST(ExpressionParserTest, UnitAttachesToWholeNumericExpression)
        {
            const Units::Quantity halfMillimetre = quantityOf("1/2 mm");
            EXPECT_EQ(halfMillimetre.getUnit(), Units::Unit::Length);
            EXPECT_DOUBLE_EQ(halfMillimetre.getValue(), 0.5);

            // 乘方先于单位附着
            const Units::Quantity squared = quantityOf("2^2 mm");
            EXPECT_EQ(squared.getUnit(), Units::Unit::Length);
            EXPECT_DOUBLE_EQ(squared.getValue(), 4.0);
        }

        /**
         * @brief 钉住：单位幂次、复合单位与倒数量纲
         */
        TEST(ExpressionParserTest, UnitPowersAndProducts)
        {
            const Units::Quantity area = quantityOf("3 mm^2");
            EXPECT_EQ(area.getUnit(), Units::Unit::Area);
            EXPECT_DOUBLE_EQ(area.getValue(), 3.0);

            const Units::Quantity inverseLength = quantityOf("1/mm");
            EXPECT_EQ(inverseLength.getUnit(), Units::Unit::InverseLength);
            EXPECT_DOUBLE_EQ(inverseLength.getValue(), 1.0);

            const Units::Quantity velocity = quantityOf("2 m/s");
            EXPECT_EQ(velocity.getUnit(), Units::Unit::Velocity);
        }

        /**
         * @brief 钉住：一元负号比乘方结合更紧，-2^2 是 (-2)^2 而不是 -(2^2)
         */
        TEST(ExpressionParserTest, UnaryMinusBindsTighterThanPower)
        {
            EXPECT_DOUBLE_EQ(quantityOf("-2^2").getValue(), 4.0);
            EXPECT_DOUBLE_EQ(quantityOf("2^-3").getValue(), 0.125);
            EXPECT_DOUBLE_EQ(quantityOf("2^3^2").getValue(), 512.0);
        }

        /**
         * @brief 钉住：算术优先级与括号
         */
        TEST(ExpressionParserTest, ArithmeticPrecedence)
        {
            EXPECT_DOUBLE_EQ(quantityOf("1 + 2 * 3").getValue(), 7.0);
            EXPECT_DOUBLE_EQ(quantityOf("(1 + 2) * 3").getValue(), 9.0);
            EXPECT_DOUBLE_EQ(quantityOf("7 % 4").getValue(), 3.0);
        }

        /**
         * @brief 钉住：英制两段写法 5' 6" 按「5 英尺 + 6 英寸」求和
         */
        TEST(ExpressionParserTest, ImperialTwoSegmentSum)
        {
            const Units::Quantity length = quantityOf("5' 6\"");
            EXPECT_EQ(length.getUnit(), Units::Unit::Length);
            EXPECT_DOUBLE_EQ(length.getValue(), 1524.0 + 152.4);
        }

        /**
         * @brief 钉住：函数调用、逗号与分号分隔的实参、以及嵌套调用
         */
        TEST(ExpressionParserTest, FunctionCalls)
        {
            // 三角函数按角度取值（与 FreeCAD 一致），因此 sin(90) 才是 1
            SCOPED_TRACE("sin(90)");
            EXPECT_NEAR(quantityOf("sin(90)").getValue(), 1.0, 1e-12);

            // 分号与逗号同义，max 取最大实参
            SCOPED_TRACE("max(1; 5, 3)");
            EXPECT_DOUBLE_EQ(quantityOf("max(1; 5, 3)").getValue(), 5.0);

            SCOPED_TRACE("abs(-3) + sqrt(4)");
            EXPECT_DOUBLE_EQ(quantityOf("abs(-3) + sqrt(4)").getValue(), 5.0);
        }

        /**
         * @brief 钉住：布尔常量与三元条件
         */
        TEST(ExpressionParserTest, ConstantsAndTernary)
        {
            // 布尔常量求值为布尔值，不是数量
            const Value trueValue = ExpressionParser::parse(nullptr, "True")->evaluate();
            const auto *trueFlag  = std::get_if<bool>(&trueValue);
            ASSERT_NE(trueFlag, nullptr);
            EXPECT_TRUE(*trueFlag);

            const Value falseValue = ExpressionParser::parse(nullptr, "false")->evaluate();
            const auto *falseFlag  = std::get_if<bool>(&falseValue);
            ASSERT_NE(falseFlag, nullptr);
            EXPECT_FALSE(*falseFlag);

            // 数值常量走数量通道
            EXPECT_NEAR(quantityOf("pi").getValue(), 3.14159265358979, 1e-12);

            EXPECT_DOUBLE_EQ(quantityOf("1 > 0 ? 2 : 3").getValue(), 2.0);
            EXPECT_DOUBLE_EQ(quantityOf("1 < 0 ? 2 : 3").getValue(), 3.0);
        }

        /**
         * @brief 钉住：引用路径能解析出结构与分量，不需要解析器也能构造出节点
         */
        TEST(ExpressionParserTest, ReferencePathsParseWithoutResolver)
        {
            // 断言语义而不是文本：文本化格式由 AST 决定，这里只关心引用被解析成了什么
            const auto referenceOf = [](const std::string &text)
            {
                const ExpressionPtr expression = ExpressionParser::parse(nullptr, text);
                const auto         *variable   = dynamic_cast<const VariableExpression *>(expression.get());
                EXPECT_NE(variable, nullptr) << text;
                if (variable == nullptr)
                {
                    return VariableExpression::Reference{};
                }
                return variable->getReference();
            };

            const auto objectPath = referenceOf("Box.Length");
            EXPECT_EQ(objectPath.objectName, "Box");
            EXPECT_EQ(objectPath.propertyName, "Length");

            const auto currentObject = referenceOf(".Length");
            EXPECT_TRUE(currentObject.objectName.empty());
            EXPECT_EQ(currentObject.propertyName, "Length");

            const auto documentPath = referenceOf("<<Part>>.Box.Length");
            EXPECT_EQ(documentPath.documentName, "Part");
            EXPECT_EQ(documentPath.objectName, "Box");
            EXPECT_EQ(documentPath.propertyName, "Length");

            // 索引与路径分量挂在同一条引用上
            const ExpressionPtr indexed         = ExpressionParser::parse(nullptr, "Box.Cells[1:5]");
            const auto         *indexedVariable = dynamic_cast<const VariableExpression *>(indexed.get());
            ASSERT_NE(indexedVariable, nullptr);
            EXPECT_EQ(indexedVariable->components().size(), 1U);
            EXPECT_EQ(indexedVariable->components().front().kind, Expression::ComponentKind::Range);
        }

        /**
         * @brief 钉住：文本、注释式的空白处理与往返一致性
         */
        TEST(ExpressionParserTest, RoundTripThroughText)
        {
            for (const std::string &text: {"1 + 2*3", "(1+2)*3", "2 mm", "1/2 mm", "sin(0.5)"})
            {
                SCOPED_TRACE(text);
                const ExpressionPtr first   = ExpressionParser::parse(nullptr, text);
                const std::string   printed = first->toString();
                const ExpressionPtr second  = ExpressionParser::parse(nullptr, printed);
                // 文本往返后求值结果必须一致
                EXPECT_EQ(first->evaluate().index(), second->evaluate().index()) << printed;
            }
        }

        /**
         * @brief 钉住拒绝面：空文本、多余记号、缺括号、缺实参、非法单位都要报错
         */
        TEST(ExpressionParserTest, RejectsMalformedExpressions)
        {
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "")), Base::ParserError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "1 2")), Base::ParserError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "(1 + 2")), Base::ParserError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "max(1,)")), Base::ParserError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "1 +")), Base::ParserError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "[1:]")), Base::ParserError);
            // 实参个数错误由 AST 构造期报出，类型是引擎错误而不是语法错误
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "sin()")), Base::Exception);
        }

        /**
         * @brief 钉住：量纲不匹配在求值期报错，而不是给出错误结果
         */
        TEST(ExpressionParserTest, UnitMismatchIsRejectedAtEvaluation)
        {
            const ExpressionPtr expression = ExpressionParser::parse(nullptr, "2 + 3 mm");
            EXPECT_THROW(static_cast<void>(expression->evaluate()), Base::UnitsMismatchError);
        }

    } // namespace
} // namespace ExpressionEngine::Expression
