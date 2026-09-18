#include <ExpressionEngine/Base/NumericInput.h>

#include <ExpressionEngine/Base/NumericFormatting.h>

#include <cctype>
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ExpressionEngine::Base {
namespace {
/// 判断 position 处是否正好以 value 开头；空 value 一律视为不匹配（C 区域没有分组分隔符）
bool startsAt(const std::string_view text,
              const std::size_t position,
              const std::string_view value) {
    return !value.empty() && position + value.size() <= text.size() &&
           text.substr(position, value.size()) == value;
}

/// 记号边界字符：符号后紧跟这些字符说明只写了符号没写数字
bool boundary(const char character) {
    return std::isspace(static_cast<unsigned char>(character)) != 0 || character == '(' ||
           character == ')' || character == '[' || character == ']' || character == '<' ||
           character == '>' || character == '+' || character == '-' || character == '*' ||
           character == '/' || character == '^' || character == ';';
}

/// 不换行空格族：它们与普通空格视觉相近但不是合法分组分隔符，单独识别以免静默截断记号
bool isSpaceLike(const char32_t codePoint) {
    return codePoint == 0x00A0 || codePoint == 0x2007 || codePoint == 0x2009 || codePoint == 0x202F;
}

/// 解码 position 处的 UTF-8 码点；非法序列或越界返回 false 且不写输出
bool decodeUtf8(const std::string_view text,
                const std::size_t position,
                char32_t& codePoint,
                std::size_t& consumedBytes) noexcept {
    if (position >= text.size()) {
        return false;
    }

    const auto byteAt = [&text](const std::size_t index) {
        return static_cast<unsigned char>(text[index]);
    };

    const unsigned char leadByte = byteAt(position);
    std::size_t sequenceLength = 0;
    char32_t value = 0;
    if (leadByte < 0x80) {
        sequenceLength = 1;
        value = leadByte;
    } else if ((leadByte & 0xE0) == 0xC0 && leadByte >= 0xC2) {
        sequenceLength = 2;
        value = leadByte & 0x1F;
    } else if ((leadByte & 0xF0) == 0xE0) {
        sequenceLength = 3;
        value = leadByte & 0x0F;
    } else if ((leadByte & 0xF8) == 0xF0 && leadByte <= 0xF4) {
        sequenceLength = 4;
        value = leadByte & 0x07;
    } else {
        return false;  // 0xC0/0xC1 是过长编码，0xF5 起超出 Unicode 范围
    }

    if (position + sequenceLength > text.size()) {
        return false;
    }
    for (std::size_t index = 1; index < sequenceLength; ++index) {
        const unsigned char continuation = byteAt(position + index);
        if ((continuation & 0xC0) != 0x80) {
            return false;
        }
        value = (value << 6) | (continuation & 0x3F);
    }
    if ((sequenceLength == 3 && value < 0x800) || (sequenceLength == 4 && value < 0x10000) ||
        value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        return false;  // 过长编码与代理对区段都不是合法码点
    }

    consumedBytes = sequenceLength;
    codePoint = value;
    return true;
}

/// 组装失败结果：consumedBytes 指出已认可的输入前缀，诊断给出出错片段的字节范围
LocalizedNumberResult invalid(const NumericDiagnosticKind kind,
                              const std::size_t offsetBytes,
                              const std::size_t consumed,
                              const std::size_t lengthBytes = 1) {
    LocalizedNumberResult result;
    result.status = LocalizedNumberResult::Status::Invalid;
    result.consumedBytes = consumed;
    result.diagnostic = NumericDiagnostic{kind, offsetBytes, lengthBytes};
    return result;
}

/// 组装未写完的结果：canonical 给出已扫部分的区域无关写法，便于调用方续写提示
LocalizedNumberResult incomplete(const NumericDiagnosticKind kind,
                                 const std::size_t offsetBytes,
                                 const std::size_t consumed,
                                 std::string canonical,
                                 const std::size_t lengthBytes = 1) {
    LocalizedNumberResult result;
    result.status = LocalizedNumberResult::Status::Incomplete;
    result.consumedBytes = consumed;
    result.canonicalText = std::move(canonical);
    result.diagnostic = NumericDiagnostic{kind, offsetBytes, lengthBytes};
    return result;
}

/// 规范写法转双精度的三种结局
enum class CanonicalNumberStatus {
    Complete,
    Invalid,
    OutOfRange
};

/// 解析区域无关的规范写法；扫描阶段已校验过语法，此处只可能因范围失败
CanonicalNumberStatus parseCanonicalDouble(const std::string_view text, double& value) {
    const auto* first = text.data();
    const auto* last = first + text.size();
    const auto conversion = std::from_chars(first, last, value, std::chars_format::general);
    if (conversion.ec == std::errc::result_out_of_range) {
        // 超出双精度范围（含下溢到 0）：数值与文本语义不符，按调用方的 OutOfRange 处置
        return CanonicalNumberStatus::OutOfRange;
    }
    if (conversion.ec != std::errc{} || conversion.ptr != last) {
        return CanonicalNumberStatus::Invalid;
    }
    return std::isfinite(value) ? CanonicalNumberStatus::Complete
                                : CanonicalNumberStatus::OutOfRange;
}

/// 判断 position 处是否是小数点的写法；长度写入 length
bool decimalAt(const std::string_view input,
               const std::size_t position,
               const NumericGrammarPolicy& policy,
               std::size_t& length) {
    length = 0;
    if (startsAt(input, position, policy.decimalSeparator)) {
        if (policy.decimalSeparator == policy.argumentSeparator) {
            return false;  // 该符号在本语法位置是结构性的实参分隔符，不能当小数点
        }
        // 逗号做小数点时，逗号后跟空白说明它是函数实参分隔符；紧贴数字的逗号才是小数点
        if (policy.argumentSeparator == ";" && policy.decimalSeparator == "," &&
            position + policy.decimalSeparator.size() < input.size() &&
            std::isspace(static_cast<unsigned char>(
                input[position + policy.decimalSeparator.size()])) != 0) {
            return false;
        }
        length = policy.decimalSeparator.size();
        return true;
    }
    // 规范点号在任何区域都可作小数点，便于表达式内部沿用与区域无关的写法
    if (input[position] == '.' && policy.decimalSeparator != ".") {
        length = 1;
        return true;
    }
    return input[position] == '.';
}

/// 判断 position 处是否是分组分隔符；长度写入 length
bool groupingAt(const std::string_view input,
                const std::size_t position,
                const NumericLocaleContext& locale,
                const NumericGrammarPolicy& policy,
                const bool alreadyGrouped,
                std::size_t& length) {
    length = 0;
    if (!policy.allowGrouping) {
        return false;
    }
    if (!startsAt(input, position, policy.groupingSeparator)) {
        return false;
    }

    // 逗号做小数点的区域常用点号做分组，此时点号也可能是规范小数点写法。只有后续内容能证明
    // 它是「分组分隔符 + 恰好一组数字 + 小数点或另一组」时才按分组处理，否则让给小数点分支。
    if (!alreadyGrouped && policy.groupingSeparator == "." && policy.decimalSeparator != ".") {
        const auto groupStart = position + policy.groupingSeparator.size();
        std::size_t digitCount = 0;
        std::size_t digitPosition = groupStart;
        while (digitPosition < input.size()) {
            int digit = 0;
            std::size_t digitLength = 0;
            if (!localizedDigitAt(input, digitPosition, locale, digit, digitLength)) {
                break;
            }
            ++digitCount;
            digitPosition += digitLength;
        }
        const auto afterGroup = digitPosition;
        // 与主要分组位数不等，或该组之后既不是小数点也不是下一个分组，则不是分组用法
        if (digitCount != static_cast<std::size_t>(locale.primaryGroupingSize) ||
            (!startsAt(input, afterGroup, policy.decimalSeparator) &&
             !startsAt(input, afterGroup, policy.groupingSeparator))) {
            return false;
        }
    }

    length = policy.groupingSeparator.size();
    if (policy.groupingSeparator == " ") {
        // 普通空格分组只在后面确实跟着数字时才算分组，避免把数量与单位之间的空格吃掉
        int digit = 0;
        std::size_t digitLength = 0;
        if (position + length >= input.size() ||
            !localizedDigitAt(input, position + length, locale, digit, digitLength)) {
            return false;
        }
    }
    return true;
}

/// 校验「各组位数」是否符合区域规则：末组等于主要分组位数，其余各组等于次要分组位数
bool validGrouping(const std::vector<int>& groups, const NumericLocaleContext& locale) {
    // 少于两组说明没有分组；区域未定义分组位数时也无规则可依
    if (groups.size() < 2 || locale.primaryGroupingSize <= 0 || locale.secondaryGroupingSize <= 0) {
        return false;
    }
    if (groups.back() != locale.primaryGroupingSize) {
        return false;
    }
    for (std::size_t index = 1; index + 1 < groups.size(); ++index) {
        if (groups[index] != locale.secondaryGroupingSize) {
            return false;
        }
    }
    // 最左组允许少于次要分组位数（如 1,234,567 的首组只有一位）
    return groups.front() > 0 && groups.front() <= locale.secondaryGroupingSize;
}

/// 判断 position 处是否是正负号；长度写入 length，规范符号写入 canonical
bool signAt(const std::string_view input,
            const std::size_t position,
            const NumericLocaleContext& locale,
            std::size_t& length,
            char& canonical) {
    length = 0;
    canonical = 0;
    if (startsAt(input, position, locale.negativeSign)) {
        length = locale.negativeSign.size();
        canonical = '-';
        return true;
    }
    if (startsAt(input, position, locale.positiveSign)) {
        length = locale.positiveSign.size();
        canonical = '+';
        return true;
    }
    // 区域符号之外始终接受 ASCII 正负号：表达式与规范写法都用它们
    if (input[position] == '-' || input[position] == '+') {
        length = 1;
        canonical = input[position];
        return true;
    }
    return false;
}
}  // namespace

NumericGrammarPolicy numericGrammarPolicy(const NumericLocaleContext& locale,
                                          const NumericSyntaxContext syntax) {
    if (syntax != NumericSyntaxContext::FunctionArgument) {
        return {locale.decimalSeparator, locale.groupingSeparator, {}, true};
    }

    // 逗号做小数点的区域改用分号分隔实参，其余区域仍是逗号。实参分隔符与分组分隔符是同一个
    // 符号时分组必须关闭，否则「1,234」在实参位置会被误当成一个分组数字。
    const std::string_view argumentSeparator =
        locale.decimalSeparator == "," ? std::string_view{";"} : std::string_view{","};
    return {
        locale.decimalSeparator,
        locale.groupingSeparator,
        argumentSeparator,
        locale.groupingSeparator != argumentSeparator,
    };
}

bool localizedDigitAt(const std::string_view input,
                      const std::size_t position,
                      const NumericLocaleContext& locale,
                      int& digit,
                      std::size_t& consumedBytes) {
    if (position >= input.size()) {
        return false;
    }

    // ASCII 数字优先：内置区域表全是 ASCII，这条快路径也是区域无关写法的入口
    if (input[position] >= '0' && input[position] <= '9') {
        digit = input[position] - '0';
        consumedBytes = 1;
        return true;
    }

    std::size_t zeroByteCount = 0;
    char32_t zeroCodePoint = 0;
    if (locale.zeroDigit.empty() ||
        !decodeUtf8(locale.zeroDigit, 0, zeroCodePoint, zeroByteCount) ||
        zeroByteCount != locale.zeroDigit.size()) {
        // 零字形为空或不是一个完整码点时只认 ASCII 数字，不猜测区域数字集合
        return false;
    }

    char32_t codePoint = 0;
    std::size_t inputByteCount = 0;
    if (!decodeUtf8(input, position, codePoint, inputByteCount)) {
        return false;
    }
    if (codePoint < zeroCodePoint || codePoint > zeroCodePoint + 9) {
        return false;
    }

    digit = static_cast<int>(codePoint - zeroCodePoint);
    consumedBytes = inputByteCount;
    return true;
}

LocalizedNumberResult scanLocalizedNumber(const std::string_view input,
                                          const NumericLocaleContext& locale,
                                          const NumericSyntaxContext syntax) {
    const NumericGrammarPolicy policy = numericGrammarPolicy(locale, syntax);
    if (input.empty()) {
        // 空输入还没开始就是「没写数字」，由调用方决定是否当作未写完
        return incomplete(NumericDiagnosticKind::ExpectedDigit, 0, 0, {});
    }

    std::size_t position = 0;
    std::string canonical;
    canonical.reserve(input.size());

    std::size_t signLength = 0;
    char sign = 0;
    if (signAt(input, position, locale, signLength, sign)) {
        // std::from_chars 接受前导负号但不接受前导正号，因此正号只被识别、不写进规范写法
        if (sign != '+') {
            canonical.push_back(sign);
        }
        position += signLength;
        // 只有符号后面就结束或紧跟边界字符时，才是「符号没跟数字」；否则继续扫，交给后续判定
        if (position == input.size() || boundary(input[position])) {
            return incomplete(NumericDiagnosticKind::IncompleteSign, position, position, canonical);
        }
    }

    std::vector<int> groups;
    int digitsInGroup = 0;
    int totalDigits = 0;
    bool grouped = false;
    std::size_t lastGroupingStart = 0;
    std::size_t lastGroupingLength = 0;

    while (position < input.size()) {
        int digit = 0;
        std::size_t digitLength = 0;
        if (localizedDigitAt(input, position, locale, digit, digitLength)) {
            canonical.push_back(static_cast<char>('0' + digit));
            position += digitLength;
            ++digitsInGroup;
            ++totalDigits;
            continue;
        }

        std::size_t separatorLength = 0;
        if (groupingAt(input, position, locale, policy, grouped, separatorLength)) {
            if (digitsInGroup == 0) {
                // 分组分隔符前没有数字（如 ",5"）：整串不是合法记号而不是没写完
                return invalid(
                    NumericDiagnosticKind::InvalidGrouping, position, position, separatorLength);
            }
            groups.push_back(digitsInGroup);
            digitsInGroup = 0;
            grouped = true;
            lastGroupingStart = position;
            lastGroupingLength = separatorLength;
            position += separatorLength;
            int nextDigit = 0;
            std::size_t nextDigitLength = 0;
            if (position == input.size() ||
                !localizedDigitAt(input, position, locale, nextDigit, nextDigitLength)) {
                // 分隔符后没数字：输入可能还没写完，由调用方续写后重扫
                return incomplete(NumericDiagnosticKind::IncompleteGrouping,
                                  position,
                                  position,
                                  canonical,
                                  separatorLength);
            }
            continue;
        }
        break;
    }

    if (totalDigits == 0) {
        // 整数部分一位都没有：只可能是「小数点 + 小数」（如 .5 / .5 的区域写法）
        std::size_t decimalLength = 0;
        if (position >= input.size() || !decimalAt(input, position, policy, decimalLength)) {
            return invalid(NumericDiagnosticKind::ExpectedDigit, position, position);
        }
        canonical.push_back('.');
        position += decimalLength;
        int nextDigit = 0;
        std::size_t nextDigitLength = 0;
        if (position == input.size() ||
            !localizedDigitAt(input, position, locale, nextDigit, nextDigitLength)) {
            return incomplete(NumericDiagnosticKind::IncompleteDecimal,
                              position,
                              position,
                              canonical,
                              decimalLength);
        }
        while (position < input.size()) {
            int fractionalDigit = 0;
            std::size_t fractionalDigitLength = 0;
            if (!localizedDigitAt(
                    input, position, locale, fractionalDigit, fractionalDigitLength)) {
                break;
            }
            canonical.push_back(static_cast<char>('0' + fractionalDigit));
            position += fractionalDigitLength;
            ++totalDigits;
        }
    } else {
        if (grouped) {
            // 各组位数只在记号结束时才能校验，因为末组的位数要看是否已到达小数点或输入末尾
            groups.push_back(digitsInGroup);
            if (!validGrouping(groups, locale)) {
                return invalid(NumericDiagnosticKind::InvalidGrouping,
                               lastGroupingStart,
                               position,
                               lastGroupingLength);
            }
        }

        std::size_t decimalLength = 0;
        if (position < input.size() && decimalAt(input, position, policy, decimalLength)) {
            canonical.push_back('.');
            position += decimalLength;
            int nextDigit = 0;
            std::size_t nextDigitLength = 0;
            if (position == input.size() ||
                !localizedDigitAt(input, position, locale, nextDigit, nextDigitLength)) {
                return incomplete(NumericDiagnosticKind::IncompleteDecimal,
                                  position,
                                  position,
                                  canonical,
                                  decimalLength);
            }
            while (position < input.size()) {
                int fractionalDigit = 0;
                std::size_t fractionalDigitLength = 0;
                if (!localizedDigitAt(
                        input, position, locale, fractionalDigit, fractionalDigitLength)) {
                    break;
                }
                canonical.push_back(static_cast<char>('0' + fractionalDigit));
                position += fractionalDigitLength;
            }
        }
    }

    if (position < input.size() && (input[position] == 'e' || input[position] == 'E')) {
        // 指数标记统一规范成 'e'，指数里的数字仍按区域字形读取
        canonical.push_back('e');
        ++position;
        std::size_t exponentSignLength = 0;
        char exponentSign = 0;
        if (position < input.size() &&
            signAt(input, position, locale, exponentSignLength, exponentSign)) {
            canonical.push_back(exponentSign);
            position += exponentSignLength;
        }
        const auto exponentStart = position;
        while (position < input.size()) {
            int exponentDigit = 0;
            std::size_t exponentDigitLength = 0;
            if (!localizedDigitAt(input, position, locale, exponentDigit, exponentDigitLength)) {
                break;
            }
            canonical.push_back(static_cast<char>('0' + exponentDigit));
            position += exponentDigitLength;
        }
        if (position == exponentStart) {
            return incomplete(
                NumericDiagnosticKind::IncompleteExponent, position, position, canonical);
        }
    }

    if (position < input.size()) {
        // 记号后跟空白再接数字说明写成了「1 234」这类非法分组，不能当成两个记号而悄悄截断
        if (std::isspace(static_cast<unsigned char>(input[position])) != 0) {
            auto next = position;
            while (next < input.size() &&
                   std::isspace(static_cast<unsigned char>(input[next])) != 0) {
                ++next;
            }
            int nextDigit = 0;
            std::size_t nextDigitLength = 0;
            if (next < input.size() &&
                localizedDigitAt(input, next, locale, nextDigit, nextDigitLength)) {
                return invalid(NumericDiagnosticKind::UnexpectedSeparator,
                               position,
                               position,
                               next - position);
            }
        }

        // 与分组分隔符视觉相近的不换行空格也不允许静默截断记号，只有配置的那一个才算分隔符
        char32_t separatorCodePoint = 0;
        std::size_t separatorBytes = 0;
        int nextDigit = 0;
        std::size_t nextDigitBytes = 0;
        const bool separatorIsSpaceLike =
            decodeUtf8(input, position, separatorCodePoint, separatorBytes) &&
            isSpaceLike(separatorCodePoint);
        const bool separatorFollowedByDigit =
            separatorIsSpaceLike &&
            localizedDigitAt(input, position + separatorBytes, locale, nextDigit, nextDigitBytes);
        if (separatorFollowedByDigit) {
            return invalid(
                NumericDiagnosticKind::UnexpectedSeparator, position, position, separatorBytes);
        }

        std::size_t separatorLength = 0;
        if (decimalAt(input, position, policy, separatorLength) ||
            groupingAt(input, position, locale, policy, grouped, separatorLength)) {
            return invalid(
                NumericDiagnosticKind::UnexpectedSeparator, position, position, separatorLength);
        }
    }

    double value = 0.0;
    switch (parseCanonicalDouble(canonical, value)) {
    case CanonicalNumberStatus::Invalid:
        return invalid(NumericDiagnosticKind::InvalidLiteral, 0, position);
    case CanonicalNumberStatus::OutOfRange:
        return invalid(NumericDiagnosticKind::OutOfRange, 0, position);
    case CanonicalNumberStatus::Complete:
        break;
    }

    LocalizedNumberResult result;
    result.status = LocalizedNumberResult::Status::Complete;
    result.value = value;
    result.canonicalText = std::move(canonical);
    result.consumedBytes = position;
    return result;
}
}  // namespace ExpressionEngine::Base
