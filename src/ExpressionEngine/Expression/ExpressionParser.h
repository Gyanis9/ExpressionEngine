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
#include <ExpressionEngine/Expression/FunctionRegistry.h>
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
         * @details 内置函数表查不到的函数名，转向进程级默认注册表 FunctionRegistry::global()。
         * @param resolver 对象解析器，可为空；为空时变量引用仍能解析出结构，求值时才会报错
         * @param text 待解析文本，可为空（空文本报错：表达式不能为空）
         * @return 解析出的表达式，调用方持有所有权
         * @throws Base::ParserError 词法或语法错误，消息中带出错列号
         */
        [[nodiscard]] static ExpressionPtr parse(IObjectResolver *resolver, std::string_view text);

        /**
         * @brief 解析表达式文本，函数名在指定注册表里查询
         * @details 供宿主隔离自己的函数集（插件、用例）：不查默认注册表，因此同名函数在
         *          不同注册表下可有不同实现，表达式文本仍然一致。
         * @param resolver 对象解析器，可为空
         * @param text 待解析文本
         * @param registry 自定义函数注册表；内置函数仍优先，注册表只兜住其余名字
         * @return 解析出的表达式，调用方持有所有权
         * @throws Base::ParserError 词法或语法错误，或函数名两边都查不到
         */
        [[nodiscard]] static ExpressionPtr parse(IObjectResolver *resolver, std::string_view text, const FunctionRegistry &registry);

        /**
         * @brief 解析表达式文本，失败时以值返回错误
         * @details 与 parse() 同语义但不抛异常：输入非法属可恢复错误，调用方拿到
         *          ParseFailure 即可分支或降级，无需 try/catch；文案与异常通道一致。
         *          捕获范围是建树期抛出的 Base::Exception —— 除词法语法错之外，函数参数个数
         *          不符、函数不可用这类同样只靠改文本解决的故障也走值返回。
         * @param resolver 对象解析器，可为空；为空时变量引用仍能解析出结构，求值时才会报错
         * @param text 待解析文本，可为空（空文本报错：表达式不能为空）
         * @return 成功返回解析结果；失败返回 ParseFailure，其 message 为中文原因与出错列号
         */
        [[nodiscard]] static std::expected<ExpressionPtr, Base::ParseFailure> tryParse(IObjectResolver *resolver, std::string_view text);

        /**
         * @brief 解析表达式文本，函数名在指定注册表里查询，失败时以值返回错误
         * @param resolver 对象解析器，可为空
         * @param text 待解析文本
         * @param registry 自定义函数注册表
         * @return 成功返回解析结果；失败返回 ParseFailure，其 message 与异常通道逐字一致
         */
        [[nodiscard]] static std::expected<ExpressionPtr, Base::ParseFailure> tryParse(IObjectResolver *resolver, std::string_view text, const FunctionRegistry &registry);
    };

} // namespace ExpressionEngine::Expression
