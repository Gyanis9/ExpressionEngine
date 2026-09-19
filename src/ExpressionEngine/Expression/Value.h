/**
 * @file Value.h
 * @brief 表达式求值的值模型
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <string>
#include <string_view>
#include <variant>

#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Placement.h>
#include <ExpressionEngine/Base/Rotation.h>
#include <ExpressionEngine/Base/Vector3D.h>
#include <ExpressionEngine/Units/Quantity.h>

namespace ExpressionEngine::Expression
{

    /**
     * @brief 表达式与属性之间的值
     * @details 固定类型集合而非任意类型：运算与函数的取值路径都能被编译器穷举检查，
     *          类型不符在编译错而不是等到运行期才发现。宿主若要承载自定义类型，
     *          应把它映射到这几类之一（如序列化成文本），而不是往值里塞任意对象。
     */
    using Value = std::variant<Units::Quantity, double, bool, std::string, Base::Vector3d, Base::Matrix4D, Base::Rotation, Base::Placement>;

    /// 是否数值型（数量或纯数）
    [[nodiscard]] bool isNumeric(const Value &value);

    /// 是否几何型（向量、矩阵、旋转、位姿）
    [[nodiscard]] bool isGeometric(const Value &value);

    /// 取值的类型名，用于报错文案（如 "数量"、"文本"、"向量"）
    [[nodiscard]] std::string_view valueTypeName(const Value &value);

    /**
     * @brief 把值转成数量
     * @param value 待转换的值
     * @param context 调用场景描述，用于报错文案定位（如 "加法左操作数"）
     * @return 数量；纯数与布尔按无量纲处理，文本若能解析成数量也接受
     * @throws Base::TypeError 值既不是数量也不是可解析为数量的文本
     * @throws Base::ParserError 文本形态的数值无法解析
     */
    [[nodiscard]] Units::Quantity toQuantity(const Value &value, std::string_view context);

    /**
     * @brief 把值转成双精度
     * @param value 待转换的值
     * @param context 调用场景描述，用于报错文案定位
     * @return 双精度数值
     * @throws Base::TypeError 值类型不受支持，或数量带量纲（调用方应明确按量处理）
     */
    [[nodiscard]] double toDouble(const Value &value, std::string_view context);

    /**
     * @brief 把值转成布尔
     * @param value 待转换的值
     * @param context 调用场景描述，用于报错文案定位
     * @return 布尔值；数值按「非零为真」判定
     * @throws Base::TypeError 值类型无法参与真值判定
     */
    [[nodiscard]] bool toBool(const Value &value, std::string_view context);

    /// 把值转成可读文本；数量按当前单位方案排版，几何值给出其紧凑写法
    [[nodiscard]] std::string toString(const Value &value);

    /**
     * @brief 判断两个值是否相等
     * @details 数量按数值与量纲同时比较（量纲不同返回 false 而不抛错，供 == 运算符复用）；
     *          几何值按容差比较，浮点舍入不会让相等判定失败。
     * @param left 左值
     * @param right 右值
     * @return 类型与内容都相等时为 true；类型不同一律 false
     */
    [[nodiscard]] bool valuesEqual(const Value &left, const Value &right);

    /**
     * @brief 按值大小比较两个值
     * @param left 左值
     * @param right 右值
     * @return 左值小于右值时 true
     * @throws Base::TypeError 任一值不可比较
     * @throws Base::UnitsMismatchError 两个数量的量纲不同
     */
    [[nodiscard]] bool valueLessThan(const Value &left, const Value &right);

} // namespace ExpressionEngine::Expression
