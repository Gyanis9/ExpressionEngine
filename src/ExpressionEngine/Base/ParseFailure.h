/**
 * @file ParseFailure.h
 * @brief 文本解析失败的可恢复错误
 * @author Gyanis
 * @date 2026-09-19
 * @version 0.0.1
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <optional>
#include <string>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Base
{
    /**
     * @brief 可恢复的解析失败
     * @details 供 std::expected 通道使用：输入文本非法属于可恢复错误，调用方拿到它即可
     *          分支或降级，无需 try/catch。异常通道（ParserError）与它承载同一份文案与同一个
     *          kind，两者只差传递方式，调用方按需要选用。
     */
    struct ParseFailure
    {
        std::string        message;                   ///< 中文可操作文案，带位置时已含列号
        std::optional<int> column;                    ///< 出错列，1 起，按 UTF-8 码点计数；只有文案以「表达式第 N 列」定位的报错才有值，
                                                       ///< 空文本、层数超限、词法期整数溢出这些没有前缀定位的留空
        ErrorKind          kind = ErrorKind::Other;   ///< 故障类别，取自抛出的那个异常对象；宿主自己构造时留 Other
    };

} // namespace ExpressionEngine::Base
