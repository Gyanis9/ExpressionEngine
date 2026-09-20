/**
 * @file ParseFailure.h
 * @brief 文本解析失败的可恢复错误
 * @author Gyanis
 * @date 2026-09-19
 * @version 0.0.1
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <string>

namespace ExpressionEngine::Base
{
    /**
     * @brief 可恢复的解析失败
     * @details 供 std::expected 通道使用：输入文本非法属于可恢复错误，调用方拿到它即可
     *          分支或降级，无需 try/catch。异常通道（ParserError）与它承载同一份文案，
     *          两者只差传递方式，调用方按需要选用。
     */
    struct ParseFailure
    {
        std::string message; ///< 中文可操作文案，已含出错位置（列号）
    };

} // namespace ExpressionEngine::Base
