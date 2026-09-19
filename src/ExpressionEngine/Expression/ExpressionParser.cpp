#include <ExpressionEngine/Expression/ExpressionParser.h>

#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Expression/ExpressionLexer.h>
#include <ExpressionEngine/Units/QuantityParser.h>

namespace ExpressionEngine::Expression
{

    namespace
    {

        /// 运算符的绑定功率：左结合运算符的右结合功率为左功率加一，幂与三元取相同值实现右结合
        constexpr int ternaryBinding        = 1;
        constexpr int comparisonBinding     = 2;
        constexpr int additiveBinding       = 3;
        constexpr int multiplicativeBinding = 4;
        /// 单位后置与乘除同级：使 "1/2 mm" 归约为 (1/2) mm，同时不会钻进乘除的右操作数
        constexpr int unitPostfixBinding    = 4;
        constexpr int powerBinding          = 6;
        /// 一元正负比乘方结合更紧，因此 -2^2 是 (-2)^2，与 Expression.y 的优先级声明一致
        constexpr int unaryBinding          = 7;

        /// 二元运算符的记号、运算符节点取值与结合功率
        struct BinaryOperatorInfo
        {
            OperatorExpression::Operator operation;
            int                          leftBinding;
            int                          rightBinding;
        };

        /**
         * @brief 取记号对应的二元运算符
         * @param kind 记号类别
         * @return 是二元运算符时返回其信息；否则返回 std::nullopt
         */
        [[nodiscard]] std::optional<BinaryOperatorInfo> binaryOperatorFor(const ExpressionTokenKind kind)
        {
            switch (kind)
            {
                case ExpressionTokenKind::Plus:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::Add, .leftBinding = additiveBinding, .rightBinding = additiveBinding + 1};
                case ExpressionTokenKind::Minus:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::Subtract, .leftBinding = additiveBinding, .rightBinding = additiveBinding + 1};
                case ExpressionTokenKind::Star:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::Multiply, .leftBinding = multiplicativeBinding, .rightBinding = multiplicativeBinding + 1};
                case ExpressionTokenKind::Slash:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::Divide, .leftBinding = multiplicativeBinding, .rightBinding = multiplicativeBinding + 1};
                case ExpressionTokenKind::Percent:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::Modulo, .leftBinding = multiplicativeBinding, .rightBinding = multiplicativeBinding + 1};
                case ExpressionTokenKind::Caret:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::Power, .leftBinding = powerBinding, .rightBinding = powerBinding};
                case ExpressionTokenKind::Equal:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::Equal, .leftBinding = comparisonBinding, .rightBinding = comparisonBinding + 1};
                case ExpressionTokenKind::NotEqual:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::NotEqual, .leftBinding = comparisonBinding, .rightBinding = comparisonBinding + 1};
                case ExpressionTokenKind::Less:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::Less, .leftBinding = comparisonBinding, .rightBinding = comparisonBinding + 1};
                case ExpressionTokenKind::Greater:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::Greater, .leftBinding = comparisonBinding, .rightBinding = comparisonBinding + 1};
                case ExpressionTokenKind::LessEqual:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::LessEqual, .leftBinding = comparisonBinding, .rightBinding = comparisonBinding + 1};
                case ExpressionTokenKind::GreaterEqual:
                    return BinaryOperatorInfo{.operation = OperatorExpression::Operator::GreaterEqual, .leftBinding = comparisonBinding, .rightBinding = comparisonBinding + 1};
                default:
                    return std::nullopt;
            }
        }

        /// 去掉 << >> 定界符；词法已剥离时原样返回（避免把文本自身的引号误当成定界符）
        [[nodiscard]] std::string unquoteReference(const std::string_view raw)
        {
            if (raw.size() >= 4 && raw.starts_with("<<") && raw.ends_with(">>"))
            {
                return std::string{raw.substr(2, raw.size() - 4)};
            }
            return std::string{raw};
        }

        /// 把记号写成报错文案里的定位前缀
        [[nodiscard]] std::string locationOf(const ExpressionToken &token)
        {
            return std::format("表达式第 {} 列", token.column);
        }

        /**
         * @brief 表达式语法分析器
         * @details 优先级自低到高：三元、比较、加减、乘除取余、单位后置、乘方、一元正负、原子。
         *          单位后置只在这一层及以上附着，因此 "1/2 mm" 得到 0.5 mm、"2^2 mm" 得到 4 mm。
         */
        class ExpressionParserImplementation
        {
        public:
            ExpressionParserImplementation(IObjectResolver *resolver, const std::string_view text) :
                m_resolver(resolver), m_lexer(text)
            {
                advance();
                advance();
            }

            [[nodiscard]] ExpressionPtr parseDocument();

        private:
            void advance();

            [[nodiscard]] bool startsUnit() const;

            void expect(ExpressionTokenKind kind, std::string_view description);

            [[nodiscard]] std::string takeIdentifierLike(std::string_view description);

            [[nodiscard]] ExpressionPtr parseExpression(int minimumBinding);

            [[nodiscard]] ExpressionPtr parsePrefix();

            [[nodiscard]] ExpressionPtr parseAtom();

            [[nodiscard]] ExpressionPtr parseReference();

            [[nodiscard]] ExpressionPtr parseFunctionCall(const std::string &name);

            [[nodiscard]] ExpressionPtr parseArgument();

            [[nodiscard]] Expression::Component parseIndexer();

            [[nodiscard]] ExpressionPtr parseUnitExpression();

            [[nodiscard]] ExpressionPtr parseUnitPower();

            [[nodiscard]] ExpressionPtr parseUnitAtom();

            [[nodiscard]] ExpressionPtr makeNumber(double value);

            [[nodiscard]] ExpressionPtr makeBinary(OperatorExpression::Operator operation, ExpressionPtr left, ExpressionPtr right);

            [[nodiscard]] ExpressionPtr makeUnary(OperatorExpression::Operator operation, ExpressionPtr operand);

            IObjectResolver *m_resolver; ///< 对象解析器，可为空
            ExpressionLexer  m_lexer;    ///< 词法分析器
            ExpressionToken  m_current;  ///< 当前记号
            ExpressionToken  m_next;     ///< 下一记号，用于识别英制两段写法与文档引用
        };

        void ExpressionParserImplementation::advance()
        {
            m_current = m_next;
            m_next    = m_lexer.next();
        }

        bool ExpressionParserImplementation::startsUnit() const
        {
            return m_current.kind == ExpressionTokenKind::Unit || m_current.kind == ExpressionTokenKind::UsUnit;
        }

        void ExpressionParserImplementation::expect(const ExpressionTokenKind kind, const std::string_view description)
        {
            if (m_current.kind != kind)
            {
                throw Base::ParserError(std::format("{}：缺少{}，请补上后重试", locationOf(m_current), description));
            }
            advance();
        }

        std::string ExpressionParserImplementation::takeIdentifierLike(const std::string_view description)
        {
            if (m_current.kind != ExpressionTokenKind::Identifier && m_current.kind != ExpressionTokenKind::CellAddress && m_current.kind != ExpressionTokenKind::String)
            {
                throw Base::ParserError(std::format("{}：需要{}，请检查引用路径的写法", locationOf(m_current), description));
            }

            std::string text = m_current.kind == ExpressionTokenKind::String ? unquoteReference(m_current.text) : m_current.text;
            advance();
            return text;
        }

        ExpressionPtr ExpressionParserImplementation::makeNumber(const double value)
        {
            return std::make_unique<NumberExpression>(m_resolver, Units::Quantity(value));
        }

        ExpressionPtr ExpressionParserImplementation::makeBinary(const OperatorExpression::Operator operation, ExpressionPtr left, ExpressionPtr right)
        {
            return std::make_unique<OperatorExpression>(m_resolver, std::move(left), operation, std::move(right));
        }

        ExpressionPtr ExpressionParserImplementation::makeUnary(const OperatorExpression::Operator operation, ExpressionPtr operand)
        {
            return std::make_unique<OperatorExpression>(m_resolver, std::move(operand), operation, nullptr);
        }

        ExpressionPtr ExpressionParserImplementation::parseDocument()
        {
            if (m_current.kind == ExpressionTokenKind::End)
            {
                throw Base::ParserError("表达式为空，请填入要计算的表达式");
            }

            ExpressionPtr result = parseExpression(0);

            // 顶层只允许一个表达式：剩下的记号说明写法有误（如 "1 2"）
            if (m_current.kind != ExpressionTokenKind::End)
            {
                throw Base::ParserError(std::format("{}：表达式后还有多余内容，请检查是否漏写运算符", locationOf(m_current)));
            }

            return result;
        }

        ExpressionPtr ExpressionParserImplementation::parseExpression(const int minimumBinding)
        {
            ExpressionPtr left                      = parsePrefix();
            // 英制两段写法要求第一段带英制单位（如 5' 6"），因此单独跟踪上一次是否附着了英制单位
            bool          lastAttachmentWasImperial = false;

            for (;;)
            {
                // 单位后置：数字或表达式后面紧跟单位时相乘
                if (startsUnit() && unitPostfixBinding >= minimumBinding)
                {
                    lastAttachmentWasImperial = m_current.kind == ExpressionTokenKind::UsUnit;
                    left                      = makeBinary(OperatorExpression::Operator::Multiply, std::move(left), parseUnitExpression());
                    continue;
                }

                // 英制两段：5' 6" 按「第一段 + 第二段」计算，与 Expression.y 的 USUNIT 规则一致
                if (lastAttachmentWasImperial && (m_current.kind == ExpressionTokenKind::Number || m_current.kind == ExpressionTokenKind::Integer) &&
                    m_next.kind == ExpressionTokenKind::UsUnit)
                {
                    ExpressionPtr segment = makeNumber(m_current.kind == ExpressionTokenKind::Integer ? static_cast<double>(m_current.integerValue) : m_current.numberValue);
                    advance();
                    segment                   = makeBinary(OperatorExpression::Operator::Multiply, std::move(segment), parseUnitExpression());
                    left                      = makeBinary(OperatorExpression::Operator::Add, std::move(left), std::move(segment));
                    lastAttachmentWasImperial = false;
                    continue;
                }

                // 三元条件：优先级最低且右结合
                if (m_current.kind == ExpressionTokenKind::Question && ternaryBinding >= minimumBinding)
                {
                    advance();
                    ExpressionPtr trueBranch = parseExpression(0);
                    expect(ExpressionTokenKind::Colon, "三元运算符的冒号 ':'");
                    ExpressionPtr falseBranch = parseExpression(ternaryBinding);
                    left                      = std::make_unique<ConditionalExpression>(m_resolver, std::move(left), std::move(trueBranch), std::move(falseBranch));
                    lastAttachmentWasImperial = false;
                    continue;
                }

                const auto operatorInfo = binaryOperatorFor(m_current.kind);
                if (!operatorInfo.has_value() || operatorInfo->leftBinding < minimumBinding)
                {
                    break;
                }

                const auto operation = operatorInfo->operation;
                advance();
                ExpressionPtr right       = parseExpression(operatorInfo->rightBinding);
                left                      = makeBinary(operation, std::move(left), std::move(right));
                lastAttachmentWasImperial = false;
            }

            return left;
        }

        ExpressionPtr ExpressionParserImplementation::parsePrefix()
        {
            // 一元正负的绑定功率高于乘方，因此 -2^2 解析为 (-2)^2
            if (m_current.kind == ExpressionTokenKind::Minus)
            {
                advance();
                return makeUnary(OperatorExpression::Operator::Negate, parseExpression(unaryBinding));
            }
            if (m_current.kind == ExpressionTokenKind::Plus)
            {
                advance();
                return makeUnary(OperatorExpression::Operator::Positive, parseExpression(unaryBinding));
            }

            return parseAtom();
        }

        ExpressionPtr ExpressionParserImplementation::parseAtom()
        {
            switch (m_current.kind)
            {
                case ExpressionTokenKind::Number:
                {
                    const double value = m_current.numberValue;
                    advance();
                    return makeNumber(value);
                }
                case ExpressionTokenKind::Integer:
                {
                    const auto value = static_cast<double>(m_current.integerValue);
                    advance();
                    return makeNumber(value);
                }
                case ExpressionTokenKind::String:
                {
                    // <<文档>>.对象.属性 里的文档名也是字符串形态，后面跟点号时按引用路径处理
                    if (m_next.kind == ExpressionTokenKind::Dot)
                    {
                        return parseReference();
                    }
                    const std::string text = unquoteReference(m_current.text);
                    advance();
                    return std::make_unique<StringExpression>(m_resolver, text);
                }
                case ExpressionTokenKind::Constant:
                {
                    const std::string name = m_current.text;
                    advance();
                    // 常量名与取值按词法约定映射：true/false 归一到 True/False，None 取 0
                    if (name == "pi")
                    {
                        return std::make_unique<ConstantExpression>(m_resolver, name, Units::Quantity(std::numbers::pi));
                    }
                    if (name == "e")
                    {
                        return std::make_unique<ConstantExpression>(m_resolver, name, Units::Quantity(std::numbers::e));
                    }
                    if (name == "True" || name == "true")
                    {
                        return std::make_unique<ConstantExpression>(m_resolver, "True", Units::Quantity(1.0));
                    }
                    if (name == "False" || name == "false")
                    {
                        return std::make_unique<ConstantExpression>(m_resolver, "False", Units::Quantity(0.0));
                    }
                    return std::make_unique<ConstantExpression>(m_resolver, name, Units::Quantity(0.0));
                }
                case ExpressionTokenKind::Function:
                    // 按值拷出函数名：advance() 会覆写 m_current，视图会立刻悬垂
                    return parseFunctionCall(std::string{m_current.text});
                case ExpressionTokenKind::Unit:
                case ExpressionTokenKind::UsUnit:
                    return parseUnitExpression();
                case ExpressionTokenKind::LeftParen:
                {
                    advance();
                    ExpressionPtr nested = parseExpression(0);
                    expect(ExpressionTokenKind::RightParen, "与左括号匹配的右括号 ')'");
                    return nested;
                }
                case ExpressionTokenKind::Identifier:
                case ExpressionTokenKind::CellAddress:
                case ExpressionTokenKind::DocumentRef:
                case ExpressionTokenKind::Dot:
                    return parseReference();
                default:
                    throw Base::ParserError(std::format("{}：需要数字、文本、函数、引用或左括号，请检查写法", locationOf(m_current)));
            }
        }

        ExpressionPtr ExpressionParserImplementation::parseReference()
        {
            VariableExpression::Reference reference;

            if (m_current.kind == ExpressionTokenKind::DocumentRef)
            {
                // <<文档#单元格>>：一次性给出文档与目标；没有 # 时按文档名前缀处理
                const std::string raw = unquoteReference(m_current.text);
                advance();
                const auto separator = raw.find('#');
                if (separator == std::string::npos)
                {
                    reference.documentName = raw;
                    if (m_current.kind == ExpressionTokenKind::Dot)
                    {
                        advance();
                        reference.objectName = takeIdentifierLike("文档中的对象名");
                        expect(ExpressionTokenKind::Dot, "对象名后的 '.'");
                        reference.propertyName = takeIdentifierLike("属性名");
                    }
                } else
                {
                    reference.documentName = raw.substr(0, separator);
                    reference.propertyName = raw.substr(separator + 1);
                }
            } else if (m_current.kind == ExpressionTokenKind::String && m_next.kind == ExpressionTokenKind::Dot)
            {
                // <<文档>>.对象.属性
                reference.documentName = unquoteReference(m_current.text);
                advance();
                expect(ExpressionTokenKind::Dot, "文档名后的 '.'");
                reference.objectName = takeIdentifierLike("文档中的对象名");
                expect(ExpressionTokenKind::Dot, "对象名后的 '.'");
                reference.propertyName = takeIdentifierLike("属性名");
            } else if (m_current.kind == ExpressionTokenKind::Dot)
            {
                // .属性：引用当前对象上的属性
                advance();
                reference.propertyName = takeIdentifierLike("当前对象的属性名");
            } else
            {
                const std::string first = takeIdentifierLike("标识符或单元格地址");
                if (m_current.kind == ExpressionTokenKind::Dot)
                {
                    advance();
                    reference.objectName   = first;
                    reference.propertyName = takeIdentifierLike("对象名后的属性名");
                } else
                {
                    // 裸标识符与单元格地址都按当前对象的属性引用处理
                    reference.propertyName = first;
                }
            }

            auto node = std::make_unique<VariableExpression>(m_resolver, std::move(reference));

            // 后续的 .分量 与 [索引] 都挂到同一条引用路径上
            for (;;)
            {
                if (m_current.kind == ExpressionTokenKind::Dot)
                {
                    advance();
                    node->addComponent(Expression::Component(takeIdentifierLike("属性路径上的分量名")));
                    continue;
                }
                if (m_current.kind == ExpressionTokenKind::LeftBracket)
                {
                    node->addComponent(parseIndexer());
                    continue;
                }
                break;
            }

            return node;
        }

        Expression::Component ExpressionParserImplementation::parseIndexer()
        {
            expect(ExpressionTokenKind::LeftBracket, "左方括号 '['");

            ExpressionPtr begin;
            ExpressionPtr end;
            ExpressionPtr step;
            bool          isRange = false;

            if (m_current.kind != ExpressionTokenKind::Colon)
            {
                begin = parseExpression(0);
            }
            if (m_current.kind == ExpressionTokenKind::Colon)
            {
                isRange = true;
                advance();
                if (m_current.kind != ExpressionTokenKind::RightBracket && m_current.kind != ExpressionTokenKind::Colon)
                {
                    end = parseExpression(0);
                }
                if (m_current.kind == ExpressionTokenKind::Colon)
                {
                    advance();
                    if (m_current.kind != ExpressionTokenKind::RightBracket)
                    {
                        step = parseExpression(0);
                    }
                }
            }

            expect(ExpressionTokenKind::RightBracket, "右方括号 ']'");

            if (isRange)
            {
                return Expression::Component::rangeComponent(std::move(begin), std::move(end), std::move(step));
            }

            // 单个字符串下标按映射键处理
            if (const auto *stringIndex = dynamic_cast<const StringExpression *>(begin.get()))
            {
                return Expression::Component::mapKey(stringIndex->getText());
            }

            return Expression::Component::arrayIndex(std::move(begin));
        }

        ExpressionPtr ExpressionParserImplementation::parseFunctionCall(const std::string &name)
        {
            advance(); // 函数记号自带左括号

            std::vector<ExpressionPtr> arguments;
            if (m_current.kind != ExpressionTokenKind::RightParen)
            {
                for (;;)
                {
                    arguments.push_back(parseArgument());
                    // 实参分隔符逗号与分号同义，与 Expression.y 的 args 规则一致
                    if (m_current.kind == ExpressionTokenKind::Comma || m_current.kind == ExpressionTokenKind::Semicolon)
                    {
                        advance();
                        continue;
                    }
                    break;
                }
            }

            expect(ExpressionTokenKind::RightParen, "函数实参后的右括号 ')'");

            const auto function = FunctionExpression::functionFromName(name);
            return std::make_unique<FunctionExpression>(m_resolver, function, std::string{name}, std::move(arguments));
        }

        ExpressionPtr ExpressionParserImplementation::parseArgument()
        {
            // 聚合函数实参可以是区间写法 A1:B2
            if ((m_current.kind == ExpressionTokenKind::Identifier || m_current.kind == ExpressionTokenKind::CellAddress) && m_next.kind == ExpressionTokenKind::Colon)
            {
                const std::string begin = m_current.text;
                advance();
                advance();
                const std::string end = takeIdentifierLike("区间写法冒号后的结束地址");
                return std::make_unique<RangeExpression>(m_resolver, begin, end);
            }

            return parseExpression(0);
        }

        ExpressionPtr ExpressionParserImplementation::parseUnitExpression()
        {
            ExpressionPtr value = parseUnitPower();

            while (m_current.kind == ExpressionTokenKind::Star || m_current.kind == ExpressionTokenKind::Slash)
            {
                const bool isMultiplication = m_current.kind == ExpressionTokenKind::Star;
                advance();
                value = makeBinary(isMultiplication ? OperatorExpression::Operator::Multiply : OperatorExpression::Operator::Divide, std::move(value), parseUnitPower());
            }

            return value;
        }

        ExpressionPtr ExpressionParserImplementation::parseUnitPower()
        {
            ExpressionPtr base = parseUnitAtom();

            if (m_current.kind == ExpressionTokenKind::Caret)
            {
                advance();

                // unit_exp '^' integer 与 unit_exp '^' MINUSSIGN integer 两条规则
                bool isNegativeExponent = false;
                if (m_current.kind == ExpressionTokenKind::Minus)
                {
                    isNegativeExponent = true;
                    advance();
                }
                if (m_current.kind != ExpressionTokenKind::Integer && m_current.kind != ExpressionTokenKind::Number)
                {
                    throw Base::ParserError(std::format("{}：单位幂次需要整数，请把指数写成整数", locationOf(m_current)));
                }

                const double exponentValue = m_current.kind == ExpressionTokenKind::Integer ? static_cast<double>(m_current.integerValue) : m_current.numberValue;
                advance();

                base = makeBinary(OperatorExpression::Operator::Power, std::move(base), makeNumber(isNegativeExponent ? -exponentValue : exponentValue));
            }

            return base;
        }

        ExpressionPtr ExpressionParserImplementation::parseUnitAtom()
        {
            if (m_current.kind == ExpressionTokenKind::Unit || m_current.kind == ExpressionTokenKind::UsUnit)
            {
                const std::string symbol = m_current.text;
                advance();

                // 单位符号表只维护在 Units 模块，这里按符号查表得到「1 个该单位」的量
                const Units::Quantity *unitQuantity = Units::findPredefinedUnit(symbol);
                if (unitQuantity == nullptr)
                {
                    throw Base::ParserError(std::format("表达式里的单位 '{}' 不在单位表中，请改用受支持的单位符号", symbol));
                }

                return std::make_unique<UnitExpression>(m_resolver, *unitQuantity, symbol);
            }

            if (m_current.kind == ExpressionTokenKind::LeftParen)
            {
                advance();
                ExpressionPtr nested = parseUnitExpression();
                expect(ExpressionTokenKind::RightParen, "与左括号匹配的右括号 ')'");
                return nested;
            }

            throw Base::ParserError(std::format("{}：这里需要单位符号或左括号，请检查写法", locationOf(m_current)));
        }

    } // namespace

    ExpressionPtr ExpressionParser::parse(IObjectResolver *resolver, const std::string_view text)
    {
        ExpressionParserImplementation parser{resolver, text};
        return parser.parseDocument();
    }

    std::expected<ExpressionPtr, Base::ParseFailure> ExpressionParser::tryParse(
            IObjectResolver *      resolver,
            const std::string_view text
            )
    {
        try
        {
            return parse(resolver, text);
        } catch (const Base::ParserError &error)
        {
            // 输入非法属可恢复错误：转成值返回，文案与异常通道逐字一致
            return std::unexpected(Base::ParseFailure{error.message()});
        }
    }

} // namespace ExpressionEngine::Expression
