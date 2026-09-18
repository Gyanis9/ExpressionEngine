#include <gtest/gtest.h>

#include <ExpressionEngine/Base/NumericFormatting.h>
#include <ExpressionEngine/Base/NumericInput.h>

#include <charconv>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

    using ExpressionEngine::Base::cLocaleContext;
    using ExpressionEngine::Base::createNumericLocaleContext;
    using ExpressionEngine::Base::localizedDigitAt;
    using ExpressionEngine::Base::LocalizedNumberResult;
    using ExpressionEngine::Base::NumericDiagnosticKind;
    using ExpressionEngine::Base::NumericGrammarPolicy;
    using ExpressionEngine::Base::NumericLocaleContext;
    using ExpressionEngine::Base::NumericSyntaxContext;
    using ExpressionEngine::Base::scanLocalizedNumber;

    /// 取出构造成功的区域快照；失败时把中文错误详情带给断言
    NumericLocaleContext expectContext(const std::string_view identifier)
    {
        const auto result = createNumericLocaleContext(identifier);
        EXPECT_TRUE(result.has_value()) << (result.has_value() ? std::string{} : result.error().message);
        return result.value_or(cLocaleContext());
    }

    /// 断言完整扫描的四个要点：状态、规范写法、值与消费字节数
    void expectScanResult(const std::string_view input, const NumericLocaleContext &locale, const std::string_view canonicalText, const double value,
                          const std::size_t consumedBytes, const NumericSyntaxContext syntax = NumericSyntaxContext::Standalone)
    {
        const auto result = scanLocalizedNumber(input, locale, syntax);
        ASSERT_EQ(result.status, LocalizedNumberResult::Status::Complete) << std::string(input);
        EXPECT_EQ(result.canonicalText, canonicalText) << std::string(input);
        EXPECT_DOUBLE_EQ(result.value, value) << std::string(input);
        EXPECT_EQ(result.consumedBytes, consumedBytes) << std::string(input);
        // 完整结果不该带诊断：诊断只用于说明失败原因
        EXPECT_FALSE(result.diagnostic.has_value()) << std::string(input);
    }

    /// 断言扫描以指定状态与原因停下，并检查诊断定位落在输入范围内
    void expectDiagnostic(const std::string_view input, const NumericLocaleContext &locale, const LocalizedNumberResult::Status status, const NumericDiagnosticKind kind,
                          const NumericSyntaxContext syntax = NumericSyntaxContext::Standalone)
    {
        const auto result = scanLocalizedNumber(input, locale, syntax);
        ASSERT_EQ(result.status, status) << std::string(input);
        ASSERT_TRUE(result.diagnostic.has_value()) << std::string(input);
        EXPECT_EQ(result.diagnostic->kind, kind) << std::string(input);
        // 定位必须落在输入范围内，调用方才能据此给出稳定的错误提示
        EXPECT_LE(result.diagnostic->offsetBytes, input.size()) << std::string(input);
        EXPECT_GE(result.diagnostic->lengthBytes, 1U) << std::string(input);
    }

    /// 校验规范写法确实能按区域无关语法还原出与 value 相同的数值
    void expectCanonicalRoundTrip(const std::string_view input, const NumericLocaleContext &locale)
    {
        const auto result = scanLocalizedNumber(input, locale, NumericSyntaxContext::Standalone);
        ASSERT_EQ(result.status, LocalizedNumberResult::Status::Complete) << std::string(input);
        double     parsed     = 0.0;
        const auto conversion = std::from_chars(result.canonicalText.data(), result.canonicalText.data() + result.canonicalText.size(), parsed, std::chars_format::general);
        ASSERT_EQ(conversion.ec, std::errc{}) << result.canonicalText;
        ASSERT_EQ(conversion.ptr, result.canonicalText.data() + result.canonicalText.size()) << result.canonicalText;
        EXPECT_DOUBLE_EQ(parsed, result.value) << result.canonicalText;
    }

} // namespace

/// @brief 钉住 localizedDigitAt 的 ASCII 快路径：任何区域都认 ASCII 数字且只占一个字节
TEST(NumericInputTest, LocalizedDigitAtReadsAsciiDigitsFirst)
{
    const NumericLocaleContext locale = cLocaleContext();

    int         digit    = -1;
    std::size_t consumed = 0;
    ASSERT_TRUE(localizedDigitAt("7mm", 0, locale, digit, consumed));
    EXPECT_EQ(digit, 7);
    EXPECT_EQ(consumed, 1U);

    ASSERT_TRUE(localizedDigitAt("x0y", 1, locale, digit, consumed));
    EXPECT_EQ(digit, 0);
    EXPECT_EQ(consumed, 1U);
}

/// @brief 钉住 localizedDigitAt 的失败面：非数字、越界位置与非法 UTF-8 都不写输出参数
TEST(NumericInputTest, LocalizedDigitAtRejectsNonDigitsAndKeepsOutputsUntouched)
{
    const NumericLocaleContext locale = cLocaleContext();

    int         digit    = -1;
    std::size_t consumed = 99;
    // 字母与符号都不是数字
    EXPECT_FALSE(localizedDigitAt("x", 0, locale, digit, consumed));
    EXPECT_FALSE(localizedDigitAt(".5", 0, locale, digit, consumed));
    // 越界位置
    EXPECT_FALSE(localizedDigitAt("", 0, locale, digit, consumed));
    EXPECT_FALSE(localizedDigitAt("12", 2, locale, digit, consumed));
    // 非法 UTF-8 前导字节
    const std::string invalidUtf8(1, static_cast<char>(0x80));
    EXPECT_FALSE(localizedDigitAt(invalidUtf8, 0, locale, digit, consumed));
    // 失败时不得写输出参数
    EXPECT_EQ(digit, -1);
    EXPECT_EQ(consumed, 99U);
}

/// @brief 钉住非 ASCII 零字形：连续十个字形都算数字，其余码点一律不算
TEST(NumericInputTest, LocalizedDigitAtSupportsNonAsciiZeroDigit)
{
    NumericLocaleContext arabicIndic = cLocaleContext();
    arabicIndic.localeId             = "custom_arabic";
    arabicIndic.zeroDigit            = "\u0660";

    const std::string digits   = "\u0660\u0665\u0669";
    int               digit    = -1;
    std::size_t       consumed = 0;
    ASSERT_TRUE(localizedDigitAt(digits, 0, arabicIndic, digit, consumed));
    EXPECT_EQ(digit, 0);
    EXPECT_EQ(consumed, 2U);
    ASSERT_TRUE(localizedDigitAt(digits, 2, arabicIndic, digit, consumed));
    EXPECT_EQ(digit, 5);
    ASSERT_TRUE(localizedDigitAt(digits, 4, arabicIndic, digit, consumed));
    EXPECT_EQ(digit, 9);

    // 零字形之后的第十一个码点不属于这十个数字
    const std::string outside = "\u066A";
    EXPECT_FALSE(localizedDigitAt(outside, 0, arabicIndic, digit, consumed));
    // 零字形字段不是单个合法码点时只认 ASCII 数字，不猜测数字集合
    NumericLocaleContext broken = cLocaleContext();
    broken.zeroDigit            = "\u0660\u0660";
    EXPECT_FALSE(localizedDigitAt(digits, 0, broken, digit, consumed));
    // ASCII 数字在任何区域都是数字
    ASSERT_TRUE(localizedDigitAt("3", 0, arabicIndic, digit, consumed));
    EXPECT_EQ(digit, 3);
}

/// @brief 钉住实参位置以外的语法：分隔符原样生效且允许分组
TEST(NumericInputTest, GrammarPolicyKeepsLocaleSeparatorsOutsideFunctionArguments)
{
    const NumericLocaleContext german = expectContext("de_DE");

    for (const NumericSyntaxContext syntax: {NumericSyntaxContext::Standalone, NumericSyntaxContext::Expression})
    {
        const NumericGrammarPolicy policy = ExpressionEngine::Base::numericGrammarPolicy(german, syntax);
        EXPECT_EQ(policy.decimalSeparator, ",");
        EXPECT_EQ(policy.groupingSeparator, ".");
        EXPECT_TRUE(policy.argumentSeparator.empty());
        EXPECT_TRUE(policy.allowGrouping);
    }
}

/// @brief 钉住实参位置的分隔符歧义：分组分隔符与实参分隔符撞车时必须关闭分组
TEST(NumericInputTest, GrammarPolicyDisablesGroupingWhenSeparatorsCollide)
{
    // 逗号做小数点：实参分隔符让位给分号，点号分组与分号不冲突，分组保留
    const NumericGrammarPolicy german = ExpressionEngine::Base::numericGrammarPolicy(expectContext("de_DE"), NumericSyntaxContext::FunctionArgument);
    EXPECT_EQ(german.argumentSeparator, ";");
    EXPECT_TRUE(german.allowGrouping);

    // 逗号仍是实参分隔符：英语区域的分组分隔符就是逗号，必须关闭分组以免误吞实参
    const NumericGrammarPolicy english = ExpressionEngine::Base::numericGrammarPolicy(expectContext("en_US"), NumericSyntaxContext::FunctionArgument);
    EXPECT_EQ(english.argumentSeparator, ",");
    EXPECT_FALSE(english.allowGrouping);
}

/// @brief 钉住 C 区域的完整扫描：小数点、规范写法与符号处理
TEST(NumericInputTest, ScansCompleteNumbersInCLocale)
{
    const NumericLocaleContext locale = cLocaleContext();
    expectScanResult("1234.5", locale, "1234.5", 1234.5, 6);
    expectScanResult("-1.5", locale, "-1.5", -1.5, 4);
    // 正号被识别但不写进规范写法：std::from_chars 不接受前导正号
    expectScanResult("+1.5", locale, "1.5", 1.5, 4);
    expectScanResult(".5", locale, ".5", 0.5, 2);
    expectScanResult("1e3", locale, "1e3", 1000.0, 3);
    expectScanResult("1.5E-2", locale, "1.5e-2", 0.015, 6);
}

/// @brief 钉住前缀消费：记号边界由区域规则决定，余下文本留给调用方
TEST(NumericInputTest, ConsumesOnlyTheLocalizedNumberPrefix)
{
    // 区域小数点后按区域小数点消费，单位文本不进入记号
    expectScanResult("1,5 mm", expectContext("de_DE"), "1.5", 1.5, 3);
    // C 区域的逗号不是分隔符，记号在逗号处结束，只消费 "1"
    expectScanResult("1,5", cLocaleContext(), "1", 1.0, 1);
    // 记号后的空白与字母都不影响已完成的记号
    expectScanResult("12.5mm", cLocaleContext(), "12.5", 12.5, 4);
}

/// @brief 钉住逗号做小数点区域的点号歧义：点号按上下文在分组与小数点之间选择
TEST(NumericInputTest, ResolvesDotAmbiguityInCommaDecimalLocales)
{
    const NumericLocaleContext german = expectContext("de_DE");

    // 点号后不足一组且后面没有别的分组或小数点：按规范小数点处理
    expectScanResult("1.5", german, "1.5", 1.5, 3);
    // 点号后正好一组三位但仍然没有后续分组：同样按规范小数点处理
    expectScanResult("1.234", german, "1.234", 1.234, 5);
    // 点号后是三位的完整分组且还有下一组：按分组分隔符处理
    expectScanResult("1.234.567", german, "1234567", 1234567.0, 9);
    expectScanResult("-1.234.567,5", german, "-1234567.5", -1234567.5, 12);
}

/// @brief 钉住分组位数校验：末组必须等于主要位数，中间各组必须等于次要位数
TEST(NumericInputTest, RejectsWrongGroupingSizes)
{
    const NumericLocaleContext german  = expectContext("de_DE");
    const NumericLocaleContext english = expectContext("en_US");

    // 末组只有两位
    expectDiagnostic("1.234.56", german, LocalizedNumberResult::Status::Invalid, NumericDiagnosticKind::InvalidGrouping);
    // 分组分隔符前没有数字
    expectDiagnostic(",5", english, LocalizedNumberResult::Status::Invalid, NumericDiagnosticKind::InvalidGrouping);
    // C 区域没有分组分隔符，前导逗号是「期望数字」
    expectDiagnostic(",5", cLocaleContext(), LocalizedNumberResult::Status::Invalid, NumericDiagnosticKind::ExpectedDigit);
    // 英语区域里 "1,5" 的分组位数不足三位
    expectDiagnostic("1,5", english, LocalizedNumberResult::Status::Invalid, NumericDiagnosticKind::InvalidGrouping);
}

/// @brief 钉住未写完的四种情况：符号、小数点、分组分隔符、指数标记后缺少内容
TEST(NumericInputTest, ReportsIncompleteTokens)
{
    const NumericLocaleContext german = expectContext("de_DE");
    const NumericLocaleContext locale = cLocaleContext();

    expectDiagnostic("", locale, LocalizedNumberResult::Status::Incomplete, NumericDiagnosticKind::ExpectedDigit);
    expectDiagnostic("-", locale, LocalizedNumberResult::Status::Incomplete, NumericDiagnosticKind::IncompleteSign);
    expectDiagnostic("1.", locale, LocalizedNumberResult::Status::Incomplete, NumericDiagnosticKind::IncompleteDecimal);
    // 逗号在 de_DE 是小数点，要考察「分组未写完」得用逗号作分组符的区域
    expectDiagnostic("1,", expectContext("en_US"), LocalizedNumberResult::Status::Incomplete, NumericDiagnosticKind::IncompleteGrouping);
    expectDiagnostic("1e", locale, LocalizedNumberResult::Status::Incomplete, NumericDiagnosticKind::IncompleteExponent);
    expectDiagnostic("1,5e+", german, LocalizedNumberResult::Status::Incomplete, NumericDiagnosticKind::IncompleteExponent);

    // 未写完时仍给出已扫部分与规范写法，便于调用方续写提示
    const auto single = scanLocalizedNumber("1,", german, NumericSyntaxContext::Standalone);
    EXPECT_EQ(single.consumedBytes, 2U);
    // 已消费的两个字节都要体现在规范写法里：逗号在 de_DE 是小数点，改写后即为 "1."
    EXPECT_EQ(single.canonicalText, "1.");
    const auto decimalOnly = scanLocalizedNumber("1.", cLocaleContext(), NumericSyntaxContext::Standalone);
    EXPECT_EQ(decimalOnly.consumedBytes, 2U);
    EXPECT_EQ(decimalOnly.canonicalText, "1.");
    const auto exponentOnly = scanLocalizedNumber("1e+", cLocaleContext(), NumericSyntaxContext::Standalone);
    EXPECT_EQ(exponentOnly.consumedBytes, 3U);
    EXPECT_EQ(exponentOnly.canonicalText, "1e+");
}

/// @brief 钉住「不静默截断」：空白或不换行空格后接数字时报 UnexpectedSeparator
TEST(NumericInputTest, RejectsUnexpectedSeparatorsBeforeDigits)
{
    const NumericLocaleContext locale = cLocaleContext();

    // 普通空格后接数字：写成了 "1 234" 这类非法分组
    expectDiagnostic("1 234", locale, LocalizedNumberResult::Status::Invalid, NumericDiagnosticKind::UnexpectedSeparator);
    // 不换行空格与普通空格视觉相近，同样不能静默截断记号
    expectDiagnostic("1\u00A0234", locale, LocalizedNumberResult::Status::Invalid, NumericDiagnosticKind::UnexpectedSeparator);

    // 记号后跟单位（非数字）不属于该情况，交给前缀消费处理
    expectScanResult("1 mm", locale, "1", 1.0, 1);
}

/// @brief 钉住数值范围失败：语法正确但超出双精度时报 OutOfRange
TEST(NumericInputTest, RejectsOutOfRangeValues)
{
    expectDiagnostic("1e999", cLocaleContext(), LocalizedNumberResult::Status::Invalid, NumericDiagnosticKind::OutOfRange);
    expectDiagnostic("1,5e999", expectContext("de_DE"), LocalizedNumberResult::Status::Invalid, NumericDiagnosticKind::OutOfRange);
}

/// @brief 钉住实参位置的扫描：结构性逗号不得被当成数字的一部分
TEST(NumericInputTest, KeepsFunctionArgumentSeparatorOutOfNumbers)
{
    const NumericLocaleContext german  = expectContext("de_DE");
    const NumericLocaleContext english = expectContext("en_US");

    // 逗号做小数点、实参分隔符是分号：紧贴数字的逗号仍读成小数点
    expectScanResult("1,5", german, "1.5", 1.5, 3, NumericSyntaxContext::FunctionArgument);
    // 逗号后跟空白说明它是实参分隔符，记号在逗号处结束
    expectScanResult("1, 5", german, "1", 1.0, 1, NumericSyntaxContext::FunctionArgument);
    // 英语区域的分组逗号与实参分隔符冲突，分组被关闭，记号只吃 "1"
    expectScanResult("1,5", english, "1", 1.0, 1, NumericSyntaxContext::FunctionArgument);
}

/// @brief 钉住规范写法的区域无关性：同一份 canonicalText 在任何区域都能还原出同一个值
TEST(NumericInputTest, CanonicalTextIsRegionIndependent)
{
    const NumericLocaleContext locale = cLocaleContext();
    expectCanonicalRoundTrip("1234.5", locale);
    expectCanonicalRoundTrip("-1.5e-3", locale);
    expectCanonicalRoundTrip(".5", locale);
    expectCanonicalRoundTrip("1.234.567", expectContext("de_DE"));
    expectCanonicalRoundTrip("1,5e3", expectContext("de_DE"));
    expectCanonicalRoundTrip("1\u202F234,5", expectContext("fr_FR"));
}

/// @brief 钉住非 ASCII 字形的完整扫描：区域字形换成 ASCII 数字后仍能解析
TEST(NumericInputTest, ScansNumbersWrittenWithLocalizedDigitGlyphs)
{
    NumericLocaleContext arabicIndic = cLocaleContext();
    arabicIndic.localeId             = "custom_arabic";
    arabicIndic.zeroDigit            = "\u0660";

    expectScanResult("\u0661\u0662.\u0665", arabicIndic, "12.5", 12.5, 7);
    // 阿拉伯-印度数字占用 2 字节，ASCII 数字仍占 1 字节
    expectScanResult("-1\u0660", arabicIndic, "-10", -10.0, 4);
}
