#include <ExpressionEngine/Expression/ExpressionLexer.h>
#include <ExpressionEngine/Base/FirstByteDispatch.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <format>
#include <iterator>
#include <numbers>
#include <string>
#include <string_view>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Expression
{
    namespace
    {
        // —— 字符分类 ——
        /// 判断是否为 ASCII 十进制数字
        constexpr bool isDecimalDigit(const char character)
        {
            return character >= '0' && character <= '9';
        }

        /// 判断是否为 ASCII 字母
        constexpr bool isAsciiLetter(const char character)
        {
            return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
        }

        /// 判断是否为非 ASCII 字节；Expression.l 用 Unicode 表允许非 ASCII 字母做标识符，这里把非 ASCII
        /// 一律当字母
        constexpr bool isNonAsciiByte(const char character)
        {
            return static_cast<unsigned char>(character) >= 0x80;
        }

        /// 字母类字符：ASCII 字母或非 ASCII 字符
        constexpr bool isLetterLike(const char character)
        {
            return isAsciiLetter(character) || isNonAsciiByte(character);
        }

        /// 标识符首字符：字母或下划线（Expression.l 的 IDENTIFIER 规则前半段）
        constexpr bool isIdentifierStart(const char character)
        {
            return isLetterLike(character) || character == '_';
        }

        /// 标识符后续字符：首字符集合再加数字与 '@'
        constexpr bool isIdentifierContinue(const char character)
        {
            return isIdentifierStart(character) || isDecimalDigit(character) || character == '@';
        }

        /// 函数名的后续字符：首字符集合再加数字（不含 '@'，Expression.l 的 FUNC 规则）
        constexpr bool isFunctionNameContinue(const char character)
        {
            return isIdentifierStart(character) || isDecimalDigit(character);
        }

        /// 热路径上的字符类别位图：把三个谓词在编译期铺成 256 项查表，逐字节判定不再走函数调用链
        struct CharacterClassBitmaps
        {
            std::array<std::uint8_t, 256> identifierStart{};      ///< 标识符首字符：字母、下划线、非 ASCII
            std::array<std::uint8_t, 256> identifierContinue{};   ///< 标识符后续字符：首字符集合再加数字与 '@'
            std::array<std::uint8_t, 256> functionNameContinue{}; ///< 函数名后续字符：首字符集合再加数字（不含 '@'）
        };

        /// 在编译期把字符类别谓词铺成位图，下标是字节值
        constexpr CharacterClassBitmaps buildCharacterClassBitmaps()
        {
            CharacterClassBitmaps bitmaps;
            for (std::size_t index = 0; index < bitmaps.identifierStart.size(); ++index)
            {
                const char character                = static_cast<char>(static_cast<unsigned char>(index));
                bitmaps.identifierStart[index]      = isIdentifierStart(character) ? 1 : 0;
                bitmaps.identifierContinue[index]   = isIdentifierContinue(character) ? 1 : 0;
                bitmaps.functionNameContinue[index] = isFunctionNameContinue(character) ? 1 : 0;
            }
            return bitmaps;
        }

        /// 编译期建好的字符类别位图；静态存储期且无运行时初始化
        constexpr CharacterClassBitmaps characterClasses = buildCharacterClassBitmaps();

        /// U+2212 减号的 UTF-8 编码；它属于运算符，不能被「非 ASCII 一律当字母」的约定吞进标识符
        constexpr std::string_view unicodeMinusSign{"−"};

        /// 减号序列的首字节：先比这一个字节就能挡掉其余多字节检查
        constexpr char unicodeMinusSignLeadByte = unicodeMinusSign.front();

        /// offset 处是否是 U+2212 减号序列
        constexpr bool startsWithUnicodeMinus(const std::string_view text, const std::size_t offset)
        {
            return text.size() - offset >= unicodeMinusSign.size() && text.compare(offset, unicodeMinusSign.size(), unicodeMinusSign) == 0;
        }

        /// 扫描连续的 ASCII 数字，返回扫过的字节数
        std::size_t scanDecimalDigits(std::string_view text, const std::size_t offset)
        {
            std::size_t position = offset;
            while (position < text.size() && isDecimalDigit(text[position]))
            {
                ++position;
            }
            return position - offset;
        }

        /// 取 offset 处的整个 UTF-8 字符，用于把非法字符原样写进报错文案
        std::string_view currentCharacterView(std::string_view text, const std::size_t offset)
        {
            const auto  leadByte = static_cast<unsigned char>(text[offset]);
            std::size_t length   = 1;
            if ((leadByte & 0xE0) == 0xC0)
            {
                length = 2;
            } else if ((leadByte & 0xF0) == 0xE0)
            {
                length = 3;
            } else if ((leadByte & 0xF8) == 0xF0)
            {
                length = 4;
            }
            return text.substr(offset, std::min(length, text.size() - offset));
        }

        // —— 单位符号表 ——
        /// 国际单位符号表：逐条对应 Expression.l 的单位规则；顺序即规则顺序，等长匹配时靠前者胜出
        constexpr std::string_view unitSymbols[]{
                "nm", "um", "µm", "mm", "cm", "dm", "m", "km",
                "l", "ml",
                "Hz", "kHz", "MHz", "GHz", "THz",
                "ug", "µg", "mg", "g", "kg", "t",
                "s", "min", "h",
                "A", "nA", "uA", "µA", "mA", "kA", "MA",
                "K", "mK", "µK", "uK",
                "mol", "nmol", "µmol", "umol", "mmol",
                "cd",
                "in", "ft", "thou", "mil", "yd", "mi",
                "mph", "sqft", "cft",
                "lb", "lbm", "oz", "st", "cwt",
                "lbf",
                "N", "mN", "kN", "MN",
                "Pa", "kPa", "MPa", "GPa",
                "bar", "mbar",
                "Torr", "mTorr", "uTorr", "µTorr",
                "psi", "ksi", "Mpsi",
                "W", "nW", "uW", "µW", "mW", "kW", "VA",
                "V", "kV", "mV",
                "MS", "kS", "S", "mS", "uS", "µS",
                "Ohm", "kOhm", "MOhm",
                "C",
                "T", "mT", "G",
                "Wb",
                "F", "mF", "µF", "uF", "nF", "pF",
                "H", "mH", "µH", "uH", "nH",
                "J", "mJ", "kJ", "Nm", "VAs", "CV", "Ws", "kWh", "eV", "keV", "MeV", "cal", "kcal",
                "°", "deg", "rad", "gon", "M", "′", "AS", "″",
        };

        /// 英制建筑单位符号：`"` 英寸、`'` 英尺（Expression.l 里返回 USUNIT 的两条规则）
        constexpr std::string_view usUnitSymbols[]{"\"", "'"};

        /// 一条单位符号候选：符号文本与它对应的记号类别
        struct UnitSymbolEntry
        {
            std::string_view    symbol; ///< 符号文本
            ExpressionTokenKind kind;   ///< Unit 或 UsUnit
        };

        /// 单位符号候选总表：国际单位在前、英制建筑单位在后，各带上记号类别
        constexpr std::array<UnitSymbolEntry, std::size(unitSymbols) + std::size(usUnitSymbols)>
        buildUnitSymbolEntries()
        {
            std::array<UnitSymbolEntry, std::size(unitSymbols) + std::size(usUnitSymbols)> entries{};
            std::size_t                                                                    index = 0;
            for (const std::string_view symbol: unitSymbols)
            {
                entries[index++] = {.symbol = symbol, .kind = ExpressionTokenKind::Unit};
            }
            for (const std::string_view symbol: usUnitSymbols)
            {
                entries[index++] = {.symbol = symbol, .kind = ExpressionTokenKind::UsUnit};
            }
            return entries;
        }

        /// 候选总表与它的首字节分派表；静态存储期、编译期建好，无运行时初始化与堆分配
        constexpr auto                                                               unitSymbolEntries  = buildUnitSymbolEntries();
        constexpr Base::FirstByteDispatch<UnitSymbolEntry, unitSymbolEntries.size()> unitSymbolDispatch = Base::buildFirstByteDispatch(
                unitSymbolEntries,
                [](const UnitSymbolEntry &entry)
                {
                    return static_cast<std::size_t>(static_cast<unsigned char>(entry.symbol.front()));
                }
                );

        /// 单位符号匹配结果
        struct UnitMatch
        {
            std::size_t         length{0};                       ///< 匹配到的字节数
            ExpressionTokenKind kind{ExpressionTokenKind::Unit}; ///< Unit 或 UsUnit
        };

        /// 在 offset 处做单位符号的最长匹配：先按首字节把候选缩到一组，再在组内挑最长；
        /// 只在严格更长时替换，因此等长时保留表里靠前的符号
        UnitMatch matchUnitSymbol(const std::string_view text, const std::size_t offset)
        {
            const Base::FirstByteBucket bucket    = unitSymbolDispatch.buckets[static_cast<unsigned char>(text[offset])];
            const std::string_view      remainder = text.substr(offset);
            UnitMatch                   best;
            for (std::size_t index = 0; index < bucket.count; ++index)
            {
                const UnitSymbolEntry &entry = *unitSymbolDispatch.entries[bucket.begin + index];
                if (entry.symbol.size() > best.length && remainder.starts_with(entry.symbol))
                {
                    best = {.length = entry.symbol.size(), .kind = entry.kind};
                }
            }
            return best;
        }

        // —— 数字 ——

        /// 数字匹配结果
        struct NumberMatch
        {
            std::size_t         length{0};                          ///< 匹配到的字节数；0 表示当前位置不是数字
            ExpressionTokenKind kind{ExpressionTokenKind::Integer}; ///< Number（带小数点或指数）或 Integer
        };

        /// 扫描 {EXPO} = [eE][-+]?[0-9]+，返回其字节数；不构成指数时返回 0
        std::size_t scanExponent(const std::string_view text, const std::size_t offset)
        {
            if (offset >= text.size() || (text[offset] != 'e' && text[offset] != 'E'))
            {
                return 0;
            }
            std::size_t position = offset + 1;
            if (position < text.size() && (text[position] == '+' || text[position] == '-'))
            {
                ++position;
            }
            const std::size_t digits = scanDecimalDigits(text, position);
            if (digits == 0)
            {
                return 0; // 没有数字的 e 不算指数，留给常量 e 或标识符去匹配
            }
            return position + digits - offset;
        }

        /// 按 Expression.l 的四条数字规则求最长匹配
        NumberMatch matchNumber(const std::string_view text, const std::size_t offset)
        {
            NumberMatch       best;
            const std::size_t integerDigits = scanDecimalDigits(text, offset);
            if (integerDigits > 0)
            {
                best = {.length = integerDigits, .kind = ExpressionTokenKind::Integer};
                if (const std::size_t exponent = scanExponent(text, offset + integerDigits); exponent > 0)
                {
                    best = {.length = integerDigits + exponent, .kind = ExpressionTokenKind::Number}; // {DIGIT}+{EXPO}
                }
            }
            // {DIGIT}* ("." | ",") {DIGIT}+ {EXPO}?：逗号在 FreeCAD 表达式里也当小数点用
            if (integerDigits < text.size() - offset && (text[offset + integerDigits] == '.' || text[offset + integerDigits] == ','))
            {
                const std::size_t fractionStart  = offset + integerDigits + 1;
                const std::size_t fractionDigits = scanDecimalDigits(text, fractionStart);
                if (fractionDigits > 0)
                {
                    const std::size_t exponent = scanExponent(text, fractionStart + fractionDigits);
                    const std::size_t length   = integerDigits + 1 + fractionDigits + exponent;
                    if (length > best.length)
                    {
                        best = {.length = length, .kind = ExpressionTokenKind::Number};
                    }
                }
            }
            return best;
        }

        /// 把数字原文里的逗号小数点规范成点号
        std::string normalizeDecimalSeparator(const std::string_view raw)
        {
            std::string normalized{raw};
            for (char &character: normalized)
            {
                if (character == ',')
                {
                    character = '.';
                }
            }
            return normalized;
        }

        /// 解析 Number 的数值
        double parseNumberValue(const std::string_view raw, const int column)
        {
            const std::string            normalized = normalizeDecimalSeparator(raw);
            double                       value      = 0;
            const char *const            begin      = normalized.data();
            const char *const            end        = begin + normalized.size();
            const std::from_chars_result result     = std::from_chars(begin, end, value);
            if (result.ptr != end)
            {
                // 数字写法已被词法规则限定，走到这里说明实现与规则不一致：宁可报错也不给出可疑数值
                throw Base::ParserError(std::format("数字 '{}' 无法解析，请检查表达式第 {} 列", raw, column));
            }
            // 超出 double 范围时（如 1e999）from_chars 报 out_of_range 并把 value 写成饱和值，
            // 与 strtod 的行为一致，因此不报错
            return value;
        }

        /// 解析 Integer 的整数值；越界时报 OverflowError（对应 Expression.l 里 strtoll 之后的溢出检查）
        long long parseIntegerValue(const std::string_view raw, const int column)
        {
            long long                    value  = 0;
            const std::from_chars_result result = std::from_chars(raw.data(), raw.data() + raw.size(), value);
            if (result.ec == std::errc::result_out_of_range)
            {
                // 纯数字串只可能向上溢出（负号是独立记号），Expression.l 里的 UnderflowError
                // 分支在本词法中不可达
                throw Base::OverflowError(std::format("整数 '{}' 超出 long long 能表示的范围，请改写成小数或缩小数值（第 {} 列）", raw, column));
            }
            if (result.ptr != raw.data() + raw.size())
            {
                throw Base::ParserError(std::format("整数 '{}' 无法解析，请检查表达式第 {} 列", raw, column));
            }
            return value;
        }

        // —— 常量 ——

        /// 常量匹配结果
        struct ConstantMatch
        {
            std::size_t      length{0};     ///< 匹配到的字节数
            std::string_view canonicalName; ///< 规范名（true 归一为 True）
            double           value{0};      ///< 常量数值
        };

        /// 匹配 Expression.l 的常量规则：pi、e、None、True、true、False、false
        ConstantMatch matchConstant(std::string_view text, const std::size_t offset)
        {
            struct ConstantSpecification
            {
                std::string_view literal;
                std::string_view canonicalName;
                double           value;
            };
            static constexpr ConstantSpecification constantSpecifications[]{
                    {.literal = "pi", .canonicalName = "pi", .value = std::numbers::pi},
                    {.literal = "e", .canonicalName = "e", .value = std::numbers::e},
                    {.literal = "None", .canonicalName = "None", .value = 0},
                    {.literal = "True", .canonicalName = "True", .value = 1},
                    {.literal = "true", .canonicalName = "True", .value = 1},
                    {.literal = "False", .canonicalName = "False", .value = 0},
                    {.literal = "false", .canonicalName = "False", .value = 0},
            };
            ConstantMatch best;
            for (const auto &[literal, canonicalName, value]: constantSpecifications)
            {
                if (literal.size() > best.length && text.size() - offset >= literal.size() &&
                    text.compare(offset, literal.size(), literal) == 0)
                {
                    best = {.length = literal.size(), .canonicalName = canonicalName, .value = value};
                }
            }
            return best;
        }

        // —— 字符串 ——

        /// 字符串匹配结果
        struct StringMatch
        {
            std::size_t length{0};                ///< 含 << 与 >> 的字节数
            bool        documentReference{false}; ///< 内容含 '#'，按 <<文档#单元格>> 跨文档引用对待
            std::string content;                  ///< 去掉定界符并处理转义后的内容
        };

        /// 处理 << >> 内的转义：\n、\t、\r 取字面含义，其余（含 \\、" 与 \'）去掉反斜杠保留原字符
        std::string unescapeStringBody(const std::string_view body)
        {
            std::string result;
            result.reserve(body.size());
            for (std::size_t index = 0; index < body.size(); ++index)
            {
                const char character = body[index];
                if (character != '\\' || index + 1 >= body.size())
                {
                    result.push_back(character);
                    continue;
                }
                switch (body[++index])
                {
                    case 'n':
                        result.push_back('\n');
                        break;
                    case 't':
                        result.push_back('\t');
                        break;
                    case 'r':
                        result.push_back('\r');
                        break;
                    default:
                        result.push_back(body[index]);
                        break;
                }
            }
            return result;
        }

        /// 匹配 <<...>>；<< 之后找不到配对的 >> 时直接报错，避免退化成一串 '<' 记号
        StringMatch matchString(const std::string_view text, const std::size_t offset, const int column)
        {
            if (text.size() - offset < 2 || text.compare(offset, 2, "<<") != 0)
            {
                return {};
            }
            std::size_t position = offset + 2;
            while (position < text.size())
            {
                const char character = text[position];
                if (character == '\\')
                {
                    position += 2; // 反斜杠连同其后一个字符整体属于内容（Expression.l 的 \\(.|\n)）
                    continue;
                }
                if (character == '>')
                {
                    if (position + 1 < text.size() && text[position + 1] == '>')
                    {
                        const std::string_view rawBody           = text.substr(offset + 2, position - offset - 2);
                        const bool             documentReference = rawBody.find('#') != std::string_view::npos;
                        return {.length = position + 2 - offset, .documentReference = documentReference, .content = unescapeStringBody(rawBody)};
                    }
                    break; // 内容里不允许单个 '>'，它不可能是结束符的一部分
                }
                if (character == '\n')
                {
                    break; // 内容里不允许裸换行
                }
                ++position;
            }
            throw Base::ParserError(std::format("字符串 << 没有配对的 >>，请补齐结束符（第 {} 列）", column));
        }

        // —— 单元格地址、函数名、标识符、运算符 ——

        /// 匹配单元格地址（$A$1、A1、$A1）；不是地址时返回 0
        std::size_t matchCellAddress(const std::string_view text, const std::size_t offset)
        {
            std::size_t position       = offset;
            const bool  absoluteColumn = text[position] == '$';
            if (absoluteColumn)
            {
                ++position;
            }
            std::size_t letters = 0;
            while (position < text.size() && isAsciiLetter(text[position]) && letters < 3)
            {
                ++position;
                ++letters;
            }
            if (letters == 0 || letters > 2)
            {
                return 0;
            }
            // Expression.l 只在列号前写了 $ 时才允许行号前也带 $：A$1 不算单元格地址
            if (absoluteColumn && position < text.size() && text[position] == '$')
            {
                ++position;
            }
            const std::size_t digits = scanDecimalDigits(text, position);
            if (digits == 0)
            {
                return 0;
            }
            return position + digits - offset;
        }

        /// 函数名匹配结果
        struct FunctionMatch
        {
            std::size_t      length{0}; ///< 含名字、名字后的空白与左括号
            std::string_view name;      ///< 去掉空白与左括号的函数名
        };

        /// 匹配「函数名 + 可选的空白 + 左括号」（Expression.l 的 FUNC 规则），名字须以字母开头
        FunctionMatch matchFunction(std::string_view text, const std::size_t offset)
        {
            if (!isLetterLike(text[offset]))
            {
                return {};
            }
            std::size_t position = offset + 1;
            while (position < text.size() && characterClasses.functionNameContinue[static_cast<unsigned char>(text[position])] != 0)
            {
                ++position;
            }
            const std::string_view name = text.substr(offset, position - offset);
            while (position < text.size() && (text[position] == ' ' || text[position] == '\t'))
            {
                ++position;
            }
            if (position >= text.size() || text[position] != '(')
            {
                return {};
            }
            // 左括号被函数记号吃掉，与 Expression.l 一致：上层拿到的函数记号后面不再跟 '('
            return {.length = position + 1 - offset, .name = name};
        }

        /// 匹配标识符（Expression.l 的 IDENTIFIER 规则），返回字节数
        std::size_t matchIdentifier(std::string_view text, const std::size_t offset)
        {
            if (characterClasses.identifierStart[static_cast<unsigned char>(text[offset])] == 0)
            {
                return 0;
            }
            if (text[offset] == unicodeMinusSignLeadByte && startsWithUnicodeMinus(text, offset))
            {
                return 0; // U+2212 减号属于运算符，不能被「非 ASCII 一律当字母」的约定吞进标识符
            }
            std::size_t position = offset + 1;
            while (position < text.size() && characterClasses.identifierContinue[static_cast<unsigned char>(text[position])] != 0)
            {
                // 减号的多字节序列要截断扫描，否则它比运算符匹配更长，会把 "1−2" 吞成一个标识符
                if (text[position] == unicodeMinusSignLeadByte && startsWithUnicodeMinus(text, position))
                {
                    break;
                }
                ++position;
            }
            return position - offset;
        }

        /// 运算符与标点匹配结果
        struct OperatorMatch
        {
            std::size_t         length{0};                      ///< 匹配到的字节数；0 表示没有匹配
            ExpressionTokenKind kind{ExpressionTokenKind::End}; ///< 记号类别
        };

        /// 匹配运算符与标点；先看多字节形式，保证 ==、!=、<=、>= 与 U+2212 减号不被拆开
        OperatorMatch matchOperator(const std::string_view text, const std::size_t offset)
        {
            if (text[offset] == unicodeMinusSignLeadByte && startsWithUnicodeMinus(text, offset))
            {
                return {.length = unicodeMinusSign.size(), .kind = ExpressionTokenKind::Minus};
            }
            if (text.size() - offset >= 2)
            {
                const std::string_view pair = text.substr(offset, 2);
                if (pair == "==")
                {
                    return {.length = 2, .kind = ExpressionTokenKind::Equal};
                }
                if (pair == "!=")
                {
                    return {.length = 2, .kind = ExpressionTokenKind::NotEqual};
                }
                if (pair == "<=")
                {
                    return {.length = 2, .kind = ExpressionTokenKind::LessEqual};
                }
                if (pair == ">=")
                {
                    return {.length = 2, .kind = ExpressionTokenKind::GreaterEqual};
                }
            }
            switch (text[offset])
            {
                case '+':
                    return {.length = 1, .kind = ExpressionTokenKind::Plus};
                case '-':
                    return {.length = 1, .kind = ExpressionTokenKind::Minus};
                case '*':
                    return {.length = 1, .kind = ExpressionTokenKind::Star};
                case '/':
                    return {.length = 1, .kind = ExpressionTokenKind::Slash};
                case '%':
                    return {.length = 1, .kind = ExpressionTokenKind::Percent};
                case '^':
                    return {.length = 1, .kind = ExpressionTokenKind::Caret};
                case '=':
                    return {.length = 1, .kind = ExpressionTokenKind::Equal};
                case '<':
                    return {.length = 1, .kind = ExpressionTokenKind::Less};
                case '>':
                    return {.length = 1, .kind = ExpressionTokenKind::Greater};
                case '?':
                    return {.length = 1, .kind = ExpressionTokenKind::Question};
                case ':':
                    return {.length = 1, .kind = ExpressionTokenKind::Colon};
                case ',':
                    return {.length = 1, .kind = ExpressionTokenKind::Comma};
                case ';':
                    return {.length = 1, .kind = ExpressionTokenKind::Semicolon};
                case '(':
                    return {.length = 1, .kind = ExpressionTokenKind::LeftParen};
                case ')':
                    return {.length = 1, .kind = ExpressionTokenKind::RightParen};
                case '[':
                    return {.length = 1, .kind = ExpressionTokenKind::LeftBracket};
                case ']':
                    return {.length = 1, .kind = ExpressionTokenKind::RightBracket};
                case '.':
                    return {.length = 1, .kind = ExpressionTokenKind::Dot};
                default:
                    break;
            }
            return {};
        }
    } // namespace

    ExpressionLexer::ExpressionLexer(const std::string_view text) :
        m_text(text)
    {
    }

    void ExpressionLexer::skipWhitespace()
    {
        while (m_offset < m_text.size())
        {
            const char character = m_text[m_offset];
            if (character == ' ' || character == '\t' || character == '\r')
            {
                // 回车一并跳过，免得 Windows 行尾在报错定位上多出一列
                ++m_offset;
                ++m_column;
            } else if (character == '\n')
            {
                ++m_offset;
                m_column = 1;
            } else
            {
                break;
            }
        }
    }

    std::string_view ExpressionLexer::takeRawText(const std::size_t byteCount)
    {
        const std::string_view rawText = m_text.substr(m_offset, byteCount);
        m_offset                       += byteCount;
        // 列号按 UTF-8 码点推进：续字节不单独计数；换行之后从 1 重新开始
        for (const char character: rawText)
        {
            if (character == '\n')
            {
                m_column = 1;
            } else if ((static_cast<unsigned char>(character) & 0xC0) != 0x80)
            {
                ++m_column;
            }
        }
        return rawText;
    }

    ExpressionToken ExpressionLexer::next()
    {
        skipWhitespace();

        const std::size_t tokenOffset = m_offset;
        const int         tokenColumn = m_column;
        if (m_offset >= m_text.size())
        {
            ExpressionToken endToken;
            endToken.offset = tokenOffset;
            endToken.column = tokenColumn;
            return endToken;
        }

        // 在同一位置并行求出各条规则的匹配长度，取最长匹配；长度相同则按 Expression.l
        // 中规则的先后取舍， 由下面 consider 的调用次序表达。如此 mm 胜过 m、min 胜过同名标识符、sin(
        // 胜过单位 s。
        const StringMatch   stringMatch       = matchString(m_text, m_offset, m_column);
        const OperatorMatch operatorMatch     = matchOperator(m_text, m_offset);
        const UnitMatch     unitMatch         = matchUnitSymbol(m_text, m_offset);
        const NumberMatch   numberMatch       = matchNumber(m_text, m_offset);
        const ConstantMatch constantMatch     = matchConstant(m_text, m_offset);
        const std::size_t   cellAddressLength = matchCellAddress(m_text, m_offset);
        const FunctionMatch functionMatch     = matchFunction(m_text, m_offset);
        const std::size_t   identifierLength  = matchIdentifier(m_text, m_offset);

        ExpressionTokenKind bestKind   = ExpressionTokenKind::End;
        std::size_t         bestLength = 0;
        const auto          consider   = [&bestKind, &bestLength](const ExpressionTokenKind kind, const std::size_t length)
        {
            if (length > bestLength)
            {
                bestKind   = kind;
                bestLength = length;
            }
        };
        const ExpressionTokenKind stringKind = stringMatch.documentReference ? ExpressionTokenKind::DocumentRef : ExpressionTokenKind::String;
        consider(stringKind, stringMatch.length);
        consider(operatorMatch.kind, operatorMatch.length);
        consider(unitMatch.kind, unitMatch.length);
        consider(numberMatch.kind, numberMatch.length);
        consider(ExpressionTokenKind::Constant, constantMatch.length);
        consider(ExpressionTokenKind::CellAddress, cellAddressLength);
        consider(ExpressionTokenKind::Function, functionMatch.length);
        consider(ExpressionTokenKind::Identifier, identifierLength);

        if (bestLength == 0)
        {
            // '#'、'@'、'{'、'}'
            // 在原词法里是独立记号，但本记号集没有对应类别；单独出现时给出明确提示， 其中 '@'
            // 仍可出现在标识符内部（见 isIdentifierContinue）
            const std::string_view characterText = currentCharacterView(m_text, m_offset);
            if (characterText == "#" || characterText == "@" || characterText == "{" || characterText == "}")
            {
                throw Base::ParserError(std::format("字符 '{}' 不能单独出现在这里，请检查表达式第 {} 列", characterText, tokenColumn));
            }
            throw Base::ParserError(std::format("无法识别的字符 '{}'，请检查表达式第 {} 列", characterText, tokenColumn));
        }

        ExpressionToken token;
        token.kind   = bestKind;
        token.offset = tokenOffset;
        token.column = tokenColumn;

        const std::string_view rawText = takeRawText(bestLength);
        token.text.assign(rawText); // 默认就是原文：运算符、单位、标识符与单元格地址
        switch (bestKind)
        {
            case ExpressionTokenKind::Number:
                token.numberValue = parseNumberValue(rawText, tokenColumn);
                break;
            case ExpressionTokenKind::Integer:
            {
                const long long value = parseIntegerValue(rawText, tokenColumn);
                token.numberValue     = static_cast<double>(value);
                // 记号里的整数字段是 int；更大的整数由 numberValue 携带，越界已在上面按 long long 校过
                token.integerValue    = static_cast<int>(value);
                break;
            }
            case ExpressionTokenKind::String:
            case ExpressionTokenKind::DocumentRef:
                token.text = stringMatch.content;
                break;
            case ExpressionTokenKind::Function:
                token.text.assign(functionMatch.name);
                break;
            case ExpressionTokenKind::Constant:
                token.text.assign(constantMatch.canonicalName);
                token.numberValue = constantMatch.value;
                break;
            default:
                break;
        }
        return token;
    }
} // namespace ExpressionEngine::Expression
