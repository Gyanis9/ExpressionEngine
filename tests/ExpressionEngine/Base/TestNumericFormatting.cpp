#include <gtest/gtest.h>

#include <ExpressionEngine/Base/NumericFormatting.h>

#include <string>
#include <string_view>
#include <utility>

namespace {

using ExpressionEngine::Base::cLocaleContext;
using ExpressionEngine::Base::createNumericLocaleContext;
using ExpressionEngine::Base::currentNumericLocaleContext;
using ExpressionEngine::Base::NumberNotation;
using ExpressionEngine::Base::NumericLocaleContext;
using ExpressionEngine::Base::NumericLocaleErrorCode;
using ExpressionEngine::Base::publishNumericLocaleContext;

/// 测试用的最小格式描述：与格式化入口的显式参数一一对应
struct FormatSpec {
    int precision{2};                                  ///< 小数位数或有效位数
    NumberNotation notation{NumberNotation::Default};  ///< 记数法
    bool omitGroupSeparator{false};                    ///< 是否关闭分组分隔符
};

/// 构造测试用格式：记数法与精度都显式给出，避免依赖宿主的默认精度配置
FormatSpec makeFormat(const NumberNotation notation, const int precision) {
    return FormatSpec{.precision = precision, .notation = notation};
}

/// 把测试用的格式描述转成库的格式化入口参数
std::string
formatWith(const double value, const FormatSpec& spec, const NumericLocaleContext& locale) {
    return ExpressionEngine::Base::formatNumericValue(
        value, spec.precision, spec.notation, spec.omitGroupSeparator, locale);
}

/// 手工构造宿主自定义快照，用来验证「不必受内置区域表限制」的用法
NumericLocaleContext makeCustomContext(std::string localeId,
                                       std::string decimalSeparator,
                                       std::string groupingSeparator,
                                       const int primaryGroupingSize,
                                       const int secondaryGroupingSize) {
    return NumericLocaleContext{
        .localeId = std::move(localeId),
        .decimalSeparator = std::move(decimalSeparator),
        .groupingSeparator = std::move(groupingSeparator),
        .positiveSign = "+",
        .negativeSign = "-",
        .primaryGroupingSize = primaryGroupingSize,
        .secondaryGroupingSize = secondaryGroupingSize,
        .zeroDigit = "0",
    };
}

/// 取出构造成功的快照；失败时把中文错误详情带给断言
NumericLocaleContext expectContext(const std::string_view identifier) {
    const auto result = createNumericLocaleContext(identifier);
    EXPECT_TRUE(result.has_value())
        << (result.has_value() ? std::string{} : result.error().message);
    return result.value_or(cLocaleContext());
}

/// 断言某个标识符必须因「写法不合法」被拒绝
void expectInvalidIdentifier(const std::string_view identifier) {
    const auto result = createNumericLocaleContext(identifier);
    ASSERT_FALSE(result.has_value()) << "区域 " << std::string(identifier) << " 不应构造成功";
    EXPECT_EQ(result.error().code, NumericLocaleErrorCode::InvalidIdentifier)
        << std::string(identifier);
    EXPECT_FALSE(result.error().message.empty()) << std::string(identifier);
}

/// 断言某个标识符必须因「表内没有该区域」被拒绝，而不是静默退回 C 区域
void expectUnsupportedLocale(const std::string_view identifier) {
    const auto result = createNumericLocaleContext(identifier);
    ASSERT_FALSE(result.has_value()) << "区域 " << std::string(identifier) << " 不应构造成功";
    EXPECT_EQ(result.error().code, NumericLocaleErrorCode::UnsupportedLocale)
        << std::string(identifier);
    // 文案要指出是哪个标识符不受支持，并给出替代做法
    EXPECT_NE(result.error().message.find(std::string(identifier)), std::string::npos)
        << result.error().message;
    EXPECT_NE(result.error().message.find("NumericLocaleContext"), std::string::npos)
        << result.error().message;
}

}  // namespace

/// @brief 钉住 C/POSIX 快照的形状：小数点 "."、无分组、ASCII 零字形
TEST(NumericFormattingTest, CLocaleContextHasDotDecimalAndNoGrouping) {
    const NumericLocaleContext context = cLocaleContext();
    EXPECT_EQ(context.localeId, "C");
    EXPECT_EQ(context.decimalSeparator, ".");
    EXPECT_TRUE(context.groupingSeparator.empty());
    EXPECT_EQ(context.positiveSign, "+");
    EXPECT_EQ(context.negativeSign, "-");
    EXPECT_EQ(context.primaryGroupingSize, 0);
    EXPECT_EQ(context.secondaryGroupingSize, 0);
    EXPECT_EQ(context.zeroDigit, "0");
}

/// @brief 钉住内置区域表的覆盖面：约定的 19 个标识符都必须能构造出可用快照
TEST(NumericFormattingTest, BuiltinLocaleTableCoversDocumentedIdentifiers) {
    for (const std::string_view identifier : {"C",
                                              "en_US",
                                              "en_GB",
                                              "de_DE",
                                              "fr_FR",
                                              "es_ES",
                                              "it_IT",
                                              "pt_BR",
                                              "ru_RU",
                                              "zh_CN",
                                              "ja_JP",
                                              "ko_KR",
                                              "pl_PL",
                                              "nl_NL",
                                              "tr_TR",
                                              "cs_CZ",
                                              "hu_HU",
                                              "sv_SE",
                                              "uk_UA"}) {
        const NumericLocaleContext context = expectContext(identifier);
        EXPECT_EQ(context.localeId, std::string(identifier));
        EXPECT_FALSE(context.decimalSeparator.empty()) << std::string(identifier);
        EXPECT_GE(context.primaryGroupingSize, 0) << std::string(identifier);
        EXPECT_GE(context.secondaryGroupingSize, 0) << std::string(identifier);
        // 内置区域的字形都是 ASCII，扫描与格式化才对得上
        EXPECT_EQ(context.zeroDigit, "0") << std::string(identifier);
    }

    // 只有 C 区域不分组，其余内置区域都按三位分组
    EXPECT_EQ(cLocaleContext().primaryGroupingSize, 0);
    EXPECT_EQ(expectContext("en_US").primaryGroupingSize, 3);
    EXPECT_EQ(expectContext("de_DE").groupingSeparator, ".");
}

/// @brief 钉住标识符规范化：连字符、编码后缀、年份后缀、大小写都折叠到同一个键
TEST(NumericFormattingTest, LocaleIdentifierSpellingsNormalizeToSameContext) {
    const NumericLocaleContext reference = expectContext("de_DE");
    EXPECT_EQ(reference.decimalSeparator, ",");
    EXPECT_EQ(reference.groupingSeparator, ".");

    for (const std::string_view variant :
         {"de-DE", "de_DE.UTF-8", "de-DE-1996", "DE_de", "de_de"}) {
        const NumericLocaleContext context = expectContext(variant);
        EXPECT_EQ(context, reference) << std::string(variant);
        EXPECT_EQ(context.localeId, "de_DE") << std::string(variant);
    }
}

/// @brief 钉住 C 区域的常见写法都等价：C、c、POSIX、posix、C.UTF-8、C.utf8
TEST(NumericFormattingTest, CLocaleAliasesAllResolveToCContext) {
    const NumericLocaleContext reference = cLocaleContext();
    for (const std::string_view alias :
         {"C", "c", "POSIX", "posix", "C.UTF-8", "C.utf8", "POSIX.utf8"}) {
        EXPECT_EQ(expectContext(alias), reference) << std::string(alias);
    }
}

/// @brief 钉住拒绝面之一：空串与含非法字符的标识符必须报 InvalidIdentifier
TEST(NumericFormattingTest, MalformedIdentifiersAreRejectedAsInvalid) {
    // 空串无法判定区域，报错比静默当 C 区域更安全
    expectInvalidIdentifier("");
    // 只有分隔符、没有语言段
    expectInvalidIdentifier("-");
    expectInvalidIdentifier("_DE");
    expectInvalidIdentifier(".UTF-8");
    // 空格、感叹号、分号都不在允许字符集内
    expectInvalidIdentifier("de DE");
    expectInvalidIdentifier("de_DE!");
    expectInvalidIdentifier("de;DE");
    // 中文区域名同样非法：区域标识必须用 ASCII 写法
    expectInvalidIdentifier("德语");
}

/// @brief 钉住拒绝面之二：表外区域必须报 UnsupportedLocale，绝不静默退回 C 区域
TEST(NumericFormattingTest, LocalesOutsideBuiltinTableAreUnsupportedNotSilentlyC) {
    for (const std::string_view identifier : {"en_CA", "ar_SA", "xx_YY", "pt_PT", "de", "en"}) {
        expectUnsupportedLocale(identifier);
    }

    // 反证：受支持的区域确实给得出区域数据，说明上一条不是「一律失败」的假象
    const NumericLocaleContext german = expectContext("de_DE");
    EXPECT_NE(german, cLocaleContext());
    EXPECT_EQ(german.decimalSeparator, ",");
}

/// @brief 钉住 C 与 de_DE 的格式化差异：小数点与分组同时按区域快照生效
TEST(NumericFormattingTest, FormattingDiffersBetweenCLocaleAndGermanLocale) {
    const FormatSpec format = makeFormat(NumberNotation::Fixed, 2);
    EXPECT_EQ(formatWith(1234567.5, format, cLocaleContext()), "1234567.50");
    EXPECT_EQ(formatWith(1234567.5, format, expectContext("de_DE")), "1.234.567,50");
}

/// @brief 钉住分组位数规则：主要位数定末组、次要位数定其余各组、次要为 0 时沿用主要位数
TEST(NumericFormattingTest, GroupingSizesFollowPrimaryAndSecondaryRules) {
    const FormatSpec format = makeFormat(NumberNotation::Fixed, 0);

    // 主要 3 / 次要 2：印度式分组，最左组允许不足两位
    const NumericLocaleContext indianStyle = makeCustomContext("custom_in", ".", ",", 3, 2);
    EXPECT_EQ(formatWith(1234567.0, format, indianStyle), "12,34,567");

    // 次要分组位数为 0 时视为与主要位数相同
    const NumericLocaleContext equalSizes = makeCustomContext("custom_eq", ".", ",", 3, 0);
    EXPECT_EQ(formatWith(1234567.0, format, equalSizes), "1,234,567");

    // 主要位数为 0 表示不分组
    const NumericLocaleContext noGrouping = makeCustomContext("custom_none", ".", ",", 0, 0);
    EXPECT_EQ(formatWith(1234567.0, format, noGrouping), "1234567");

    // 分组分隔符为空时即便位数非 0 也不插入任何分隔符
    const NumericLocaleContext emptySeparator = makeCustomContext("custom_empty", ".", "", 3, 3);
    EXPECT_EQ(formatWith(1234567.0, format, emptySeparator), "1234567");

    // 位数不足一组、正好一组、刚好跨组边界都要正确
    EXPECT_EQ(formatWith(12.0, format, equalSizes), "12");
    EXPECT_EQ(formatWith(123.0, format, equalSizes), "123");
    EXPECT_EQ(formatWith(1234.0, format, equalSizes), "1,234");
}

/// @brief 钉住 OmitGroupSeparator 选项：要求省略分组时不得再插入分隔符
TEST(NumericFormattingTest, OmitGroupSeparatorOptionDisablesGrouping) {
    FormatSpec format = makeFormat(NumberNotation::Fixed, 2);
    format.omitGroupSeparator = true;
    EXPECT_EQ(formatWith(1234567.5, format, expectContext("de_DE")), "1234567,50");
    EXPECT_EQ(formatWith(1234567.5, format, cLocaleContext()), "1234567.50");
}

/// @brief 钉住符号处理：负数用区域负号，正数不加正号（ICU/CLDR 默认没有正号前缀）
TEST(NumericFormattingTest, NegativeSignFollowsLocaleAndPositiveNumbersHaveNoSign) {
    const FormatSpec format = makeFormat(NumberNotation::Fixed, 1);
    EXPECT_EQ(formatWith(-1234.5, format, expectContext("de_DE")), "-1.234,5");
    EXPECT_EQ(formatWith(1234.5, format, expectContext("de_DE")), "1.234,5");

    // 宿主自定义负号（U+2212 减号）同样按快照生效，正号字段不参与输出
    NumericLocaleContext custom = makeCustomContext("custom_sign", ".", ",", 3, 3);
    custom.negativeSign = "\u2212";
    EXPECT_EQ(formatWith(-1234.5, format, custom), "\u22121,234.5");
    EXPECT_EQ(formatWith(1234.5, format, custom), "1,234.5");
}

/// @brief 钉住三种记数法的精度语义与小数分隔符本地化
TEST(NumericFormattingTest, NotationAndPrecisionAreHonored) {
    const NumericLocaleContext german = expectContext("de_DE");

    // Fixed 保留固定小数位（含尾随零）；精度 0 输出整数
    EXPECT_EQ(formatWith(1234.5, makeFormat(NumberNotation::Fixed, 3), cLocaleContext()),
              "1234.500");
    EXPECT_EQ(formatWith(1234.5678, makeFormat(NumberNotation::Fixed, 0), cLocaleContext()),
              "1235");

    // Scientific 的尾数按精度取小数位，小数点换成区域写法，指数标记保持 ASCII
    const std::string scientific =
        formatWith(12345.0, makeFormat(NumberNotation::Scientific, 2), german);
    EXPECT_EQ(scientific.rfind("1,23e", 0), 0U) << scientific;
    EXPECT_EQ(scientific.find('.'), std::string::npos) << scientific;

    // Default 按有效位数取精度并去掉尾随零，量级过大时自动切到科学计数（与 %g/ICU 一致）
    EXPECT_EQ(formatWith(1.5, makeFormat(NumberNotation::Default, 4), cLocaleContext()), "1.5");
    EXPECT_EQ(formatWith(1234567.0, makeFormat(NumberNotation::Default, 4), cLocaleContext())
                  .rfind("1.235e", 0),
              0U);
    // Default 精度非正时退化为最短往返表示，不抛异常也不输出空串
    EXPECT_EQ(formatWith(0.1, makeFormat(NumberNotation::Default, 0), cLocaleContext()), "0.1");
}

/// @brief 钉住多字节分组分隔符：法语用窄不换行空格 U+202F
TEST(NumericFormattingTest, FrenchLocaleUsesNarrowNoBreakSpaceGrouping) {
    const NumericLocaleContext french = expectContext("fr_FR");
    EXPECT_EQ(french.decimalSeparator, ",");
    EXPECT_EQ(french.groupingSeparator, "\u202F");
    EXPECT_EQ(formatWith(1234.5, makeFormat(NumberNotation::Fixed, 1), french), "1\u202F234,5");
}

/// @brief 钉住非 ASCII 零字形：宿主把 zeroDigit 设成阿拉伯-印度数字时输出对应字形
TEST(NumericFormattingTest, HostProvidedZeroDigitLocalizesOutputDigits) {
    NumericLocaleContext arabicIndic = makeCustomContext("custom_arabic", ".", ",", 3, 3);
    arabicIndic.zeroDigit = "\u0660";
    EXPECT_EQ(formatWith(12.5, makeFormat(NumberNotation::Fixed, 1), arabicIndic),
              "\u0661\u0662.\u0665");
}

/// @brief 钉住用法错误：小数点为空时抛 std::invalid_argument 而不是返回脏文本
TEST(NumericFormattingTest, EmptyDecimalSeparatorThrowsInvalidArgument) {
    const NumericLocaleContext broken = makeCustomContext("custom_broken", "", ",", 3, 3);
    EXPECT_THROW(static_cast<void>(formatWith(1.0, makeFormat(NumberNotation::Fixed, 2), broken)),
                 std::invalid_argument);
}

/// @brief 钉住默认发布值：没有调用过 publishNumericLocaleContext 时读取结果是 C 区域
TEST(NumericFormattingTest, CurrentContextDefaultsToCLocale) {
    EXPECT_EQ(currentNumericLocaleContext(), cLocaleContext());
}

/// @brief 钉住整体替换语义：发布后读到新快照，调用方再改自己的副本也不影响已发布状态
TEST(NumericFormattingTest, PublishReplacesTheWholeSnapshot) {
    const NumericLocaleContext original = currentNumericLocaleContext();

    const NumericLocaleContext german = expectContext("de_DE");
    publishNumericLocaleContext(german);
    EXPECT_EQ(currentNumericLocaleContext(), german);

    // 发布是按值整体替换：发布后再改调用方的副本不得影响已发布快照
    NumericLocaleContext modified = german;
    modified.decimalSeparator = "!";
    modified.groupingSeparator = "?";
    modified.localeId = "modified_after_publish";
    EXPECT_EQ(currentNumericLocaleContext(), german);
    EXPECT_EQ(currentNumericLocaleContext().decimalSeparator, ",");

    // 手工构造的快照同样可以整体发布，字段没有任何一个来自上一份快照
    const NumericLocaleContext custom = makeCustomContext("custom_published", ",", "\u00A0", 3, 3);
    publishNumericLocaleContext(custom);
    EXPECT_EQ(currentNumericLocaleContext(), custom);
    EXPECT_EQ(currentNumericLocaleContext().localeId, "custom_published");
    EXPECT_EQ(currentNumericLocaleContext().groupingSeparator, "\u00A0");

    // 复原，避免影响同进程中的其它用例
    publishNumericLocaleContext(original);
}
