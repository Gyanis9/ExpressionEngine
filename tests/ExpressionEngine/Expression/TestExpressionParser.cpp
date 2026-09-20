// 本文件钉住表达式解析器的优先级、歧义处理与拒绝面。

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <variant>

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
                const auto *        quantityValue = std::get_if<Units::Quantity>(&value);
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
         * @brief 钉住：单位乘除只在后面确实跟着单位时才算单位乘除
         */
        TEST(ExpressionParserTest, UnitsAndOperandsShareStarAndSlash)
        {
            // 带单位的两个量相乘不该逼用户加括号
            const Units::Quantity area = quantityOf("3 mm * 4 mm");
            EXPECT_EQ(area.getUnit(), Units::Unit::Area);
            EXPECT_DOUBLE_EQ(area.getValue(), 12.0);

            const Units::Quantity velocity = quantityOf("(60 mm) / (4 s)");
            EXPECT_EQ(velocity.getUnit(), Units::Unit::Velocity);
            EXPECT_DOUBLE_EQ(velocity.getValue(), 15.0);

            // 单位后置与 *、/ 同级：`60 mm / 4 s` 里的 s 贴在整段商上，得到长度×时间。
            // 这与「1/2 mm 先算除法」是同一条约定，想要速度必须像上面那样写明两侧
            const Units::Quantity carried = quantityOf("60 mm / 4 s");
            EXPECT_EQ(carried.getUnit(), Units::Unit::Length * Units::Unit::TimeSpan);
            EXPECT_DOUBLE_EQ(carried.getValue(), 15.0);

            // 单位与单位之间仍然连写：2 m/s 是速度，1/mm 是倒数长度
            EXPECT_EQ(quantityOf("2 m/s").getUnit(), Units::Unit::Velocity);
            EXPECT_EQ(quantityOf("1/mm").getUnit(), Units::Unit::InverseLength);
            EXPECT_EQ(quantityOf("3 mm^2").getUnit(), Units::Unit::Area);

            // 单位后置之后接纯数
            EXPECT_DOUBLE_EQ(quantityOf("2 mm * 3").getValue(), 6.0);
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
            // 三角函数按角度取值，因此 sin(90) 才是 1
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
                const auto *        variable   = dynamic_cast<const VariableExpression *>(expression.get());
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
            const auto *        indexedVariable = dynamic_cast<const VariableExpression *>(indexed.get());
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
         * @brief 钉住：文本取值的持久化文本能原样解析回同一个值
         */
        TEST(ExpressionParserTest, TextValueRoundTripsThroughPersistentText)
        {
            for (const std::string &text: {"", "abc", "a'b", "a\"b", "a>b", "a>>b", "a#b", "a\\b", "<<x>>", "中文 1.5", "a\nb"})
            {
                SCOPED_TRACE(text);
                // 从 AST 侧出发：文本节点的持久化写法必须能被词法器读回同一个值
                const StringExpression node(nullptr, text);
                const std::string      printed = node.toString(true, true);

                const ExpressionPtr reparsed = ExpressionParser::parse(nullptr, printed);
                const Value         value    = reparsed->evaluate();
                const auto *        parsed   = std::get_if<std::string>(&value);
                ASSERT_NE(parsed, nullptr) << "持久化文本 " << printed << " 没解析回文本取值";
                EXPECT_EQ(*parsed, text);
            }
        }

        /**
         * @brief 钉住：映射键分量的持久化文本同样能解析回同一结构
         */
        TEST(ExpressionParserTest, MapKeyComponentRoundTrips)
        {
            auto keyed = std::make_unique<VariableExpression>(nullptr, VariableReference{.objectName = "Box", .propertyName = "Cells"});
            keyed->addComponent(Expression::Component::mapKey("Length"));

            const std::string   printed  = keyed->toString(true, true);
            const ExpressionPtr reparsed = ExpressionParser::parse(nullptr, printed);
            EXPECT_TRUE(keyed->isSame(*reparsed)) << printed;
        }

        /**
         * @brief 钉住：量纲不匹配在求值期报错，而不是给出错误结果
         */
        TEST(ExpressionParserTest, UnitMismatchIsRejectedAtEvaluation)
        {
            const ExpressionPtr expression = ExpressionParser::parse(nullptr, "2 + 3 mm");
            EXPECT_THROW(static_cast<void>(expression->evaluate()), Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：非异常通道——非法文本以 ParseFailure 返回，且文案与异常通道逐字一致
         */
        TEST(ExpressionParserTest, TryParseReportsFailureAsValue)
        {
            const auto parsed = ExpressionParser::tryParse(nullptr, "Box.Length * 2");
            ASSERT_TRUE(parsed.has_value());
            EXPECT_NE(*parsed, nullptr);

            const auto failed = ExpressionParser::tryParse(nullptr, "1 +");
            ASSERT_FALSE(failed.has_value());
            EXPECT_FALSE(failed.error().message.empty());

            // 两个通道必须给出同一份文案，否则调用方换通道时行为会变
            try
            {
                static_cast<void>(ExpressionParser::parse(nullptr, "1 +"));
                FAIL() << "非法文本应当抛错";
            } catch (const Base::ParserError &error)
            {
                EXPECT_EQ(failed.error().message, error.message());
            }
        }

        /**
         * @brief 钉住：list() 造出序列取值，可下标、可区间，也能被聚合函数摊平
         */
        TEST(ExpressionParserTest, ListValuesSupportIndexSliceAndAggregates)
        {
            const Value listValue = ExpressionParser::parse(nullptr, "list(1 mm; 2 mm; <<abc>>)")->evaluate();
            const auto *sequence  = std::get_if<ValueSequence>(&listValue);
            ASSERT_NE(sequence, nullptr);
            ASSERT_EQ(sequence->size(), 3U);

            // 下标按值语义取元素，负下标从末尾计数
            EXPECT_DOUBLE_EQ(quantityOf("list(1 mm; 2 mm; <<abc>>)[1]").getValue(), 2.0);
            EXPECT_EQ(std::get<std::string>(ExpressionParser::parse(nullptr, "list(1 mm; 2 mm; <<abc>>)[2]")->evaluate()), "abc");
            EXPECT_DOUBLE_EQ(quantityOf("list(1; 2; 3)[-1]").getValue(), 3.0);

            // 区间分量得到一个序列，可直接喂给聚合函数；含末端
            EXPECT_DOUBLE_EQ(quantityOf("sum(list(1; 2; 3)[0:2])").getValue(), 6.0);
            EXPECT_DOUBLE_EQ(quantityOf("sum(list(1; 2; 3)[0:1])").getValue(), 3.0);

            // 序列实参被摊平，与逐个给实参同义
            EXPECT_DOUBLE_EQ(quantityOf("sum(list(1; 2); 3)").getValue(), 6.0);
            EXPECT_DOUBLE_EQ(quantityOf("average(list(2; 4))").getValue(), 3.0);
            EXPECT_DOUBLE_EQ(quantityOf("count(list(1; 2; 3))").getValue(), 3.0);

            // 空序列是合法取值；对空区间取聚合由聚合函数自己判定
            EXPECT_TRUE(std::get<ValueSequence>(ExpressionParser::parse(nullptr, "list()")->evaluate()).empty());

            // 内置几何函数的取值同样可按下标取分量
            EXPECT_DOUBLE_EQ(toDouble(ExpressionParser::parse(nullptr, "vector(1; 2; 3)[0]")->evaluate(), "向量下标"), 1.0);

            // 拒绝面：序列不能直接当数量参与算术
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "list(1; 2) + 1")->evaluate()), Base::TypeError);
        }

        /**
         * @brief 钉住：list 的持久化文本能原样解析回同一棵树
         */
        TEST(ExpressionParserTest, ListTextRoundTrips)
        {
            for (const std::string &text: {"list()", "list(1; 2 mm)", "list(1; list(2; 3))", "sum(list(1; 2)[0:1])"})
            {
                SCOPED_TRACE(text);
                const ExpressionPtr first   = ExpressionParser::parse(nullptr, text);
                const std::string   printed = first->toString(true, true);
                const ExpressionPtr second  = ExpressionParser::parse(nullptr, printed);
                EXPECT_TRUE(first->isSame(*second)) << printed;
            }
        }

        /**
         * @brief 钉住：tuple 是 list 的别名，且按书写原样回写
         */
        TEST(ExpressionParserTest, TupleIsAnAliasOfList)
        {
            const ExpressionPtr tuple = ExpressionParser::parse(nullptr, "tuple(1; 2 mm)");
            ASSERT_NE(tuple, nullptr);
            const Value value = tuple->evaluate();
            EXPECT_EQ(std::get<ValueSequence>(value).size(), 2U);

            // 回写保持使用者写的名字，规范名只在函数表查询侧出现；单位后置按显式乘法排版
            const std::string   printed  = tuple->toString(true, true);
            const ExpressionPtr reparsed = ExpressionParser::parse(nullptr, printed);
            EXPECT_TRUE(tuple->isSame(*reparsed)) << printed;
            EXPECT_EQ(printed, "tuple(1; 2 * mm)");
        }

        /**
         * @brief 钉住：折成常量的带单位数量，其持久化文本仍带着量纲
         */
        TEST(ExpressionParserTest, FoldedConstantKeepsItsUnit)
        {
            // 单量纲、复合量纲与角度量各一条：折成常量后文本必须带着量纲
            for (const std::string &text: {"2 m + 3 m", "(2 m) / (4 s)", "(3 mm) * (4 mm)", "90 deg + 90 deg"})
            {
                SCOPED_TRACE(text);
                const ExpressionPtr folded  = ExpressionParser::parse(nullptr, text)->simplify();
                const std::string   printed = folded->toString(true);

                const Units::Quantity original = quantityOf(text);
                const Units::Quantity reparsed = quantityOf(printed);
                EXPECT_NE(printed.find(original.getUnit().getString()), std::string::npos) << printed;
                EXPECT_EQ(reparsed.getUnit(), original.getUnit()) << printed;
                EXPECT_DOUBLE_EQ(reparsed.getValue(), original.getValue()) << printed;
            }
        }

        /**
         * @brief 钉住：几何与序列取值的持久化文本写成可重新解析的构造调用
         */
        TEST(ExpressionParserTest, GeometryAndListTextRoundTrip)
        {
            for (const std::string &text: {"vector(1; 2; 3)",
                                           "matrix(1; 0; 0; 0; 0; 1; 0; 0; 0; 0; 1; 0; 0; 0; 0; 1)",
                                           "rotation(0; 0; 0.5)",
                                           "placement(vector(1; 2; 3); rotation(0; 0; 0.5))",
                                           "list(vector(1; 2; 3); 2 m; <<文本>>)"})
            {
                SCOPED_TRACE(text);
                const ExpressionPtr folded   = ExpressionParser::parse(nullptr, text)->simplify();
                const std::string   printed  = folded->toString(true, true);
                const ExpressionPtr reparsed = ExpressionParser::parse(nullptr, printed);
                EXPECT_TRUE(valuesEqual(folded->evaluate(), reparsed->evaluate())) << printed;
            }
        }

        /// 求值并取文本；非文本结果由用例报告
        std::string textOf(const std::string &text)
        {
            const Value value = ExpressionParser::parse(nullptr, text)->evaluate();
            if (const auto *result = std::get_if<std::string>(&value))
            {
                return *result;
            }
            ADD_FAILURE() << text << " 的结果不是文本，而是" << valueTypeName(value);
            return {};
        }

        /// 求值并取纯数；带量纲或别的类型都由 toDouble 明确报错
        double numberOf(const std::string &text)
        {
            return toDouble(ExpressionParser::parse(nullptr, text)->evaluate(), text);
        }

        /// 求值并取布尔
        bool boolOf(const std::string &text)
        {
            const Value value = ExpressionParser::parse(nullptr, text)->evaluate();
            if (const auto *result = std::get_if<bool>(&value))
            {
                return *result;
            }
            ADD_FAILURE() << text << " 的结果不是布尔，而是" << valueTypeName(value);
            return false;
        }

        /**
         * @brief 钉住：文本函数按字符而非字节计数与切片
         */
        TEST(ExpressionParserTest, TextFunctionsCountAndSliceByCharacter)
        {
            EXPECT_DOUBLE_EQ(numberOf("len(<<abc>>)"), 3.0);
            EXPECT_DOUBLE_EQ(numberOf("len(<<中文 a>>)"), 4.0);

            EXPECT_EQ(textOf("substr(<<abcdef>>; 2)"), "cdef");
            EXPECT_EQ(textOf("substr(<<abcdef>>; 0; 2)"), "ab");
            EXPECT_EQ(textOf("substr(<<中文ab>>; -2)"), "ab");
            // 长度超出剩余字符数时按剩余截断
            EXPECT_EQ(textOf("substr(<<中文abc>>; 2; 99)"), "abc");
            EXPECT_EQ(textOf("substr(<<abc>>; 3)"), "");

            // 起点越界与负长度各自明确报错，不静默给空串
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "substr(<<abc>>; 9)")->evaluate()), Base::IndexError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "substr(<<abc>>; 0; -1)")->evaluate()), Base::ValueError);
        }

        /**
         * @brief 钉住：大小写只映射 ASCII，去空白覆盖四种控制码
         */
        TEST(ExpressionParserTest, TextCaseAndTrimFunctions)
        {
            EXPECT_EQ(textOf("upper(<<aB1中>>)"), "AB1中");
            EXPECT_EQ(textOf("lower(<<aB1中>>)"), "ab1中");
            EXPECT_EQ(textOf("trim(<<  x  >>)"), "x");
            EXPECT_DOUBLE_EQ(numberOf("len(trim(<<  a  b  >>))"), 4.0); // 中间的两个空格保留
            // << >> 里的 \t、\n 由词法器还原成真控制字符，trim 一并去掉
            EXPECT_DOUBLE_EQ(numberOf("len(trim(<<\\t a \\n >>))"), 1.0);
        }

        /**
         * @brief 钉住：查找、替换与拼接
         */
        TEST(ExpressionParserTest, TextSearchReplaceAndConcat)
        {
            EXPECT_TRUE(boolOf("contains(<<abc>>; <<bc>>)"));
            EXPECT_FALSE(boolOf("contains(<<abc>>; <<cb>>)"));
            EXPECT_TRUE(boolOf("contains(<<abc>>; <<>>)"));

            EXPECT_EQ(textOf("replace(<<a-b-c>>; <<->>; <<+>>)"), "a+b+c");
            EXPECT_EQ(textOf("replace(<<abc>>; <<b>>; <<>>)"), "ac");
            // 空模式没有「全部匹配」可言，原样返回而不是逐字符插入
            EXPECT_EQ(textOf("replace(<<abc>>; <<>>; <<x>>)"), "abc");

            EXPECT_EQ(textOf("concat(<<x>>; <<y>>)"), "xy");
            // 数字实参按 str() 的写法参与拼接，不预设排版小数位
            EXPECT_DOUBLE_EQ(numberOf("len(concat(<<x>>; 1))"), numberOf("len(str(1))") + 1.0);
        }

        /**
         * @brief 钉住：文本函数的拒绝面——参数个数在建节点时校验，类型不符不隐式转换
         */
        TEST(ExpressionParserTest, TextFunctionsRejectWrongArguments)
        {
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "len()")), EvaluationError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "substr(<<abc>>)")), EvaluationError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "contains(<<abc>>)")), EvaluationError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "concat()")), EvaluationError);

            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "len(2 mm)")->evaluate()), Base::TypeError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "upper(2)")->evaluate()), Base::TypeError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "contains(<<abc>>; 1)")->evaluate()), Base::TypeError);
        }

    } // namespace
}     // namespace ExpressionEngine::Expression
