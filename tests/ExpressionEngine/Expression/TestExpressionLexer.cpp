#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Expression/ExpressionLexer.h>

namespace
{
    using ExpressionEngine::Expression::ExpressionLexer;
    using ExpressionEngine::Expression::ExpressionToken;
    using ExpressionEngine::Expression::ExpressionTokenKind;

    /// @brief 取尽输入中的记号，不返回结束符
    std::vector<ExpressionToken> tokenize(const std::string_view text)
    {
        ExpressionLexer              lexer{text};
        std::vector<ExpressionToken> tokens;
        for (ExpressionToken token = lexer.next(); token.kind != ExpressionTokenKind::End; token = lexer.next())
        {
            tokens.push_back(token);
        }
        return tokens;
    }

    /// @brief 断言输入恰好是单个 Number 记号并返回其数值，供各类数字写法复用
    double singleNumberValue(const std::string_view text)
    {
        const std::vector<ExpressionToken> tokens = tokenize(text);
        EXPECT_EQ(tokens.size(), 1U) << "输入：" << text;
        if (tokens.size() != 1U)
        {
            return 0;
        }
        EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::Number) << "输入：" << text;
        return tokens[0].numberValue;
    }

    /// @brief 抽出记号序列的类别，便于整体比对顺序
    std::vector<ExpressionTokenKind> kindsOf(const std::vector<ExpressionToken> &tokens)
    {
        std::vector<ExpressionTokenKind> kinds;
        kinds.reserve(tokens.size());
        for (const ExpressionToken &token: tokens)
        {
            kinds.push_back(token.kind);
        }
        return kinds;
    }
} // namespace

/// @brief 钉住纯整数单独成类，且整数与浮点两个字段同时可用
TEST(ExpressionLexerTest, IntegerLiteral)
{
    const std::vector<ExpressionToken> tokens = tokenize("42");
    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::Integer);
    EXPECT_EQ(tokens[0].integerValue, 42);
    EXPECT_DOUBLE_EQ(tokens[0].numberValue, 42.0);
    EXPECT_EQ(tokens[0].text, "42");
    EXPECT_EQ(tokens[0].offset, 0U);
    EXPECT_EQ(tokens[0].column, 1);
}

/// @brief 钉住小数点的各种写法：常规、省略整数部分、以及用逗号当小数点
TEST(ExpressionLexerTest, DecimalPointForms)
{
    EXPECT_DOUBLE_EQ(singleNumberValue("1.5"), 1.5);
    EXPECT_DOUBLE_EQ(singleNumberValue(".5"), 0.5);
    EXPECT_DOUBLE_EQ(singleNumberValue("1,5"), 1.5);
    const std::vector<ExpressionToken> tokens = tokenize("1,5");
    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0].text, "1,5");
}

/// @brief 钉住指数写法，含负指数与显式正号
TEST(ExpressionLexerTest, ExponentForms)
{
    EXPECT_DOUBLE_EQ(singleNumberValue("1e3"), 1000.0);
    EXPECT_DOUBLE_EQ(singleNumberValue("1.5e-3"), 0.0015);
    EXPECT_DOUBLE_EQ(singleNumberValue("1E+2"), 100.0);
    EXPECT_DOUBLE_EQ(singleNumberValue(".25e2"), 25.0);
}

/// @brief 钉住整数越界报 OverflowError；边界内的最大值仍按 Integer 接受
TEST(ExpressionLexerTest, IntegerOverflowThrows)
{
    EXPECT_THROW(static_cast<void>(tokenize("9223372036854775808")), ExpressionEngine::Base::OverflowError);
    EXPECT_THROW(static_cast<void>(tokenize("99999999999999999999")), ExpressionEngine::Base::OverflowError);
    // 恰好等于 long long 最大值不算「超出」，与 Expression.l 里误判最大值的写法有意不同
    const std::vector<ExpressionToken> tokens = tokenize("9223372036854775807");
    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::Integer);
}

/// @brief 钉住单位的最长匹配：mm 不能被拆成两个 m，m 仍单独成记号
TEST(ExpressionLexerTest, UnitLongestMatch)
{
    const std::vector<ExpressionToken> tokens = tokenize("mm m");
    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::Unit);
    EXPECT_EQ(tokens[0].text, "mm");
    EXPECT_EQ(tokens[1].kind, ExpressionTokenKind::Unit);
    EXPECT_EQ(tokens[1].text, "m");
    // 单位与同名标识符等长时单位优先，这是 Expression.l 的规则先后决定的
    const std::vector<ExpressionToken> single = tokenize("m");
    ASSERT_EQ(single.size(), 1U);
    EXPECT_EQ(single[0].kind, ExpressionTokenKind::Unit);
}

/// @brief 钉住数字与紧邻的单位是两枚记号，单位表不会吃掉数字
TEST(ExpressionLexerTest, NumberBeforeUnit)
{
    const std::vector<ExpressionToken> tokens = tokenize("12mm");
    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::Integer);
    EXPECT_EQ(tokens[1].kind, ExpressionTokenKind::Unit);
    EXPECT_EQ(tokens[1].text, "mm");
    EXPECT_EQ(tokens[1].offset, 2U);
    EXPECT_EQ(tokens[1].column, 3);
}

/// @brief 钉住函数名胜过同前缀单位：sin( 不能被拆成 s + in，min( 也不能被单位 min 抢走
TEST(ExpressionLexerTest, FunctionBeatsUnitPrefix)
{
    const std::vector<ExpressionToken> tokens = tokenize("sin(2)");
    ASSERT_EQ(tokens.size(), 3U);
    EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::Function);
    EXPECT_EQ(tokens[0].text, "sin"); // 左括号已被函数记号吃掉
    EXPECT_EQ(tokens[0].offset, 0U);
    EXPECT_EQ(tokens[1].kind, ExpressionTokenKind::Integer);
    EXPECT_EQ(tokens[1].column, 5);
    EXPECT_EQ(tokens[2].kind, ExpressionTokenKind::RightParen);

    // 名字与左括号之间的空白被一起吃掉
    const std::vector<ExpressionToken> spaced = tokenize("sin (2)");
    ASSERT_EQ(spaced.size(), 3U);
    EXPECT_EQ(spaced[0].kind, ExpressionTokenKind::Function);
    EXPECT_EQ(spaced[0].text, "sin");

    // min( 也不能被单位 min 抢走
    const std::vector<ExpressionToken> minute = tokenize("min(1)");
    ASSERT_EQ(minute.size(), 3U);
    EXPECT_EQ(minute[0].kind, ExpressionTokenKind::Function);
    EXPECT_EQ(minute[0].text, "min");

    const std::vector<ExpressionToken> log10Tokens = tokenize("log10(x)");
    ASSERT_EQ(log10Tokens.size(), 3U);
    EXPECT_EQ(log10Tokens[0].kind, ExpressionTokenKind::Function);
    EXPECT_EQ(log10Tokens[0].text, "log10");
    EXPECT_EQ(log10Tokens[1].kind, ExpressionTokenKind::Identifier);
    EXPECT_EQ(log10Tokens[2].kind, ExpressionTokenKind::RightParen);
}

/// @brief 钉住英制建筑单位：英尺 ' 与英寸 " 是 UsUnit，字母写法 in、ft 仍是 Unit
TEST(ExpressionLexerTest, UsUnitQuotes)
{
    const std::vector<ExpressionToken> tokens = tokenize("5' 6\"");
    ASSERT_EQ(tokens.size(), 4U);
    EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::Integer);
    EXPECT_EQ(tokens[1].kind, ExpressionTokenKind::UsUnit);
    EXPECT_EQ(tokens[1].text, "'");
    EXPECT_EQ(tokens[2].kind, ExpressionTokenKind::Integer);
    EXPECT_EQ(tokens[3].kind, ExpressionTokenKind::UsUnit);
    EXPECT_EQ(tokens[3].text, "\"");

    const std::vector<ExpressionToken> spelled = tokenize("in ft");
    ASSERT_EQ(spelled.size(), 2U);
    EXPECT_EQ(spelled[0].kind, ExpressionTokenKind::Unit);
    EXPECT_EQ(spelled[1].kind, ExpressionTokenKind::Unit);
}

/// @brief 钉住 <<...>> 与普通标识符区分，且 text 是去掉定界符、处理完转义的内容
TEST(ExpressionLexerTest, StringLiteral)
{
    const std::vector<ExpressionToken> tokens = tokenize("<<a b>>");
    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::String);
    EXPECT_EQ(tokens[0].text, "a b");

    const std::vector<ExpressionToken> escaped = tokenize("<<a\\nb>>");
    ASSERT_EQ(escaped.size(), 1U);
    EXPECT_EQ(escaped[0].kind, ExpressionTokenKind::String);
    EXPECT_EQ(escaped[0].text, "a\nb");

    const std::vector<ExpressionToken> mixed = tokenize("1+<<x>>");
    ASSERT_EQ(mixed.size(), 3U);
    EXPECT_EQ(mixed[1].kind, ExpressionTokenKind::Plus);
    EXPECT_EQ(mixed[2].kind, ExpressionTokenKind::String);
    EXPECT_EQ(mixed[2].text, "x");
}

/// @brief 钉住带 # 的 <<...>> 按跨文档引用区分出来
TEST(ExpressionLexerTest, DocumentReference)
{
    const std::vector<ExpressionToken> tokens = tokenize("<<Doc#A1>>");
    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::DocumentRef);
    EXPECT_EQ(tokens[0].text, "Doc#A1");
}

/// @brief 钉住单元格地址的三种写法，以及字母超过两位、含多余字符时退回标识符
TEST(ExpressionLexerTest, CellAddressForms)
{
    const std::vector<ExpressionToken> tokens = tokenize("A1 $A$1 $A1 ab12");
    ASSERT_EQ(tokens.size(), 4U);
    for (const ExpressionToken &token: tokens)
    {
        EXPECT_EQ(token.kind, ExpressionTokenKind::CellAddress);
    }
    EXPECT_EQ(tokens[0].text, "A1");
    EXPECT_EQ(tokens[1].text, "$A$1");
    EXPECT_EQ(tokens[2].text, "$A1");
    EXPECT_EQ(tokens[3].text, "ab12");

    const std::vector<ExpressionToken> notAddresses = tokenize("ABC1 A1B _x");
    ASSERT_EQ(notAddresses.size(), 3U);
    for (const ExpressionToken &token: notAddresses)
    {
        EXPECT_EQ(token.kind, ExpressionTokenKind::Identifier);
    }
    EXPECT_EQ(notAddresses[0].text, "ABC1");
    EXPECT_EQ(notAddresses[2].text, "_x");
}

/// @brief 钉住标识符里的 @，并确认 @ 不能进入函数名
TEST(ExpressionLexerTest, IdentifierAllowsAtSign)
{
    const std::vector<ExpressionToken> tokens = tokenize("a@b");
    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::Identifier);
    EXPECT_EQ(tokens[0].text, "a@b");

    // 含 @ 的名字不满足函数名规则（FUNC 规则不含 @），因此按「标识符 + 左括号」切开
    const std::vector<ExpressionToken> call = tokenize("a@b(1)");
    ASSERT_EQ(call.size(), 4U);
    EXPECT_EQ(call[0].kind, ExpressionTokenKind::Identifier);
    EXPECT_EQ(call[0].text, "a@b");
    EXPECT_EQ(call[1].kind, ExpressionTokenKind::LeftParen);
    EXPECT_EQ(call[2].kind, ExpressionTokenKind::Integer);
    EXPECT_EQ(call[3].kind, ExpressionTokenKind::RightParen);
}

/// @brief 钉住常量集合：pi、e、None、True/true、False/false，且 true 归一为 True
TEST(ExpressionLexerTest, Constants)
{
    const std::vector<ExpressionToken> tokens = tokenize("pi e None True true False false");
    ASSERT_EQ(tokens.size(), 7U);
    for (const ExpressionToken &token: tokens)
    {
        EXPECT_EQ(token.kind, ExpressionTokenKind::Constant);
    }
    EXPECT_DOUBLE_EQ(tokens[0].numberValue, 3.141592653589793);
    EXPECT_DOUBLE_EQ(tokens[1].numberValue, 2.718281828459045);
    EXPECT_DOUBLE_EQ(tokens[2].numberValue, 0.0);
    EXPECT_DOUBLE_EQ(tokens[3].numberValue, 1.0);
    EXPECT_DOUBLE_EQ(tokens[4].numberValue, 1.0);
    EXPECT_EQ(tokens[4].text, "True");
    EXPECT_DOUBLE_EQ(tokens[5].numberValue, 0.0);
    EXPECT_EQ(tokens[6].text, "False");
}

/// @brief 钉住运算符与标点逐枚成号：两字符运算符不被拆开，= 与 == 都归 Equal
TEST(ExpressionLexerTest, OperatorsAndPunctuation)
{
    const std::vector<ExpressionToken>     tokens = tokenize("+ - * / % ^ == != < > <= >= = ? : , ; ( ) [ ]");
    const std::vector<ExpressionTokenKind> expected{
            ExpressionTokenKind::Plus, ExpressionTokenKind::Minus, ExpressionTokenKind::Star, ExpressionTokenKind::Slash, ExpressionTokenKind::Percent,
            ExpressionTokenKind::Caret, ExpressionTokenKind::Equal, ExpressionTokenKind::NotEqual, ExpressionTokenKind::Less, ExpressionTokenKind::Greater,
            ExpressionTokenKind::LessEqual, ExpressionTokenKind::GreaterEqual, ExpressionTokenKind::Equal, ExpressionTokenKind::Question, ExpressionTokenKind::Colon,
            ExpressionTokenKind::Comma, ExpressionTokenKind::Semicolon, ExpressionTokenKind::LeftParen, ExpressionTokenKind::RightParen, ExpressionTokenKind::LeftBracket,
            ExpressionTokenKind::RightBracket,
    };
    EXPECT_EQ(kindsOf(tokens), expected);
}

/// @brief 钉住 Unicode 减号 U+2212 也识别为 Minus
TEST(ExpressionLexerTest, UnicodeMinusSign)
{
    const std::vector<ExpressionToken> tokens = tokenize("1−2");
    ASSERT_EQ(tokens.size(), 3U);
    EXPECT_EQ(tokens[1].kind, ExpressionTokenKind::Minus);
    EXPECT_EQ(tokens[1].offset, 1U);
    EXPECT_EQ(tokens[1].column, 2);
    EXPECT_EQ(tokens[2].offset, 4U); // 减号占 3 个字节
}

/// @brief 钉住换行后列号重新从 1 起算，而偏移继续累加
TEST(ExpressionLexerTest, ColumnAfterNewline)
{
    const std::vector<ExpressionToken> tokens = tokenize("1\n 2");
    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[1].kind, ExpressionTokenKind::Integer);
    EXPECT_EQ(tokens[1].offset, 3U);
    EXPECT_EQ(tokens[1].column, 2);
}

/// @brief 钉住多字节单位按码点占一列，报错列号不因 UTF-8 而漂移
TEST(ExpressionLexerTest, MultiByteUnitKeepsColumns)
{
    const std::vector<ExpressionToken> tokens = tokenize("1 ° + 2");
    ASSERT_EQ(tokens.size(), 4U);
    EXPECT_EQ(tokens[1].kind, ExpressionTokenKind::Unit);
    EXPECT_EQ(tokens[1].text, "°");
    EXPECT_EQ(tokens[1].column, 3);
    EXPECT_EQ(tokens[2].column, 5);

    try
    {
        static_cast<void>(tokenize("1 ° + ~"));
        FAIL() << "非法的 ~ 应当报错";
    } catch (const ExpressionEngine::Base::ParserError &error)
    {
        const std::string message = error.what();
        EXPECT_NE(message.find("第 7 列"), std::string::npos) << message;
    }
}

/// @brief 钉住非法字符报 ParserError，且消息里带出错的列号与字符
TEST(ExpressionLexerTest, IllegalCharacterReportsColumn)
{
    try
    {
        static_cast<void>(tokenize("1 + ~"));
        FAIL() << "非法的 ~ 应当报错";
    } catch (const ExpressionEngine::Base::ParserError &error)
    {
        const std::string message = error.what();
        EXPECT_NE(message.find("第 5 列"), std::string::npos) << message;
        EXPECT_NE(message.find('~'), std::string::npos) << message;
    }
}

/// @brief 钉住没有对应记号类别的 '#'、'@'、'{'、'}' 单独出现时报错并给出行列
TEST(ExpressionLexerTest, StandalonePunctuationWithoutTokenKind)
{
    try
    {
        static_cast<void>(tokenize("1 @"));
        FAIL() << "单独出现的 @ 应当报错";
    } catch (const ExpressionEngine::Base::ParserError &error)
    {
        const std::string message = error.what();
        EXPECT_NE(message.find("第 3 列"), std::string::npos) << message;
        EXPECT_NE(message.find('@'), std::string::npos) << message;
    }
    EXPECT_THROW(static_cast<void>(tokenize("1 # 2")), ExpressionEngine::Base::ParserError);
}

/// @brief 钉住点号是独立记号：引用路径切成 标识符 + '.' + 标识符/单元格地址
TEST(ExpressionLexerTest, DotSeparatesReferenceComponents)
{
    const std::vector<ExpressionToken> tokens = tokenize("Sheet.A1");
    ASSERT_EQ(tokens.size(), 3U);
    EXPECT_EQ(tokens[0].kind, ExpressionTokenKind::Identifier);
    EXPECT_EQ(tokens[0].text, "Sheet");
    EXPECT_EQ(tokens[1].kind, ExpressionTokenKind::Dot);
    EXPECT_EQ(tokens[2].kind, ExpressionTokenKind::CellAddress);
    EXPECT_EQ(tokens[2].text, "A1");
}

/// @brief 钉住未闭合的 << 字符串报错，不会退化成一串 '<'
TEST(ExpressionLexerTest, UnterminatedStringThrows)
{
    try
    {
        static_cast<void>(tokenize("<<abc"));
        FAIL() << "未闭合的 << 应当报错";
    } catch (const ExpressionEngine::Base::ParserError &error)
    {
        const std::string message = error.what();
        EXPECT_NE(message.find("第 1 列"), std::string::npos) << message;
        EXPECT_NE(message.find(">>"), std::string::npos) << message;
    }
}

/// @brief 钉住结束行为：输入耗尽返回 End，之后一直返回 End
TEST(ExpressionLexerTest, EndTokenIsSticky)
{
    ExpressionLexer       empty{""};
    const ExpressionToken first = empty.next();
    EXPECT_EQ(first.kind, ExpressionTokenKind::End);
    EXPECT_EQ(first.offset, 0U);
    EXPECT_EQ(first.column, 1);
    EXPECT_EQ(empty.next().kind, ExpressionTokenKind::End);

    ExpressionLexer lexer{"1"};
    EXPECT_EQ(lexer.next().kind, ExpressionTokenKind::Integer);
    const ExpressionToken end = lexer.next();
    EXPECT_EQ(end.kind, ExpressionTokenKind::End);
    EXPECT_EQ(end.offset, 1U);
    EXPECT_EQ(end.column, 2);
    EXPECT_EQ(lexer.next().kind, ExpressionTokenKind::End);
}

/// @brief 钉住只有空白（含回车换行）的输入得到 End
TEST(ExpressionLexerTest, WhitespaceOnlyInput)
{
    const std::vector<ExpressionToken> tokens = tokenize(" \t\r\n ");
    EXPECT_TRUE(tokens.empty());

    const std::vector<ExpressionToken> spaced = tokenize("  1  +  2  ");
    ASSERT_EQ(spaced.size(), 3U);
    EXPECT_EQ(spaced[0].column, 3);
    EXPECT_EQ(spaced[1].column, 6);
    EXPECT_EQ(spaced[2].column, 9);
}

/// @brief 钉住字符串正文的转义：\# 不算跨文档分隔符，\> 不算结束符
TEST(ExpressionLexerTest, EscapedHashAndGreaterSignInStringBody)
{
    const std::vector<ExpressionToken> escapedHash = tokenize(R"(<<a\#b>>)");
    ASSERT_EQ(escapedHash.size(), 1U);
    EXPECT_EQ(escapedHash[0].kind, ExpressionTokenKind::String);
    EXPECT_EQ(escapedHash[0].text, "a#b");

    const std::vector<ExpressionToken> escapedGreater = tokenize(R"(<<x\>>>)");
    ASSERT_EQ(escapedGreater.size(), 1U);
    EXPECT_EQ(escapedGreater[0].kind, ExpressionTokenKind::String);
    EXPECT_EQ(escapedGreater[0].text, "x>");

    // 未转义的 # 仍按跨文档引用对待
    const std::vector<ExpressionToken> reference = tokenize("<<Doc#A1>>");
    ASSERT_EQ(reference.size(), 1U);
    EXPECT_EQ(reference[0].kind, ExpressionTokenKind::DocumentRef);
}
