/**
 * @file UnitsConvData.h
 * @brief 英制与工程单位的换算常量
 * @author Gyanis
 * @date 2026-09-18
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

namespace ExpressionEngine::Units::UnitsConvData {
// 基准量纲为 mm / kg / s，下列常量都是从基准量纲换算到对应单位所需的比例
constexpr auto inch{25.4};
constexpr auto foot{12 * inch};
constexpr auto yard{3 * foot};
constexpr auto mile{1760 * yard};
constexpr auto pound{0.45359237};
constexpr auto poundForce{9.80665 * pound};
constexpr auto psi{poundForce / (inch * inch) * 1000};
}  // namespace ExpressionEngine::Units::UnitsConvData
