// 本文件覆盖文本转义工具的边界：引号、控制字符、多字节与非 ASCII 码点。

#include <gtest/gtest.h>

#include <string>

#include <ExpressionEngine/Base/Tools.h>

namespace ExpressionEngine::Base::Tools
{
    namespace
    {

        /**
         * @brief 钉住：只转义单双引号，其余字节（含中文与反斜杠）原样保留
         */
        TEST(ToolsTest, EscapesQuotesOnly)
        {
            EXPECT_EQ(escapeQuotesFromString("a\"b'c"), "a\\\"b\\'c");
            EXPECT_EQ(escapeQuotesFromString("中文 1.5 mm"), "中文 1.5 mm");
            EXPECT_EQ(escapeQuotesFromString("back\\slash"), "back\\slash");
            EXPECT_EQ(escapeQuotesFromString(""), "");
        }

        /**
         * @brief 钉住：可打印 ASCII 原样输出，反斜杠与控制字符按常规转义
         */
        TEST(ToolsTest, EscapesControlCharactersAndBackslash)
        {
            EXPECT_EQ(escapedUnicodeFromUtf8("abc123"), "abc123");
            EXPECT_EQ(escapedUnicodeFromUtf8("line\nbreak"), "line\\nbreak");
            EXPECT_EQ(escapedUnicodeFromUtf8("tab\there"), "tab\\there");
            EXPECT_EQ(escapedUnicodeFromUtf8("back\\slash"), "back\\\\slash");
        }

        /**
         * @brief 钉住：非 ASCII 码点写成 \uXXXX，基本多文种平面之外写成代理对
         */
        TEST(ToolsTest, EscapesNonAsciiCodePoints)
        {
            // U+4E2D 中文「中」
            EXPECT_EQ(escapedUnicodeFromUtf8("中"), "\\u4E2D");
            // U+00B5 微符号，单位符号 µm 里用到
            EXPECT_EQ(escapedUnicodeFromUtf8("µ"), "\\u00B5");
            // U+1F600 超出基本多文种平面
            EXPECT_EQ(escapedUnicodeFromUtf8("😀"), "\\uD83D\\uDE00");
        }

        /**
         * @brief 钉住：非法 UTF-8 序列按单字节原样保留，不静默丢弃数据
         */
        TEST(ToolsTest, KeepsMalformedBytesIntact)
        {
            const std::string malformed{'a', static_cast<char>(0xFF), 'b'};
            const std::string escaped = escapedUnicodeFromUtf8(malformed.c_str());

            EXPECT_NE(escaped.find('a'), std::string::npos);
            EXPECT_NE(escaped.find(static_cast<char>(0xFF)), std::string::npos);
            EXPECT_NE(escaped.find('b'), std::string::npos);
        }

        /**
         * @brief 钉住：UTF-8 字符计数与定位；截断与越界都不越界读
         */
        TEST(ToolsTest, CountsAndLocatesUtf8Characters)
        {
            EXPECT_EQ(countUtf8Characters(""), 0U);
            EXPECT_EQ(countUtf8Characters("abc"), 3U);
            EXPECT_EQ(countUtf8Characters("中文 a"), 4U);
            EXPECT_EQ(countUtf8Characters(std::string("\xF0\x9F\x98\x80x", 5)), 2U); // 一个四字节省符加一个 ASCII

            EXPECT_EQ(locateUtf8Character("abc", 0).offset, 0U);
            EXPECT_EQ(locateUtf8Character("abc", 2).length, 1U);
            EXPECT_EQ(locateUtf8Character("中文", 1).offset, 3U); // 第二个汉字的字节偏移
            EXPECT_EQ(locateUtf8Character("中文", 1).length, 3U);

            // 下标超出：偏移落在文本末尾、长度为零，由调用方按越界处理
            const CharacterSpan beyond = locateUtf8Character("abc", 7);
            EXPECT_EQ(beyond.offset, 3U);
            EXPECT_EQ(beyond.length, 0U);

            // 尾部被截断的多字节序列按剩余字节数取，不会越界读
            const CharacterSpan truncated = locateUtf8Character(std::string("a\xE4\xB8", 3), 1);
            EXPECT_EQ(truncated.offset, 1U);
            EXPECT_EQ(truncated.length, 2U);
        }

        /**
         * @brief 钉住：计数与定位对同一份文本给出同一套切分
         * @details 宿主文本可能混进孤立的续字节（来自外部数据）。两处规则一旦不一致，「字符数」
         *          就会少算，负下标据此起算会取到别的字符而不是末字符。
         */
        TEST(ToolsTest, CountAndLocateAgreeOnMalformedBytes)
        {
            const std::string strayContinuation("\x80" "A", 2);
            EXPECT_EQ(countUtf8Characters(strayContinuation), 2U);
            EXPECT_EQ(locateUtf8Character(strayContinuation, 1).offset, 1U);
            EXPECT_EQ(locateUtf8Character(strayContinuation, 1).length, 1U);

            for (const std::string_view sample: {std::string_view{"abc"},
                                                 std::string_view{"\x80" "A", 2},
                                                 std::string_view{"a\xE4\xB8", 3},
                                                 std::string_view{"\xF0\x9F\x98", 3},
                                                 std::string_view{"\xC2" " A", 3}})
            {
                const std::size_t characters = countUtf8Characters(sample);

                // 逐字走完必须正好铺满文本：既不多切也不漏切
                std::size_t covered = 0;
                for (std::size_t index = 0; index < characters; ++index)
                {
                    const CharacterSpan span = locateUtf8Character(sample, index);
                    ASSERT_GT(span.length, 0U) << "第 " << index << " 个字符长度为零";
                    ASSERT_LE(span.offset + span.length, sample.size()) << "切分越出了文本末尾";
                    covered += span.length;
                }

                EXPECT_EQ(covered, sample.size()) << "切分没有铺满文本";
                EXPECT_EQ(locateUtf8Character(sample, characters).length, 0U) << "多出一个可定位的下标";
            }
        }

    } // namespace
} // namespace ExpressionEngine::Base::Tools
