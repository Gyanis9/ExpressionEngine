/**
 * @file Tools.h
 * @brief 文本与编码辅助函数
 * @author Gyanis
 * @date 2026-09-18
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <string>

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
} // namespace ExpressionEngine::Base::Tools
