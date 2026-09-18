/**
 * @file UnitsSchemas.h
 * @brief 单位方案集合与当前方案的切换
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <ExpressionEngine/Units/UnitsSchema.h>
#include <ExpressionEngine/Units/UnitsSchemasSpecifications.h>

namespace ExpressionEngine::Units
{
    /**
     * @brief 全部单位方案的集合，并持有当前生效的方案
     * @details 方案数据整体传入，集合内部按名或按序号查找；当前方案以独占指针持有，
     *          切换方案会重建方案对象，因此运行中修改数据不会影响已取出的方案对象。
     */
    class UnitsSchemas
    {
    public:
        /**
         * @brief 以方案数据包构造
         * @param pack 方案数据包，包含方案列表与默认小数位数、分数分母
         */
        explicit UnitsSchemas(const UnitsSchemasDataPack &pack);

        /// 选中默认方案
        void select();

        /**
         * @brief 按名选中方案
         * @param name 方案名
         * @throws NameError 找不到该名称的方案
         */
        void select(std::string_view name);

        /**
         * @brief 按序号选中方案
         * @param schemaNumber 方案编号
         * @throws IndexError 找不到该编号的方案
         */
        void select(std::size_t schemaNumber);

        /// 取默认方案（标记 isDefault 的方案，没有标记时取第一个）
        [[nodiscard]] UnitsSchemaSpecification specification();

        /**
         * @brief 按名取方案
         * @param name 方案名
         * @return 方案定义
         * @throws NameError 找不到该名称的方案
         */
        [[nodiscard]] UnitsSchemaSpecification specification(std::string_view name);

        /**
         * @brief 按序号取方案
         * @param schemaNumber 方案编号
         * @return 方案定义
         * @throws IndexError 找不到该编号的方案
         */
        [[nodiscard]] UnitsSchemaSpecification specification(std::size_t schemaNumber);

        /// 取方案总数
        [[nodiscard]] std::size_t count() const;

        /// 取全部方案名
        [[nodiscard]] std::vector<std::string> names();

        /// 取全部方案描述
        [[nodiscard]] std::vector<std::string> descriptions();

        /// 取默认小数位数
        [[nodiscard]] std::size_t getDecimals() const;

        /// 取默认分数分母
        [[nodiscard]] std::size_t defaultFractionDenominator() const;

        /// 设置默认分数分母
        void setDefaultFractionDenominator(std::size_t denominator);

        /// 取当前生效的方案对象
        [[nodiscard]] UnitsSchema *currentSchema() const;

    private:
        /// 把各方案按取值函数投影成字符串列表，供名字与描述两处复用
        [[nodiscard]] std::vector<std::string> collect(const std::function<std::string(UnitsSchemaSpecification)> &projector);

        /// 按判定函数查找第一个匹配的方案
        [[nodiscard]] UnitsSchemaSpecification findSpecification(const std::function<bool(UnitsSchemaSpecification)> &predicate);

        /// 把方案设为当前方案
        void makeCurrent(const UnitsSchemaSpecification &specification);

        UnitsSchemasDataPack         m_pack;                   ///< 方案数据包
        std::unique_ptr<UnitsSchema> m_currentSchema;          ///< 当前方案对象
        std::size_t                  m_fractionDenominator{0}; ///< 当前分数分母
    };
} // namespace ExpressionEngine::Units
