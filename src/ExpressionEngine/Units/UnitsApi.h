/**
 * @file UnitsApi.h
 * @brief 单位模块的对外门面
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/UnitsSchema.h>
#include <ExpressionEngine/Units/UnitsSchemas.h>
#include <ExpressionEngine/Units/UnitsSchemasData.h>

namespace ExpressionEngine::Units
{
    /**
     * @brief 单位模块的门面
     * @details 以静态成员维护「当前方案」与显示精度，供数量排版与宿主设置界面使用。
     *          方案数据内置为默认值，宿主可调用 setSchema() 切换，也可自行构造 UnitsSchema
     *          绕开这里的全局状态。
     * @note 当前方案与精度是进程级共享状态：多线程读取前应先完成设置，运行期切换需自行加锁。
     */
    class UnitsApi
    {
    public:
        /**
         * @brief 按编号新建一个方案对象
         * @param schemaNumber 方案编号
         * @return 方案对象，调用方持有所有权
         * @throws NameError 找不到该编号的方案
         */
        [[nodiscard]] static std::unique_ptr<UnitsSchema> createSchema(std::size_t schemaNumber);

        /**
         * @brief 切换当前方案
         * @param name 方案名，可用 getNames() 取全部名称
         * @throws NameError 找不到该名称的方案
         */
        static void setSchema(const std::string &name);

        /**
         * @brief 切换当前方案
         * @param schemaNumber 方案编号
         * @throws NameError 找不到该编号的方案
         */
        static void setSchema(std::size_t schemaNumber);

        /**
         * @brief 按当前方案换算并排版
         * @param quant 待换算的量
         * @param factor 输出参数，实际使用的换算因子
         * @param unitString 输出参数，实际使用的单位串
         * @return 排版后的文本
         */
        static std::string schemaTranslate(const Quantity &quant, double &factor, std::string &unitString);

        /**
         * @brief 按当前方案与指定区域上下文换算并排版
         * @param quant 待换算的量
         * @param formatting 区域快照
         * @param factor 输出参数，实际使用的换算因子
         * @param unitString 输出参数，实际使用的单位串
         * @return 排版后的文本
         */
        static std::string schemaTranslate(const Quantity &quant, const Base::NumericLocaleContext &formatting, double &factor, std::string &unitString);

        /// 按当前方案换算并排版
        [[nodiscard]] static std::string schemaTranslate(const Quantity &quant);

        /// 按当前方案与指定区域上下文换算并排版
        [[nodiscard]] static std::string schemaTranslate(const Quantity &quant, const Base::NumericLocaleContext &formatting);

        /// 设置显示精度；传入负数表示恢复为方案默认值
        static void setDecimals(int precision);

        /// 取显示精度，未设置时为方案默认值
        [[nodiscard]] static int getDecimals();

        /// 设置分数分母；传入负数表示恢复为方案默认值
        static void setDenominator(int denominator);

        /// 取分数分母，未设置时为方案默认值
        [[nodiscard]] static int getDenominator();

        /// 取全部方案的描述，顺序按方案编号
        [[nodiscard]] static std::vector<std::string> getDescriptions();

        /// 取全部方案名，顺序按方案编号
        [[nodiscard]] static std::vector<std::string> getNames();

        /// 取方案总数
        [[nodiscard]] static std::size_t count();

        /// 当前方案是否用多单位表示角度
        [[nodiscard]] static bool isMultiUnitAngle();

        /// 当前方案是否用多单位表示长度
        [[nodiscard]] static bool isMultiUnitLength();

        /// 取当前方案的基准长度单位
        [[nodiscard]] static std::string getBasicLengthUnit();

        /// 取默认方案的编号
        [[nodiscard]] static std::size_t getDefaultSchemaNumber();

    protected:
        static inline auto s_schemas = std::make_unique<UnitsSchemas>(UnitsSchemasData::unitSchemasDataPack); ///< 方案集合
        static inline int  s_decimals{-1};                                                                    ///< 显示精度，负数表示取方案默认值
        static inline int  s_denominator{-1};                                                                 ///< 分数分母，负数表示取方案默认值
    };
} // namespace ExpressionEngine::Units
