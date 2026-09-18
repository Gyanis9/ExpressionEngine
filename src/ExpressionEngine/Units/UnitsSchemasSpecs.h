/**
 * @file UnitsSchemasSpecs.h
 * @brief 单位方案的数据结构定义
 * @author Gyanis
 * @date 2026-09-18
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ExpressionEngine::Units
{
    /**
     * @brief 单个单位的换算与显示规则
     * @details 适用规则：在一组候选里取第一个「阈值大于待换算值」的条目；阈值为 0 表示兜底条目；
     *          换算因子为 0 表示 unitString 里写的是要调用的特殊函数名而不是单位串。
     */
    struct UnitTranslationSpec
    {
        double                             threshold{1}; ///< 适用阈值
        std::string                        unitString;   ///< 目标单位串，或特殊函数名
        double                             factor{1};    ///< 从基准单位到该单位的换算因子
        std::function<std::string(double)> callback;     ///< 特殊函数为 0 时使用的自定义转换
    };

    /// 一个单位方案（如 "Internal"、"ImperialDecimal"）的完整定义
    struct UnitsSchemaSpec
    {
        std::size_t num;                      ///< 方案编号
        std::string name;                     ///< 方案名
        std::string basicLengthUnitString;    ///< 基准长度单位
        bool        isMultiUnitLength{false}; ///< 长度是否用多个单位复合表示
        bool        isMultiUnitAngle{false};  ///< 角度是否用多个单位复合表示
        const char *description{nullptr};     ///< 方案描述
        bool        isDefault{false};         ///< 是否为默认方案

        /// 按单位类型名索引的换算规则集合
        std::map<std::string, std::vector<UnitTranslationSpec>> translationSpecs;
    };

    /// 全部方案的打包数据，宿主可整体替换以自定义单位显示规则
    struct UnitsSchemasDataPack
    {
        std::vector<UnitsSchemaSpec> specs;          ///< 方案列表
        std::size_t                  defDecimals;    ///< 默认小数位数
        std::size_t                  defDenominator; ///< 默认分数分母
    };
} // namespace ExpressionEngine::Units
