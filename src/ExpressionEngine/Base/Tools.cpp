#include <ExpressionEngine/Base/Tools.h>

#include <algorithm>
#include <cstdint>
#include <format>

namespace ExpressionEngine::Base::Tools
{
    namespace
    {
        /// 按 UTF-8 前导字节判断序列总长度；非法前导字节返回 0
        [[nodiscard]] std::size_t utf8SequenceLength(const unsigned char leadByte) noexcept
        {
            if (leadByte < 0x80)
            {
                return 1;
            }
            if ((leadByte & 0xE0) == 0xC0)
            {
                return 2;
            }
            if ((leadByte & 0xF0) == 0xE0)
            {
                return 3;
            }
            if ((leadByte & 0xF8) == 0xF0)
            {
                return 4;
            }
            return 0;
        }

        /// 把单个控制字符写成转义文本；\n、\r、\t 用惯用转义，其余控制码写成 \uXXXX
        [[nodiscard]] std::string escapeControlCharacter(const unsigned char byte)
        {
            switch (byte)
            {
                case '\n':
                    return "\\n";
                case '\r':
                    return "\\r";
                case '\t':
                    return "\\t";
                default:
                    return std::format("\\u{:04X}", static_cast<std::uint32_t>(byte));
            }
        }

        /**
         * @brief 从 offset 起的一个字符占多少字节
         * @details 计数与定位共用这一条规则：非法前导字节（含孤立的续字节）按单字节成字，
         *          尾部被截断的序列按剩余字节数取，因此两者数出来的字符个数不可能不一致。
         * @param text 待切分文本，可含任意字节
         * @param offset 起始字节偏移，须小于文本长度
         * @return 字节长度，至少为 1 且不超过剩余字节数
         */
        [[nodiscard]] std::size_t characterSpanLength(const std::string_view text, const std::size_t offset)
        {
            const std::size_t sequenceLength = utf8SequenceLength(static_cast<unsigned char>(text[offset]));
            const std::size_t remaining      = text.size() - offset;

            return sequenceLength == 0 ? 1 : std::min(sequenceLength, remaining);
        }
    } // namespace

    std::string escapeQuotesFromString(const std::string &text)
    {
        std::string result;
        result.reserve(text.size());

        // 单双引号是表达式文本的定界符，出现时前置反斜杠；其余字节原样保留
        for (const char character: text)
        {
            switch (character)
            {
                case '\"':
                    result += "\\\"";
                    break;
                case '\'':
                    result += "\\\'";
                    break;
                default:
                    result += character;
                    break;
            }
        }

        return result;
    }

    std::string escapedUnicodeFromUtf8(const char *text)
    {
        std::string result;
        const auto *current = reinterpret_cast<const unsigned char *>(text);

        while (*current != 0)
        {
            const std::size_t sequenceLength = utf8SequenceLength(*current);

            // 非法前导字节与截断序列都按单字节原样输出：宁可留下可疑字节，也不静默丢弃数据
            if (sequenceLength == 0 || sequenceLength == 1)
            {
                if (*current < 0x20 || *current == 0x7F)
                {
                    result += escapeControlCharacter(*current);
                } else if (*current == '\\')
                {
                    result += "\\\\";
                } else
                {
                    result += static_cast<char>(*current);
                }
                ++current;
                continue;
            }

            std::uint32_t codePoint  = 0;
            bool          isComplete = true;
            for (std::size_t index = 0; index < sequenceLength; ++index)
            {
                const unsigned char byte = current[index];
                if (byte == 0 || (index > 0 && (byte & 0xC0) != 0x80))
                {
                    isComplete = false;
                    break;
                }
                codePoint = index == 0 ? (byte & (0xFFu >> (sequenceLength + 1))) : ((codePoint << 6) | (byte & 0x3Fu));
            }
            if (!isComplete)
            {
                result += static_cast<char>(*current);
                ++current;
                continue;
            }

            // 基本多文种平面之外写成 UTF-16 代理对，与 Python 的 \uXXXX 转义保持一致
            if (codePoint > 0xFFFF)
            {
                const std::uint32_t offset = codePoint - 0x10000u;
                result                     += std::format("\\u{:04X}\\u{:04X}", 0xD800u + (offset >> 10), 0xDC00u + (offset & 0x3FFu));
            } else
            {
                result += std::format("\\u{:04X}", codePoint);
            }
            current += sequenceLength;
        }

        return result;
    }

    std::size_t countUtf8Characters(const std::string_view text)
    {
        // 走与 locateUtf8Character 相同的切分规则，逐字前进直到走完文本
        std::size_t count  = 0;
        std::size_t offset = 0;
        while (offset < text.size())
        {
            offset += characterSpanLength(text, offset);
            ++count;
        }

        return count;
    }

    CharacterSpan locateUtf8Character(const std::string_view text, const std::size_t index)
    {
        std::size_t characterIndex = 0;
        std::size_t offset         = 0;
        while (offset < text.size())
        {
            if (characterIndex == index)
            {
                return CharacterSpan{.offset = offset, .length = characterSpanLength(text, offset)};
            }
            offset += characterSpanLength(text, offset);
            ++characterIndex;
        }
        return CharacterSpan{.offset = text.size(), .length = 0};
    }
} // namespace ExpressionEngine::Base::Tools
