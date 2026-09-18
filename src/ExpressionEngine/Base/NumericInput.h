/**
 * @file NumericInput.h
 * @brief 区域化数字输入扫描
 * @author Gyanis
 * @date 2026-09-18
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace ExpressionEngine::Base {
struct NumericLocaleContext;

/// 数字被扫描时所在的语法位置
enum class NumericSyntaxContext {
    Standalone,       ///< 独立的一个数量或数字
    Expression,       ///< 表达式内部的一个数字记号
    FunctionArgument  ///< 函数实参位置，此时区域的分隔符可能与语法标点冲突
};

/// 某个语法位置下实际生效的分隔符与分组规则
struct NumericGrammarPolicy {
    std::string_view decimalSeparator;   ///< 生效的小数点
    std::string_view groupingSeparator;  ///< 生效的分组分隔符
    std::string_view argumentSeparator;  ///< 生效的实参分隔符
    bool allowGrouping{true};            ///< 是否允许分组分隔符
};

/// 扫描失败的原因分类
enum class NumericDiagnosticKind {
    ExpectedDigit,        ///< 期望数字却遇到其他字符
    IncompleteSign,       ///< 只有符号没有数字
    IncompleteDecimal,    ///< 小数点后没有数字
    IncompleteGrouping,   ///< 分组分隔符后没有数字
    IncompleteExponent,   ///< 指数标记后没有数字
    InvalidGrouping,      ///< 分组位数不符合区域规则
    UnexpectedSeparator,  ///< 出现了不该出现的分隔符
    InvalidLiteral,       ///< 字面量本身非法
    OutOfRange            ///< 数值超出双精度可表示范围
};

/// 一次扫描失败的类型化原因与定位
struct NumericDiagnostic {
    NumericDiagnosticKind kind{NumericDiagnosticKind::InvalidLiteral};  ///< 失败原因
    std::size_t offsetBytes{};  ///< 出错片段在输入中的字节偏移
    std::size_t lengthBytes{};  ///< 出错片段的字节长度
};

/// 单个区域化数字记号的扫描结果
struct LocalizedNumberResult {
    enum class Status {
        Complete,    ///< 扫完一个完整记号
        Incomplete,  ///< 输入在此处结束但记号未写完
        Invalid      ///< 输入非法
    };

    Status status{Status::Invalid};               ///< 扫描状态
    double value{};                               ///< 解析值，仅 Complete 时有意义
    std::string canonicalText;                    ///< 与区域无关的标准写法，仅 Complete 时有意义
    std::size_t consumedBytes{};                  ///< 本记号在输入中占用的字节数
    std::optional<NumericDiagnostic> diagnostic;  ///< 失败时的类型化原因与定位
};

/**
 * @brief 取指定语法位置下的分隔符策略
 * @param locale 区域快照
 * @param syntax 语法位置
 * @return 生效的分隔符与分组开关
 */
[[nodiscard]] NumericGrammarPolicy numericGrammarPolicy(const NumericLocaleContext& locale,
                                                        NumericSyntaxContext syntax);

/**
 * @brief 读取指定位置上的区域化数字字符
 * @param input UTF-8 输入
 * @param position 起始字节偏移
 * @param locale 区域快照，其零字形决定数字的起始码点
 * @param digit 输出参数，成功时写入 0-9
 * @param consumedBytes 输出参数，成功时写入该字符的字节宽度
 * @return true 该位置是一个数字字符
 * @return false 该位置不是数字字符，digit 与 consumedBytes 不被写入
 */
[[nodiscard]] bool localizedDigitAt(std::string_view input,
                                    std::size_t position,
                                    const NumericLocaleContext& locale,
                                    int& digit,
                                    std::size_t& consumedBytes);

/**
 * @brief 扫描一个区域化数字记号
 * @details 记号边界由本函数按区域规则判定，不依赖任何外部区域库；完整结果可以只消费输入的前缀，
 *          由 consumedBytes 指出该前缀长度。
 * @param input 待扫描的 UTF-8 输入
 * @param locale 区域快照
 * @param syntax 语法位置，决定分隔符是否具有结构含义
 * @return 扫描结果；Incomplete 与 Invalid 时 diagnostic 给出原因与字节范围
 */
[[nodiscard]] LocalizedNumberResult scanLocalizedNumber(std::string_view input,
                                                        const NumericLocaleContext& locale,
                                                        NumericSyntaxContext syntax);
}  // namespace ExpressionEngine::Base
