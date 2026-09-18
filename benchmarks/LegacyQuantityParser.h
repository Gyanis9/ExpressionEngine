/**
 * @file LegacyQuantityParser.h
 * @brief FreeCAD flex/bison 生成代码的对比入口（仅基准使用）
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cstdint>
#include <string>

namespace ExpressionEngine::Benchmarks
{
    /**
     * @brief 用 FreeCAD 的 flex 扫描器与 bison 解析器解析一条数量文本
     * @details 语义与 QuantityParser::parse 对齐：成功时返回结果数值的位模式，失败时抛 ParserError。
     *          实现在 LegacyQuantityParser.cpp 里，该文件把 Quantity.tab.c 与 Quantity.lex.c
     *          原样包含进来，并补齐它们要求的宿主环境（Quantity 门面、QuantResult、
     *          num_change、Quantity_yyerror、yylex）。生成代码不随仓库分发，
     *          由 CMake 变量 EXPRESSIONENGINE_LEGACY_QUANTITY_DIR 指向本机目录。
     * @param text 数量文本，如 "1.5 mm"、"5' 6\""、"2 m/s"
     * @return 解析结果数值的位模式，供基准的防优化汇聚点使用
     */
    std::uintptr_t parseLegacyQuantity(const std::string &text);
} // namespace ExpressionEngine::Benchmarks
