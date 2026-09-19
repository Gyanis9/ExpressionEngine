/**
 * @file ExpressionParser.h
 * @brief 表达式文本解析器
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <expected>
#include <string_view>

#include <ExpressionEngine/Base/ParseFailure.h>
#include <ExpressionEngine/Expression/Expression.h>
#include <ExpressionEngine/Expression/PropertyModel.h>

namespace ExpressionEngine::Expression
{

    /**
     * @brief 表达式文本解析器
     * @details 手写词法分析（ExpressionLexer）加 Pratt 语法分析，替代原先的 flex + bison 生成
     *          代码：绑定功率表与原文法的优先级声明一一对应，报错位置直接来自输入偏移，
     *          且不再需要代码生成步骤。
     */
    class ExpressionParser
    {
    public:
        /**
         * @brief 解析表达式文本
         * @param resolver 对象解析器，可为空；为空时变量引用仍能解析出结构，求值时才会报错
         * @param text 待解析文本，可为空（空文本报错：表达式不能为空）
         * @return 解析出的表达式，调用方持有所有权
         * @throws Base::ParserError 词法或语法错误，消息中带出错列号
         */
        [[nodiscard]] static ExpressionPtr parse(IObjectResolver *resolver, std::string_view text);

        /**
         * @brief 解析表达式文本，失败时以值返回错误
         * @details 与 parse() 同语义但不抛异常：输入非法属可恢复错误，调用方拿到
         *          ParseFailure 即可分支或降级，无需 try/catch；文案与异常通道一致。
         * @param resolver 对象解析器，可为空；为空时变量引用仍能解析出结构，求值时才会报错
         * @param text 待解析文本，可为空（空文本报错：表达式不能为空）
         * @return 成功返回解析结果；失败返回 ParseFailure，其 message 为中文原因与出错列号
         */
        [[nodiscard]] static std::expected<ExpressionPtr, Base::ParseFailure> tryParse(IObjectResolver *resolver, std::string_view text);
    };

} // namespace ExpressionEngine::Expression
