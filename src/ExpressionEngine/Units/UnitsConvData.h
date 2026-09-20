/**
 * @file UnitsConvData.h
 * @brief 英制与工程单位的换算常量
 * @author Gyanis
 * @date 2026-09-19
 * @version 0.0.1
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

namespace ExpressionEngine::Units::UnitsConvData
{
    // 基准量纲为 mm / kg / s，下列常量都是从基准量纲换算到对应单位所需的比例
    constexpr auto inch{25.4};                             ///< 英寸
    constexpr auto foot{12 * inch};                        ///< 英尺
    constexpr auto yard{3 * foot};                         ///< 码
    constexpr auto mile{1760 * yard};                      ///< 英里
    constexpr auto pound{0.45359237};                      ///< 磅
    constexpr auto poundForce{9.80665 * pound};            ///< 磅力
    constexpr auto psi{poundForce / (inch * inch) * 1000}; ///< 磅力每平方英寸
}
