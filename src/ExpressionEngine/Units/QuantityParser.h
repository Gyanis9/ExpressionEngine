/**
 * @file QuantityParser.h
 * @brief 数量文本解析器
 * @author Gyanis
 * @date 2026-09-18
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <string>
#include <string_view>

#include <ExpressionEngine/Units/Quantity.h>

namespace ExpressionEngine::Units {
/**
 * @brief 解析数量文本的手写解析器
 * @details 用一遍扫描的词法分析与递归下降替代原先的 flex + bison 生成代码：没有生成步骤、
 *          没有 DFA 表跳转，报错位置直接来自输入偏移。
 *          支持的写法与原文法一致：数字与单位、单位乘除与幂、标量函数、括号、
 *          方括号注释，以及最多三段相邻数量求和（如 5' 6"）。
 */
class QuantityParser {
public:
    /**
     * @brief 解析数量文本
     * @param text 待解析文本，可为空（空输入得到「最小正数」量，与原文法一致）
     * @return 解析结果
     * @throws ParserError 文本存在词法或语法错误，消息中带出错位置
     */
    [[nodiscard]] static Quantity parse(std::string_view text);
};
}  // namespace ExpressionEngine::Units
