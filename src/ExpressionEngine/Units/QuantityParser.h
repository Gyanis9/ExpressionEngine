/**
 * @file QuantityParser.h
 * @brief 数量文本解析器
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <expected>
#include <string_view>

#include <ExpressionEngine/Base/ParseFailure.h>
#include <ExpressionEngine/Units/Quantity.h>

namespace ExpressionEngine::Units
{
    /**
     * @brief 解析数量文本的手写解析器
     * @details 用一遍扫描的词法分析与递归下降替代原先的 flex + bison 生成代码：没有生成步骤、
     *          没有 DFA 表跳转，报错位置直接来自输入偏移。
     *          支持的写法与原文法一致：数字与单位、单位乘除与幂、标量函数、括号、
     *          方括号注释，以及最多三段相邻数量求和（如 5' 6"）。
     */
    class QuantityParser
    {
    public:
        /**
         * @brief 解析数量文本
         * @param text 待解析文本，可为空（空输入得到「最小正数」量，与原文法一致）
         * @return 解析结果
         * @throws ParserError 文本存在词法或语法错误，消息中带出错位置
         */
        [[nodiscard]] static Quantity parse(std::string_view text);

        /**
         * @brief 解析数量文本，失败时以值返回错误
         * @details 与 parse() 同语义但不抛异常：输入非法属可恢复错误，调用方拿到
         *          ParseFailure 即可分支或降级，无需 try/catch；文案与异常通道一致。
         * @param text 待解析文本，可为空（空输入得到「最小正数」量，与原文法一致）
         * @return 成功返回解析结果；失败返回 ParseFailure，其 message 为中文原因与替代做法
         */
        [[nodiscard]] static std::expected<Quantity, Base::ParseFailure> tryParse(std::string_view text);
    };

    /**
     * @brief 按符号查预定义单位量
     * @details 单位符号表只在本模块维护一份，表达式解析器等使用方按符号查表即可，不必各自
     *          抄录；`"` 与 `'` 分别对应英寸与英尺，`°`、`′`、`″` 对应角度符号。
     * @param symbol 单位符号，如 "mm"、"kg"、"in"
     * @return 指向预定义量的指针（静态存储期，无需释放）；符号未登记时返回 nullptr
     */
    [[nodiscard]] const Quantity *findPredefinedUnit(std::string_view symbol);
} // namespace ExpressionEngine::Units
