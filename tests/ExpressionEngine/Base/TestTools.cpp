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

    } // namespace
} // namespace ExpressionEngine::Base::Tools
