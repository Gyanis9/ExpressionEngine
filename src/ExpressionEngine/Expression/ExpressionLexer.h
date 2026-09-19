/**
 * @file ExpressionLexer.h
 * @brief 表达式词法分析器
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <string>
#include <string_view>

namespace ExpressionEngine::Expression
{
    /// 记号类别
    enum class ExpressionTokenKind
    {
        Number,       ///< 十进制数值：1.5、.5、1,5、1e3
        Integer,      ///< 纯整数：1、42
        Unit,         ///< 国际单位符号：mm、kg、°、′
        UsUnit,       ///< 英制建筑单位符号：' 与 "
        String,       ///< <<...>> 文本；text 为去掉定界符并处理转义后的内容
        Identifier,   ///< 标识符：abc、_x、含 @ 的形式
        CellAddress,  ///< 单元格地址：A1、$A$1、$A1
        Function,     ///< 函数名；记号连同其后的左括号一起吃掉：sin(、log10 (
        Constant,     ///< 常量：pi、e、None、True、False
        Plus,         ///< '+'
        Minus,        ///< '-' 或 Unicode 减号 U+2212
        Star,         ///< '*'
        Slash,        ///< '/'
        Percent,      ///< '%'
        Caret,        ///< '^'
        Equal,        ///< '=' 或 '=='
        NotEqual,     ///< '!='
        Less,         ///< '<'
        Greater,      ///< '>'
        LessEqual,    ///< '<='
        GreaterEqual, ///< '>='
        Question,     ///< '?'
        Colon,        ///< ':'
        Comma,        ///< ','
        Semicolon,    ///< ';'
        LeftParen,    ///< '('
        RightParen,   ///< ')'
        LeftBracket,  ///< '['
        RightBracket, ///< ']'
        Dot,          ///< '.'，引用路径的分量分隔符
        DocumentRef,  ///< <<文档#单元格>> 形式的跨文档引用
        End           ///< 输入结束
    };

    /// 一个记号
    struct ExpressionToken
    {
        ExpressionTokenKind kind{ExpressionTokenKind::End}; ///< 记号类别

        std::string text;            ///<去定界与转义后的内容
        double      numberValue{0};  ///< Number/Integer/Constant 的数值
        int         integerValue{0}; ///< Integer 的整数值；数值超出 int 时以 numberValue 为准
        std::size_t offset{0};       ///< 在输入中的字节偏移，供报错定位
        int         column{1};       ///< 1 起的列号，按 UTF-8 码点计数
    };

    /**
     * @brief 手写表达式词法分析器
     * @details 在同一位置并行求出各条规则的匹配长度并取最长匹配，等长时按规则的先后取舍，
     *          因此 mm 胜过 m、sin( 胜过单位 s；数字写法、单位与常量表、单元格地址与转义
     *          规则由实现文件统一给出。
     */
    class ExpressionLexer
    {
    public:
        /**
         * @brief 构造词法分析器
         * @param text 待分析文本；分析器只持有视图，调用方须保证其生命周期不短于本对象
         */
        explicit ExpressionLexer(std::string_view text);

        /**
         * @brief 取下一个记号
         * @return 记号；输入耗尽后返回 End，之后再调用仍返回 End
         * @throws Base::ParserError 词法错误（无法识别的字符、不能单独出现的字符、未闭合的 <<
         * 字符串），消息带列号
         * @throws Base::OverflowError 整数字面量超出 long long 的表示范围
         */
        ExpressionToken next();

    private:
        /// 跳过空白：空格、制表符、回车与换行；换行把列号重置为 1
        void skipWhitespace();

        /// 取出从当前偏移起 byteCount 个字节的原文并前进，同时按码点推进列号
        [[nodiscard]] std::string_view takeRawText(std::size_t byteCount);

        std::string_view m_text;      ///< 输入文本（只持有视图）
        std::size_t      m_offset{0}; ///< 下一个待读字节的偏移
        int              m_column{1}; ///< 下一个待读字符的列号
    };
} // namespace ExpressionEngine::Expression
