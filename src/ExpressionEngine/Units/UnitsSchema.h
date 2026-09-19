/**
 * @file UnitsSchema.h
 * @brief 单个单位方案：把量换算成该方案偏好的单位并排版
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later；出处与上游版权见 NOTICE
 */

#pragma once

#include <string>

#include <ExpressionEngine/Base/NumericFormatting.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/UnitsSchemasSpecifications.h>

namespace ExpressionEngine::Units
{
    /**
     * @brief 一个单位方案的运行时对象
     * @details 方案数据由 UnitsSchemaSpecification 提供，本类只负责按方案把量换算到目标单位并排版；
     *          宿主可自行构造方案数据，因此不需要联网或内置区域数据库。
     */
    class UnitsSchema
    {
    public:
        /**
         * @brief 以方案数据构造
         * @param specification 方案定义，构造后由本对象持有
         */
        explicit UnitsSchema(UnitsSchemaSpecification specification);

        UnitsSchema() = delete;

        /// 长度是否用多单位复合表示（如英尺+英寸）
        [[nodiscard]] bool isMultiUnitLength() const;

        /// 角度是否用多单位复合表示（如度+分+秒）
        [[nodiscard]] bool isMultiUnitAngle() const;

        /// 取方案的基准长度单位串
        [[nodiscard]] std::string getBasicLengthUnit() const;

        /// 取方案名
        [[nodiscard]] std::string getName() const;

        /// 取方案描述
        [[nodiscard]] std::string getDescription() const;

        /// 取方案编号
        [[nodiscard]] int getNumber() const;

        /**
         * @brief 按当前发布的区域上下文换算并排版
         * @param quant 待换算的量
         * @return 形如 "25.4 mm" 的文本
         */
        [[nodiscard]] std::string translate(const Quantity &quant) const;

        /**
         * @brief 按当前发布的区域上下文换算，并回传换算结果
         * @param quant 待换算的量
         * @param factor 输出参数，实际使用的换算因子
         * @param unitString 输出参数，实际使用的单位串
         * @return 形如 "1.5 in" 的文本
         */
        [[nodiscard]] std::string translate(const Quantity &quant, double &factor, std::string &unitString) const;

        /**
         * @brief 按指定区域上下文换算，并回传换算结果
         * @param quant 待换算的量
         * @param formatting 区域快照
         * @param factor 输出参数，实际使用的换算因子
         * @param unitString 输出参数，实际使用的单位串
         * @return 排版后的文本
         * @throws ExpressionError 方案里没有匹配的换算条目且缺少阈值 0 的兜底条目
         */
        [[nodiscard]] std::string translate(const Quantity &quant, const Base::NumericLocaleContext &formatting, double &factor, std::string &unitString) const;

    private:
        /**
         * @brief 按区域上下文把数值与单位拼成最终文本
         * @param quant 待排版的量
         * @param formatting 区域快照
         * @param factor 换算因子
         * @param unitString 单位串
         * @return 数值与单位之间按需插入空格后的文本
         */
        [[nodiscard]] static std::string toLocale(const Quantity &quant, const Base::NumericLocaleContext &formatting, double factor, const std::string &unitString);

        UnitsSchemaSpecification m_specification; ///< 方案定义
    };
} // namespace ExpressionEngine::Units
