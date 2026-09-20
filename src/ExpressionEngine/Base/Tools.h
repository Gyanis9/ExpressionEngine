/**
 * @file Tools.h
 * @brief 文本与编码辅助函数
 * @author Gyanis
 * @date 2026-09-19
 * @version 0.0.1
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ExpressionEngine::Base::Tools
{
    /**
     * @brief 转义文本中的单双引号
     * @details 用于把值安全嵌入以引号作定界符的表达式文本，反斜杠本身不在此处处理。
     * @param text 待转义文本，可含任意字节
     * @return 转义后的新文本
     */
    [[nodiscard]] std::string escapeQuotesFromString(const std::string &text);

    /**
     * @brief 把 UTF-8 文本转义为 Python 风格的转义序列
     * @details 非 ASCII 码点写成 \uXXXX（基本多文种平面之外写成代理对），反斜杠与控制字符按
     *          \n、\t 等常规转义输出，其余可打印 ASCII 原样保留。
     * @param text 以 NUL 结尾的 UTF-8 文本
     * @return 全 ASCII 的转义文本；输入含非法 UTF-8 序列时按单字节原样保留，不静默丢弃
     */
    [[nodiscard]] std::string escapedUnicodeFromUtf8(const char *text);

    /**
     * @brief 一个 UTF-8 字符在字节序列里的位置
     */
    struct CharacterSpan
    {
        std::size_t offset; ///< 起始字节偏移
        std::size_t length; ///< 字节长度；下标越界时为 0
    };

    /**
     * @brief 统计文本里的 UTF-8 字符个数
     * @details 从前往后切分：合法前导字节按序列长度成字，非法前导字节（含没有归属的孤立续字节）
     *          与尾部被截断的序列各算一个字符。规则与 locateUtf8Character 共用同一份实现，
     *          因此「个数」与「按下标定位」不可能对不上。
     * @param text 待统计文本，可含任意字节
     * @return 字符个数
     */
    [[nodiscard]] std::size_t countUtf8Characters(std::string_view text);

    /**
     * @brief 定位文本里第 index 个 UTF-8 字符
     * @details 尾部被截断时按剩余字节数取，保证 substr 不越界；非法字节序列不会越界读，
     *          孤立的续字节自成一个单字节字符。
     * @param text 待定位文本，可含任意字节
     * @param index 字符下标，0 起；允许等于字符个数（指到文本末尾之后）
     * @return 该字符的起始偏移与字节长度；下标超出时 length 为 0、offset 为文本长度
     */
    [[nodiscard]] CharacterSpan locateUtf8Character(std::string_view text, std::size_t index);
} // namespace ExpressionEngine::Base::Tools
