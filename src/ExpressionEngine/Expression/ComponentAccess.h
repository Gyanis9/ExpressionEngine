/**
 * @file ComponentAccess.h
 * @brief 把分量作用到值上：单个分量取子值，区间分量取多个子值
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <string_view>
#include <vector>

#include <ExpressionEngine/Expression/Expression.h>
#include <ExpressionEngine/Expression/Value.h>

namespace ExpressionEngine::Expression
{

    /**
     * @brief 按单个分量从值里取子值
     * @details 只处理值语义的分量：Index 下标、MapKey 映射键、Name 名字；Range 由
     *          applyRangeComponent 处理，落在本函数上时会报错并提示改用聚合函数。
     *          Index 支持向量分量（[0] 取 x，负下标从末尾计数）与文本字符（[i] 取第 i
     *          个 UTF-8 字符），其余值类型报错，不做任何隐式转换。
     * @param value 基值
     * @param component 分量
     * @param context 报错场景描述，如「引用 'Box.Placement.Base' 的分量访问」
     * @return 取到的子值
     * @throws Base::IndexError 下标越界
     * @throws Base::AttributeError 名字分量在该值上不存在（值层面无法解析）
     * @throws Base::TypeError 值类型不支持该分量（如给数量取下标、映射键分量）
     * @throws EvaluationError 分量是区间；区间只能作为聚合函数的实参
     */
    [[nodiscard]] Value applyComponent(const Value &value, const Expression::Component &component, std::string_view context);

    /**
     * @brief 按区间分量从值里取出多个子值，供聚合函数使用
     * @details 区间的端点与步长都是常量表达式（解析器已保证）；端点按含末端语义取值，
     *          开放端按其边界含义取到开头或末尾：正步长时起点缺省为 0、终点缺省为末位，
     *          负步长时方向相反。目前只有向量支持区间取子值。
     * @param value 基值
     * @param component Range 分量，其 index/endIndex/step 都是常量表达式（解析器已保证）
     * @param context 报错场景描述
     * @return 依次取出的子值；开放区间按其边界含义取到末尾/开头
     * @throws Base::IndexError 区间越界或步长为 0
     * @throws Base::TypeError 值类型不支持区间取子值
     */
    [[nodiscard]] std::vector<Value> applyRangeComponent(const Value &value, const Expression::Component &component, std::string_view context);

} // namespace ExpressionEngine::Expression
