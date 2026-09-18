#include <ExpressionEngine/Base/NumericFormatting.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ExpressionEngine::Base {
namespace {
/// 内置区域表的一行：分隔符以 string_view 存放，取用时才复制进快照
struct BuiltinLocaleEntry {
    std::string_view localeKey;          ///< 规范化键，形如 "de_DE"
    std::string_view decimalSeparator;   ///< 小数点的 UTF-8 写法
    std::string_view groupingSeparator;  ///< 分组分隔符的 UTF-8 写法，空串表示不分组
    int primaryGroupingSize{};           ///< 末组（最右侧一组）的位数
    int secondaryGroupingSize{};         ///< 其余各组的位数
};

// 区域表按 CLDR 的符号与分组位数整理：正负号各区域都用 ASCII，零字形一律为 ASCII '0'。
// 法语的分组用窄不换行空格 U+202F，俄语、波兰语、捷克语、匈牙利语、瑞典语、乌克兰语用
// 不换行空格 U+00A0，因此分组分隔符是 UTF-8 多字节序列，扫描侧必须整串匹配而不是按单字节比较。
constexpr int defaultGroupingSize = 3;
constexpr std::array<BuiltinLocaleEntry, 19> builtinLocaleTable{{
    {"C", ".", "", 0, 0},
    {"en_US", ".", ",", defaultGroupingSize, defaultGroupingSize},
    {"en_GB", ".", ",", defaultGroupingSize, defaultGroupingSize},
    {"de_DE", ",", ".", defaultGroupingSize, defaultGroupingSize},
    {"fr_FR", ",", "\u202F", defaultGroupingSize, defaultGroupingSize},
    {"es_ES", ",", ".", defaultGroupingSize, defaultGroupingSize},
    {"it_IT", ",", ".", defaultGroupingSize, defaultGroupingSize},
    {"pt_BR", ",", ".", defaultGroupingSize, defaultGroupingSize},
    {"ru_RU", ",", "\u00A0", defaultGroupingSize, defaultGroupingSize},
    {"zh_CN", ".", ",", defaultGroupingSize, defaultGroupingSize},
    {"ja_JP", ".", ",", defaultGroupingSize, defaultGroupingSize},
    {"ko_KR", ".", ",", defaultGroupingSize, defaultGroupingSize},
    {"pl_PL", ",", "\u00A0", defaultGroupingSize, defaultGroupingSize},
    {"nl_NL", ",", ".", defaultGroupingSize, defaultGroupingSize},
    {"tr_TR", ",", ".", defaultGroupingSize, defaultGroupingSize},
    {"cs_CZ", ",", "\u00A0", defaultGroupingSize, defaultGroupingSize},
    {"hu_HU", ",", "\u00A0", defaultGroupingSize, defaultGroupingSize},
    {"sv_SE", ",", "\u00A0", defaultGroupingSize, defaultGroupingSize},
    {"uk_UA", ",", "\u00A0", defaultGroupingSize, defaultGroupingSize},
}};

/// 已发布快照的存储：shared_ptr<const> 保证读侧永远看到一份完整快照
std::mutex publishedLocaleMutex;

/// 懒初始化默认值，避开静态初始化顺序问题（cLocaleContext 也是本翻译单元的函数）
std::shared_ptr<const NumericLocaleContext>& publishedLocaleStorage() {
    static std::shared_ptr<const NumericLocaleContext> state =
        std::make_shared<const NumericLocaleContext>(cLocaleContext());
    return state;
}

/// 标识符里允许出现的字节：ASCII 字母、数字、下划线、连字符、点
bool isIdentifierByte(const char character) noexcept {
    const bool isLetter =
        (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
    const bool isDigit = character >= '0' && character <= '9';
    return isLetter || isDigit || character == '_' || character == '-' || character == '.';
}

/// 把 ASCII 大写折成小写；区域键的语言段按小写匹配
char toLowerAscii(const char character) noexcept {
    return (character >= 'A' && character <= 'Z') ? static_cast<char>(character - 'A' + 'a')
                                                  : character;
}

/// 把 ASCII 小写折成大写；区域键的国家段按大写匹配
char toUpperAscii(const char character) noexcept {
    return (character >= 'a' && character <= 'z') ? static_cast<char>(character - 'a' + 'A')
                                                  : character;
}

/// "C"、"POSIX"、"C.UTF-8" 等写法都指无区域信息的 C 区域
bool isCLocaleIdentifier(const std::string_view identifier) {
    static constexpr std::array<std::string_view, 6> cLocaleNames{
        "c",
        "posix",
        "c.utf-8",
        "c.utf8",
        "posix.utf-8",
        "posix.utf8",
    };
    std::string lowered(identifier);
    for (char& byte : lowered) {
        byte = toLowerAscii(byte);
    }
    return std::ranges::find(cLocaleNames, std::string_view{lowered}) != cLocaleNames.end();
}

/**
 * @brief 把区域标识符折叠成「语言_国家」两段的规范化键
 * @details "de_DE"、"de-DE"、"de_DE.UTF-8"、"de-DE-1996" 都折叠成 "de_DE"；语言段小写、国家段
 *          大写，第三段起（编码、年份等）不参与匹配，因为内置表只按语言与国家区分符号。
 */
std::string canonicalLocaleKey(const std::string_view identifier) {
    std::string key;
    std::string region;
    int segment = 0;  // 0=语言，1=国家/地区，2 起为编码或年份等尾部信息
    for (const char byte : identifier) {
        if (byte == '_' || byte == '-' || byte == '.') {
            ++segment;
            continue;
        }
        if (segment == 0) {
            key += toLowerAscii(byte);
        } else if (segment == 1) {
            region += toUpperAscii(byte);
        }
        // segment >= 2：只保留两段，尾部信息丢弃
    }
    if (!region.empty()) {
        key += '_';
        key += region;
    }
    return key;
}

/// 在内置表里查规范化键，找不到返回 nullptr
const BuiltinLocaleEntry* findBuiltinLocale(const std::string_view localeKey) {
    const auto entry =
        std::ranges::find(builtinLocaleTable, localeKey, &BuiltinLocaleEntry::localeKey);
    return entry == builtinLocaleTable.end() ? nullptr : &*entry;
}

/// 解码 UTF-8 的首个码点；非法序列返回 false 且不写输出
bool decodeUtf8CodePoint(const std::string_view text,
                         std::size_t& consumedBytes,
                         char32_t& codePoint) noexcept {
    const auto byteAt = [&text](const std::size_t index) {
        return static_cast<unsigned char>(text[index]);
    };

    const unsigned char leadByte = byteAt(0);
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

    if (text.size() < sequenceLength) {
        return false;
    }
    for (std::size_t index = 1; index < sequenceLength; ++index) {
        const unsigned char continuation = byteAt(index);
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

/// 把一个码点按 UTF-8 追加到文本尾部
void appendUtf8CodePoint(std::string& text, const char32_t codePoint) {
    if (codePoint < 0x80) {
        text.push_back(static_cast<char>(codePoint));
        return;
    }
    if (codePoint < 0x800) {
        text.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return;
    }
    if (codePoint < 0x10000) {
        text.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        text.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return;
    }
    text.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
    text.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
    text.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
}

/**
 * @brief 用与区域无关的 std::to_chars 输出 ASCII 文本
 * @details 不使用 <sstream>/<iomanip>，结果不随全局区域变化；precision 为空表示最短往返表示。
 *          缓冲不足时按 to_chars 的 value_too_large 倍增重试，定点记数法的极端量级（如 1e308
 *          配大量小数位）会需要很长的输出。
 */
std::string toAsciiNumber(const double value,
                          const std::chars_format notation,
                          const std::optional<int> precision) {
    std::size_t capacity = 512 + static_cast<std::size_t>(std::max(precision.value_or(0), 0));
    for (;;) {
        std::string buffer(capacity, '\0');
        char* const first = buffer.data();
        char* const last = first + buffer.size();
        const auto conversion = precision.has_value()
                                    ? std::to_chars(first, last, value, notation, *precision)
                                    : std::to_chars(first, last, value, notation);
        if (conversion.ec == std::errc{}) {
            buffer.resize(static_cast<std::size_t>(conversion.ptr - first));
            return buffer;
        }
        if (conversion.ec == std::errc::value_too_large) {
            capacity *= 2;
            continue;
        }
        // 除缓冲区不足外只有「标准库未实现该记数法」会失败：GCC 11 之前的 libstdc++ 缺浮点
        // to_chars，属于构建环境问题，调用方换标准库或改用 Default 记数法即可
        throw std::invalid_argument(
            "std::to_chars 无法把数值转成文本：请确认标准库实现了浮点 to_chars（GCC 11 起、"
            "MSVC 2019 起），或改用 Default 记数法。");
    }
}

/// 按区域分组位数在整数部分插入分组分隔符；输入必须是纯 ASCII 数字
std::string applyGrouping(const std::string_view integerDigits,
                          const NumericLocaleContext& formatting,
                          const bool groupingAllowed) {
    const int primary = formatting.primaryGroupingSize;
    const bool groupingPossible =
        groupingAllowed && primary > 0 && !formatting.groupingSeparator.empty();
    const bool allAsciiDigits =
        !integerDigits.empty() && std::ranges::all_of(integerDigits, [](const char byte) {
            return byte >= '0' && byte <= '9';
        });
    // 条件不足或整数部分是 inf/nan 之类内容时原样返回：分组只对纯数字串有意义
    if (!groupingPossible || !allAsciiDigits) {
        return std::string(integerDigits);
    }

    // 次要分组位数为 0 按头部约定视为与主要分组位数相同
    const int secondary =
        formatting.secondaryGroupingSize > 0 ? formatting.secondaryGroupingSize : primary;
    const int digitCount = static_cast<int>(integerDigits.size());
    const auto separatorCount = static_cast<std::size_t>(digitCount / primary);
    std::string grouped;
    grouped.reserve(integerDigits.size() + separatorCount * formatting.groupingSeparator.size());

    // 从右往左切组：最右一组固定 primary 位，其左侧各组固定 secondary 位，
    // 剩余的前缀自成一组（位数可以少于 secondary），因此首位不会出现空组
    int groupEnd = digitCount;
    int groupSize = primary;
    while (groupEnd > 0) {
        const int groupBegin = groupEnd - groupSize > 0 ? groupEnd - groupSize : 0;
        grouped.insert(0,
                       integerDigits.substr(static_cast<std::size_t>(groupBegin),
                                            static_cast<std::size_t>(groupEnd - groupBegin)));
        if (groupBegin > 0) {
            grouped.insert(0, formatting.groupingSeparator);
        }
        groupEnd = groupBegin;
        groupSize = secondary;
    }

    return grouped;
}

/// 把 ASCII 数字换成本区域零字形起的连续字形；零字形为 ASCII '0' 时原样返回
std::string localizeDigits(std::string text, const std::string& zeroDigit) {
    std::size_t zeroBytes = 0;
    char32_t zeroCodePoint = 0;
    if (zeroDigit == "0" || zeroDigit.empty() ||
        !decodeUtf8CodePoint(zeroDigit, zeroBytes, zeroCodePoint) ||
        zeroBytes != zeroDigit.size()) {
        // 内置区域表全是 ASCII 数字；零字形不是单个合法码点时也退回 ASCII，避免产出乱码
        return text;
    }

    std::string localized;
    localized.reserve(text.size() * zeroBytes);
    for (const char byte : text) {
        if (byte >= '0' && byte <= '9') {
            appendUtf8CodePoint(localized, zeroCodePoint + static_cast<char32_t>(byte - '0'));
            continue;
        }
        // 符号、小数点、分组分隔符与指数标记保持 ASCII：它们由区域快照另行决定
        localized.push_back(byte);
    }
    return localized;
}
}  // namespace

NumericLocaleContext cLocaleContext() {
    // C/POSIX 区域不看小数点以外的任何区域数据：无分组、ASCII 字形
    return NumericLocaleContext{
        .localeId = "C",
        .decimalSeparator = ".",
        .groupingSeparator = "",
        .positiveSign = "+",
        .negativeSign = "-",
        .primaryGroupingSize = 0,
        .secondaryGroupingSize = 0,
        .zeroDigit = "0",
    };
}

std::expected<NumericLocaleContext, NumericLocaleError>
createNumericLocaleContext(const std::string_view localeId) {
    if (localeId.empty()) {
        // 空串无法判定区域，且静默当成 C 区域会让「用户配置为空」这种配置错误悄悄生效
        return std::unexpected(NumericLocaleError{
            NumericLocaleErrorCode::InvalidIdentifier,
            "区域标识符为空：请传入 \"C\"、\"POSIX\" 或 \"语言_国家\" 形式的标识符（如 "
            "\"de_DE\"）。",
        });
    }

    // 非法字节先于区域表检查：含非法字符要与「合法但表外」区分开，后者才能提示换区域
    for (const char byte : localeId) {
        if (!isIdentifierByte(byte)) {
            return std::unexpected(NumericLocaleError{
                NumericLocaleErrorCode::InvalidIdentifier,
                std::format(
                    "区域标识符 \"{}\" 含非法字符 \"{}\"：只允许字母、数字、下划线、连字符与点"
                    "（如 \"de_DE\"、\"de-DE.UTF-8\"）。",
                    localeId,
                    byte),
            });
        }
    }

    if (isCLocaleIdentifier(localeId)) {
        return cLocaleContext();
    }

    const std::string key = canonicalLocaleKey(localeId);
    if (key.empty() || key.front() == '_') {
        // 只有分隔符或以分隔符开头（如 "-"、".UTF-8"）时没有语言段，任何区域表都匹配不上
        return std::unexpected(NumericLocaleError{
            NumericLocaleErrorCode::InvalidIdentifier,
            std::format(
                "区域标识符 \"{}\" 缺少语言段：请写成 \"语言_国家\"（如 \"de_DE\"）或 \"C\"。",
                localeId),
        });
    }

    const BuiltinLocaleEntry* entry = findBuiltinLocale(key);
    if (entry == nullptr) {
        // 表外区域明确报错而不是退回 C 区域：否则用户以为生效的区域设置会静默失效
        return std::unexpected(NumericLocaleError{
            NumericLocaleErrorCode::UnsupportedLocale,
            std::format(
                "区域 \"{}\"（规范化后为 \"{}\"）没有内置分隔符数据：可改用受支持的区域（如 "
                "\"de_DE\"、\"en_US\"、\"zh_CN\"），或自行构造 NumericLocaleContext 填入小数点与"
                "分组分隔符后发布。",
                localeId,
                key),
        });
    }

    return NumericLocaleContext{
        .localeId = std::string{entry->localeKey},
        .decimalSeparator = std::string{entry->decimalSeparator},
        .groupingSeparator = std::string{entry->groupingSeparator},
        .positiveSign = "+",
        .negativeSign = "-",
        .primaryGroupingSize = entry->primaryGroupingSize,
        .secondaryGroupingSize = entry->secondaryGroupingSize,
        // 内置表的区域都用 ASCII 数字：非 ASCII 零字形可由宿主自行构造快照提供
        .zeroDigit = "0",
    };
}

void publishNumericLocaleContext(NumericLocaleContext state) {
    const std::lock_guard lock(publishedLocaleMutex);
    // 整体替换：老读者手里的 shared_ptr 仍指向旧快照，不会读到半新半旧的符号组合
    publishedLocaleStorage() = std::make_shared<const NumericLocaleContext>(std::move(state));
}

NumericLocaleContext currentNumericLocaleContext() {
    std::shared_ptr<const NumericLocaleContext> state;
    {
        // 读侧只在锁内取 shared_ptr 副本，解引用放在锁外，缩短持锁时间
        const std::lock_guard lock(publishedLocaleMutex);
        state = publishedLocaleStorage();
    }
    return *state;
}

std::string formatNumericValue(const double value,
                               const int precision,
                               const NumberNotation notation,
                               const bool omitGroupSeparator,
                               const NumericLocaleContext& formatting) {
    if (formatting.decimalSeparator.empty()) {
        // 没有小数点就无法把整数部分与小数部分分开，属调用方用法错误而不是可恢复故障
        throw std::invalid_argument(
            "区域快照 \"" + formatting.localeId +
            "\" 的小数点为空，无法格式化数值：请改用 cLocaleContext() 或 "
            "createNumericLocaleContext() 得到的快照，并为自定义快照填好 decimalSeparator。");
    }

    std::string ascii;
    switch (notation) {
    case NumberNotation::Fixed:
        // 定点：小数位数固定为精度，位数不足补零，与 ICU setMinimum/MaximumFractionDigits 一致
        ascii = toAsciiNumber(value, std::chars_format::fixed, std::max(precision, 0));
        break;
    case NumberNotation::Scientific:
        // 科学计数：尾数按精度保留小数位；指数标记保持 ASCII 'e'，扫描侧才认得出该记号
        ascii = toAsciiNumber(value, std::chars_format::scientific, std::max(precision, 0));
        break;
    case NumberNotation::Default:
    default:
        // Default 按有效位数取精度。general 记数法本身就是 %g 规则：指数小于 -4 或大于等于
        // 精度时切换成科学计数并去掉尾随零，因此不需要再手写 ICU/Qt 的那套阈值分支。
        // 精度非正说明宿主没配置精度，退化为最短往返表示。
        ascii = precision > 0 ? toAsciiNumber(value, std::chars_format::general, precision)
                              : toAsciiNumber(value, std::chars_format::general, std::nullopt);
        break;
    }

    // 指数片段整体保留 ASCII：区域化只作用于数值本体，避免指数符号变成区域写法后无法回读
    std::string body = ascii;
    std::string exponent;
    if (const auto exponentPosition = body.find_first_of("eE");
        exponentPosition != std::string::npos) {
        exponent = body.substr(exponentPosition);
        body.resize(exponentPosition);
    }

    const bool negative = !body.empty() && body.front() == '-';
    if (negative) {
        body.erase(0, 1);  // 去掉 to_chars 的 ASCII 负号，负号稍后按区域快照输出
    }

    const auto pointPosition = body.find('.');
    const bool hasDecimalPoint = pointPosition != std::string::npos;
    const std::string_view bodyView{body};
    const std::string_view integerDigits =
        hasDecimalPoint ? bodyView.substr(0, pointPosition) : bodyView;
    const std::string_view fractionDigits =
        hasDecimalPoint ? bodyView.substr(pointPosition + 1) : std::string_view{};

    // 调用方要求不带分组分隔符时，分组必须整体关闭
    const bool groupingAllowed = !omitGroupSeparator;
    const std::string groupedInteger = applyGrouping(integerDigits, formatting, groupingAllowed);

    std::string result;
    result.reserve(body.size() + exponent.size() + formatting.decimalSeparator.size() + 8);
    if (negative) {
        // 正数不加正号：ICU/CLDR 默认没有正号前缀，positiveSign 只在扫描侧使用
        result += formatting.negativeSign;
    }
    result += groupedInteger;
    if (hasDecimalPoint) {
        result += formatting.decimalSeparator;
    }
    result += fractionDigits;
    result += exponent;

    return localizeDigits(std::move(result), formatting.zeroDigit);
}
}  // namespace ExpressionEngine::Base
