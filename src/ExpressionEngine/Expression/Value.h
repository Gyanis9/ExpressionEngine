/**
 * @file Value.h
 * @brief 表达式求值的值模型
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Placement.h>
#include <ExpressionEngine/Base/Rotation.h>
#include <ExpressionEngine/Base/Vector3D.h>
#include <ExpressionEngine/Units/Quantity.h>

namespace ExpressionEngine::Expression
{

    /**
     * @brief 有序序列取值
     * @details list() 的取值形态，元素仍是 Value。Value 是个别名、无法前向声明，所以元素存储
     *          单独放进载体 ValueSequenceItems（定义在本文件 Value 别名之后），本类只持有它。
     *          序列一经构造就不可变，拷贝与移动只转手共享指针，与深拷贝无法从外部区分。
     */
    class ValueSequence
    {
    public:
        ValueSequence() = default; ///< 空序列

        /**
         * @brief 接管一份元素存储
         * @param items 元素存储；宿主通常改用 makeValueSequence()
         */
        explicit ValueSequence(std::shared_ptr<class ValueSequenceItems> items);

        /**
         * @brief 元素个数
         * @return 元素个数
         */
        [[nodiscard]] std::size_t size() const noexcept;

        /**
         * @brief 是否为空序列
         * @return 没有元素时为 true
         */
        [[nodiscard]] bool empty() const noexcept;

        /**
         * @brief 取元素存储
         * @return 元素存储；默认构造的空序列返回 nullptr
         */
        [[nodiscard]] const class ValueSequenceItems *items() const noexcept;

    private:
        std::shared_ptr<class ValueSequenceItems> m_items; ///< 元素存储，空序列时为空指针
    };

    /**
     * @brief 表达式与属性之间的值
     * @details 固定类型集合而非任意类型：运算与函数的取值路径都能被编译器穷举检查，
     *          类型不符在编译错而不是等到运行期才发现。宿主若要承载自定义类型，
     *          应把它映射到这几类之一（如序列化成文本），而不是往值里塞任意对象。
     */
    using Value = std::variant<Units::Quantity, double, bool, std::string, Base::Vector3d, Base::Matrix4D, Base::Rotation, Base::Placement, ValueSequence>;

    /**
     * @brief 序列的元素载体
     * @details 必须定义在 Value 别名之后（元素类型就是 Value 本身），因此与 ValueSequence 分开。
     *          构造序列请走 makeValueSequence()，本类只负责装元素。
     */
    class ValueSequenceItems
    {
    public:
        std::vector<Value> values; ///< 序列元素，构造后不再改动
    };

    /// 是否数值型（数量或纯数）
    [[nodiscard]] bool isNumeric(const Value &value);

    /// 是否几何型（向量、矩阵、旋转、位姿）
    [[nodiscard]] bool isGeometric(const Value &value);

    /// 是否序列型（list() 的取值）
    [[nodiscard]] bool isSequence(const Value &value);

    /**
     * @brief 造一个序列取值
     * @param values 元素，按值接收所有权
     * @return 序列取值；传入空列表得到空序列
     */
    [[nodiscard]] ValueSequence makeValueSequence(std::vector<Value> values);

    /**
     * @brief 取序列的全部元素
     * @param sequence 序列取值
     * @return 元素列表的只读引用；空序列返回空列表
     */
    [[nodiscard]] const std::vector<Value> &sequenceValues(const ValueSequence &sequence);

    /**
     * @brief 按下标取序列元素
     * @param sequence 序列取值
     * @param index 下标，0 起
     * @return 元素引用，指向序列自身的存储
     * @throws Base::IndexError 下标越界
     */
    [[nodiscard]] const Value &sequenceAt(const ValueSequence &sequence, std::size_t index);

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
     * @brief 数值在表达式文本里的写法
     * @details 取「能解析回同一个 double 的最短写法」，保证折成常量再写回文本不改变取值；
     *          与面向用户的排版（按单位方案与小数位）分开，后者不保证可逆。
     * @param value 数值
     * @return 可直接解析回该数的文本
     */
    [[nodiscard]] std::string formatExpressionNumber(double value);

    /**
     * @brief 把文本写成表达式里的文本取值
     * @details 词法器只认 << >> 一种文本定界符（单引号是英尺单位），正文里的反斜杠、'>'、
     *          '#' 与控制字符一并转义，使写出的文本能被词法器原样读回。
     * @param text 待写出的文本，可含任意字节
     * @return 带定界符的表达式文本
     */
    [[nodiscard]] std::string quoteExpressionText(std::string_view text);

    /**
     * @brief 取值的表达式写法，保证能被解析器读回同一个值
     * @details 与面向用户的 toString() 不同：几何值写成 vector()、matrix()、rotation()、
     *          placement() 构造调用，序列写成 list(...)，数量在非纯数时带上单位符号，
     *          文本按 << >> 定界，数值一律走 formatExpressionNumber()。
     * @param value 待写出的取值
     * @return 可重新解析的表达式文本
     */
    [[nodiscard]] std::string toExpressionText(const Value &value);

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
