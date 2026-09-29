/**
 * @file ExpressionLexer.h
 * @brief 表达式词法分析器
 * @author Gyanis
 * @date 2026-09-19
 * @version 0.0.3
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

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
        long long   integerValue{0}; ///< Integer 的整数值；词法器已按 long long 校过越界（更大或更小的整数走 Number 那条通道）
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

        /**
         * @brief 一个可写在表达式里的单位符号
         */
        struct UnitSymbolInfo
        {
            std::string_view symbol;   ///< 原文写法，大小写与重音符号都要照抄（µm 与 um 是两条）
            bool             isUsUnit; ///< 是否英制建筑记号（双引号与单引号），它们只跟在数字后面
        };

        /**
         * @brief 列出词法器认得的全部单位符号
         * @details 宿主的单位选择器以此为准：表里的写法都是能被词法器认出来的，不必再抄一份。
         *          用例逐条验证「列出来的都能被单位表查到」与「列出来的（除英制两记号）都能真的
         *          解析成功」，所以这份目录不会给出一个让用户选了却用不上的符号。
         *          顺序与词法匹配一致：先国际单位与派生单位，再英制建筑记号。
         * @return 单位符号目录，首次调用组装一次后只读
         */
        [[nodiscard]] static const std::vector<UnitSymbolInfo> &supportedUnitSymbols();

    private:
        /// 跳过空白：空格、制表符、回车与换行；换行把列号重置为 1
        void skipWhitespace();

        /// 取出从当前偏移起 byteCount 个字节的原文并前进，同时按码点推进列号
        [[nodiscard]] std::string_view takeRawText(std::size_t byteCount);

        std::string_view m_text;      ///< 输入文本（只持有视图）
        std::size_t      m_offset{0}; ///< 下一个待读字节的偏移
        int              m_column{1}; ///< 下一个待读字符的列号
    };

    /**
     * @brief 判断一段文本能否被词法器识别为函数名
     * @details 与函数记号的匹配规则同源，因此可作为「登记的名字是否永远调用不到」的判据：
     *          首字符须是字母类字符（下划线开头只会识别成标识符），其后允许字母、数字、下划线
     *          与非 ASCII 字母，但不允许 '@'（标识符专有）与 U+2212 减号（运算符）。
     *          供函数注册表校验宿主给出的函数名。
     * @param name 待判定的名字原文
     * @return 整个名字都能被函数记号规则吃下时返回 true；空文本返回 false
     */
    [[nodiscard]] bool isFunctionNameText(std::string_view name);

} // namespace ExpressionEngine::Expression
