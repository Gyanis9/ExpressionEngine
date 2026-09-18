/**
 * @file Range.h
 * @brief 电子表格式单元格地址与区间迭代
 * @author Gyanis
 * @date 2026-09-18
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <string>
#include <string_view>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Expression {

/**
 * @brief 单元格地址
 * @details 行号与列号都从 0 开始计数；地址文本形如 "A1"、"$B$2"，'$' 表示绝对引用，
 *          宿主在插入行列时据此决定是否偏移该引用。非法地址一律记为无效（行列均为 -1），
 *          由调用方决定是报错还是当作普通名字处理。
 */
struct CellAddress {
    /// 显示选项位；位值可直接相或，如 Absolute | ShowRowColumn
    enum class Cell : unsigned {
        Absolute = 1,       ///< 输出 '$' 绝对引用标记
        ShowRow = 2,        ///< 输出行号
        ShowColumn = 4,     ///< 输出列标
        ShowRowColumn = 6,  ///< ShowRow | ShowColumn
        ShowFull = 7        ///< Absolute | ShowRow | ShowColumn
    };

    static constexpr int s_maxRows = 16384;   ///< 行号上限，从 1 起计数
    static constexpr int s_maxColumns = 702;  ///< 列号上限，列标 A 到 ZZ

    /**
     * @brief 以行列号构造
     * @param row 行号，0 起；超出 [0, s_maxRows) 记为无效
     * @param column 列号，0 起；超出 [0, s_maxColumns) 记为无效
     * @param absoluteRow 行是否为绝对引用
     * @param absoluteColumn 列是否为绝对引用
     */
    explicit CellAddress(int row = -1,
                         int column = -1,
                         bool absoluteRow = false,
                         bool absoluteColumn = false);

    /**
     * @brief 以地址文本构造
     * @param address 形如 "A1"、"$B$2" 的文本
     * @throws Base::ParserError 文本不是合法地址
     */
    explicit CellAddress(const std::string& address);

    /// 取行号，0 起；无效地址返回 -1
    [[nodiscard]] int row() const noexcept;

    /// 取列号，0 起；无效地址返回 -1
    [[nodiscard]] int column() const noexcept;

    /**
     * @brief 设置行号
     * @param row 行号，0 起
     * @param clip true 时越界夹到末行，false 时越界记为无效
     */
    void setRow(int row, bool clip = false);

    /**
     * @brief 设置列号
     * @param column 列号，0 起
     * @param clip true 时越界夹到末列，false 时越界记为无效
     */
    void setColumn(int column, bool clip = false);

    /// 行列是否都落在合法范围内
    [[nodiscard]] bool isValid() const noexcept;

    /// 行是否绝对引用
    [[nodiscard]] bool isAbsoluteRow() const noexcept;

    /// 列是否绝对引用
    [[nodiscard]] bool isAbsoluteColumn() const noexcept;

    /**
     * @brief 转成地址文本
     * @param style 输出哪些部分，默认输出绝对标记、列标与行号
     * @return 形如 "A1"、"$B$2" 的文本；无效地址按行列原值编码，不做额外校验
     */
    [[nodiscard]] std::string toString(Cell style = Cell::ShowFull) const;

    bool operator<(const CellAddress& other) const noexcept;

    bool operator>(const CellAddress& other) const noexcept;

    bool operator==(const CellAddress& other) const noexcept;

    bool operator!=(const CellAddress& other) const noexcept;

private:
    /**
     * @brief 把行列压进一个整数，用于大小比较与排序
     * @return 行号在高 16 位、列号在低 16 位的无符号编码
     */
    [[nodiscard]] unsigned int asInteger() const noexcept;

    short m_row;            ///< 行号，0 起；负数表示无效地址
    short m_column;         ///< 列号，0 起；负数表示无效地址
    bool m_absoluteRow;     ///< 行是否绝对引用
    bool m_absoluteColumn;  ///< 列是否绝对引用
};

/**
 * @brief 把地址文本解析成单元格地址
 * @param address 形如 "A1"、"$B$2" 的文本
 * @param silent true 时解析失败返回无效地址，false 时抛错
 * @return 解析结果
 * @throws Base::ParserError silent 为 false 且文本不合法
 */
[[nodiscard]] CellAddress stringToAddress(std::string_view address, bool silent = false);

/**
 * @brief 把列标文本解析成 0 起的列号
 * @param columnText 形如 "A"、"ZZ" 的列标
 * @param silent true 时解析失败返回 -1，false 时抛错
 * @return 0 起的列号
 * @throws Base::IndexError silent 为 false 且列标不合法
 */
[[nodiscard]] int decodeColumn(std::string_view columnText, bool silent = false);

/**
 * @brief 把行号文本解析成 0 起的行号
 * @param rowText 形如 "1"、"16384" 的行号文本
 * @param silent true 时解析失败返回 -1，false 时抛错
 * @return 0 起的行号
 * @throws Base::IndexError silent 为 false 且行号文本不合法
 */
[[nodiscard]] int decodeRow(std::string_view rowText, bool silent = false);

/// 列标文本是否合法（一到两个 A..Z 字母且不超过 ZZ）
[[nodiscard]] bool validColumn(std::string_view columnText);

/// 行号文本是否合法；合法时返回 0 起的行号，否则返回 -1
[[nodiscard]] int validRow(std::string_view rowText);

/**
 * @brief 单元格区间迭代器
 * @details 区间至少含一个单元格，遍历写法固定为 do { ... } while (range.next());，
 *          首次取值前不需要调用 next()。行列顺序上先沿列推进，再换行。
 */
class Range {
public:
    /**
     * @brief 以区间文本构造
     * @param rangeText 形如 "A1"、"A1:B2" 的文本
     * @param normalize true 时把区间整理成左上到右下
     * @throws Base::ParserError 文本不是合法区间
     */
    explicit Range(std::string_view rangeText, bool normalize = false);

    /**
     * @brief 以行列号构造
     * @param rowBegin 起始行，0 起
     * @param columnBegin 起始列，0 起
     * @param rowEnd 结束行，0 起
     * @param columnEnd 结束列，0 起
     * @param normalize true 时把区间整理成左上到右下
     */
    Range(int rowBegin, int columnBegin, int rowEnd, int columnEnd, bool normalize = false);

    /**
     * @brief 以两个地址构造
     * @param from 起始地址
     * @param to 结束地址
     * @param normalize true 时把区间整理成左上到右下
     */
    Range(const CellAddress& from, const CellAddress& to, bool normalize = false);

    /**
     * @brief 前进到下一个单元格
     * @details 与 FreeCAD 的区间语义一致：遍历游标是内部状态，因此本方法为 const，
     *          使 const 区间也能参与 `do { … } while (range.next())` 的遍历写法。
     * @return 还有下一个单元格时为 true；已遍历完成为 false
     */
    bool next() const;

    /// 整理区间，使起点在左上、终点在右下
    void normalize();

    /// 当前行，0 起
    [[nodiscard]] int row() const noexcept;

    /// 当前列，0 起
    [[nodiscard]] int column() const noexcept;

    /// 区间行数，至少为 1
    [[nodiscard]] int rowCount() const noexcept;

    /// 区间列数，至少为 1
    [[nodiscard]] int columnCount() const noexcept;

    /// 起始地址
    [[nodiscard]] CellAddress from() const;

    /// 结束地址
    [[nodiscard]] CellAddress to() const;

    /// 当前地址
    [[nodiscard]] CellAddress current() const;

    /// 起始地址文本
    [[nodiscard]] std::string fromCellText() const;

    /// 结束地址文本
    [[nodiscard]] std::string toCellText() const;

    /// 当前地址文本，可作为宿主的属性名使用
    [[nodiscard]] std::string address() const;

    /// 区间文本，形如 "A1:B2"
    [[nodiscard]] std::string rangeText() const;

    /// 区间包含的单元格个数
    [[nodiscard]] int size() const noexcept;

    bool operator<(const Range& other) const;

private:
    mutable int m_rowCurrent{0};     ///< 当前行，遍历时随 next() 推进
    mutable int m_columnCurrent{0};  ///< 当前列，遍历时随 next() 推进
    int m_rowBegin{0};               ///< 起始行
    int m_columnBegin{0};            ///< 起始列
    int m_rowEnd{0};                 ///< 结束行
    int m_columnEnd{0};              ///< 结束列
};

}  // namespace ExpressionEngine::Expression
