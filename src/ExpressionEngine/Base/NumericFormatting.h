/**
 * @file NumericFormatting.h
 * @brief 区域数字格式快照与数字格式化
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace ExpressionEngine::Base
{
    /**
     * @brief 数字的呈现方式
     * @details 取值与数量格式设置里的同名枚举一一对应，用于选择定点、科学计数或默认记数法。
     */
    enum class NumberNotation
    {
        Default,   ///< 由有效位数决定，等价于 printf 的 %g
        Fixed,     ///< 定点记数法，等价于 printf 的 %f
        Scientific ///< 科学记数法，等价于 printf 的 %e
    };

    /**
     * @brief 一次完整的区域数字格式快照
     * @details 解析与格式化都按整份快照进行，发布时整体替换，因此消费方不会读到「区域标识来自 A、
     *          分隔符来自 B」的混合状态。宿主应用可用任意分隔符自行构造，不必受内置区域表限制。
     */
    struct NumericLocaleContext
    {
        std::string localeId;                ///< 区域标识，如 "C"、"de_DE"
        std::string decimalSeparator;        ///< 小数点，如 "." 或 ","
        std::string groupingSeparator;       ///< 分组分隔符，如 "," 或 "."
        std::string positiveSign;            ///< 正号，如 "+"
        std::string negativeSign;            ///< 负号，如 "-"
        int         primaryGroupingSize{};   ///< 主要分组位数，0 表示不分组
        int         secondaryGroupingSize{}; ///< 次要分组位数，0 表示与主要分组位数相同
        std::string zeroDigit;               ///< 该区域的零字形（UTF-8），十进制数字自它起连续

        bool operator==(const NumericLocaleContext &) const = default;
    };

    /**
     * @brief 区域设置错误的分类
     */
    enum class NumericLocaleErrorCode
    {
        InvalidIdentifier, ///< 标识符不合法（空串或含非字母数字字符）
        UnsupportedLocale, ///< 标识符合法但没有内置分隔符数据，需调用方显式构造上下文
    };

    /**
     * @brief 区域设置错误详情
     */
    struct NumericLocaleError
    {
        NumericLocaleErrorCode code;    ///< 错误分类
        std::string            message; ///< 中文可操作文案
    };

    /**
     * @brief 取 C/POSIX 区域的上下文
     * @return 小数点为 "."、无分组、零字形为 ASCII '0' 的快照
     */
    [[nodiscard]] NumericLocaleContext cLocaleContext();

    /**
     * @brief 按内置区域表构造上下文
     * @details 仅覆盖内置表内的区域；表外区域返回 UnsupportedLocale 而不是静默退回 C 区域，
     *          调用方据此提示用户或显式构造 NumericLocaleContext。
     * @param localeId 区域标识，接受 "de_DE"、"de-DE"、"de_DE.UTF-8" 等写法
     * @return 成功返回快照；失败返回 NumericLocaleError，message 为中文原因与替代做法
     */
    [[nodiscard]] std::expected<NumericLocaleContext, NumericLocaleError> createNumericLocaleContext(std::string_view localeId);

    /**
     * @brief 取当前已发布的上下文
     * @return 最近一次 publishNumericLocaleContext() 发布的快照；从未发布过时为 C 区域上下文
     */
    [[nodiscard]] NumericLocaleContext currentNumericLocaleContext();

    /**
     * @brief 整体替换已发布的上下文
     * @param state 新快照，整体生效；跨线程读取时不会读到半新半旧的分隔符组合
     */
    void publishNumericLocaleContext(NumericLocaleContext state);

    /**
     * @brief 按精度、记数法与区域快照格式化标量
     * @details 只接收格式化所需的原始参数而不接收整个数量格式对象，因此本层不需要知道数量类型。
     * @param value 待格式化的数值
     * @param precision 小数位数或有效位数，负数或 0 表示按最短往返写法输出
     * @param notation 记数法
     * @param omitGroupSeparator 是否关闭分组分隔符输出
     * @param formatting 区域快照，其小数点为空时抛 std::invalid_argument
     * @return 按区域分隔符与分组规则排版后的文本
     */
    [[nodiscard]] std::string formatNumericValue(double value, int precision, NumberNotation notation, bool omitGroupSeparator, const NumericLocaleContext &formatting);
} // namespace ExpressionEngine::Base
