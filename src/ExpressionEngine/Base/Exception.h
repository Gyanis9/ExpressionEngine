/**
 * @file Exception.h
 * @brief 库的统一异常体系
 * @author Gyanis
 * @date 2026-09-19
 * @version 0.0.3
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace ExpressionEngine::Base
{
    /**
     * @brief 故障类别
     * @details 异常体系本来就能按类型分支，但 tryParse()、tryEvaluate() 这类非异常通道只带回一条
     *          中文文案，宿主想区分「改文本」与「改单位」就只能去匹配中文字串。ErrorKind 给两条
     *          通道同一个可判的类别：异常对象用 kind() 报告，值通道原样带过去，因此同一次故障在
     *          两条通道上的类别必然相等。每个异常类都报自己的类别（用例逐条钉住这张对应表），
     *          新增的类若忘了标，落到 Other 而不是静默沿用别人的类别。
     */
    enum class ErrorKind
    {
        Other,         ///< 未归类：新增故障类型却没标类别时的兜底
        Parser,        ///< 词法与语法错，改文本即可
        EmptyInput,    ///< 输入为空，没有任何可解析的内容
        TooDeep,       ///< 嵌套层数或表达式树深超上限，需要把式子拆短
        UnitsMismatch, ///< 运算两侧单位不同，改成同一单位的量再算
        Overflow,      ///< 数值超出可表示范围，缩小量级
        Underflow,     ///< 数值低于可表示范围，放大量级
        Type,          ///< 值类型不参与该运算
        Value,         ///< 类型对但内容不被接受（除数为零、零向量归一化等）
        Index,         ///< 分量下标越界
        Attribute,     ///< 属性或分量不存在
        Name,          ///< 标识符解析不到对象或属性
        Expression,    ///< 表达式引擎的其它运行期故障
    };

    /**
     * @brief 运行期故障的统一基类
     * @details 库对外抛出的运行期故障全部派生自本类，调用方可用一条 catch (const Exception&) 兜住。
     *          编程/用法错误（空指针、违反前置条件等）刻意不并入本类，直接抛 std::invalid_argument 或
     *          std::logic_error，以免调用方把自己的 bug 当成可恢复故障吞掉。
     *          每个派生类型对应一种处置方式：改文本（ParserError）、改单位（UnitsMismatchError）、
     *          改数值范围（OverflowError / UnderflowError）、其余按引擎故障记录。
     */
    class Exception : public std::runtime_error
    {
    public:
        /**
         * @brief 构造异常
         * @param message 中文可操作文案，写清「原因 + 替代做法」
         * @param kind 故障类别；派生类带上自己的类别，宿主自己的派生类留默认值即可
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit Exception(std::string message, ErrorKind kind = ErrorKind::Other, const std::source_location &location = std::source_location::current());

        /**
         * @brief 取故障类别
         * @details 与值通道（ParseFailure::kind、EvaluationFailure::kind）同源：抛出时定下，
         *          转成值返回时原样带过去，宿主因此可以在「捕获」与「判返回值」两条写法上
         *          得到同一个分支依据。
         * @return 构造时带来的类别
         */
        [[nodiscard]] ErrorKind kind() const noexcept
        {
            return m_kind;
        }

        /**
         * @brief 取原始消息
         * @return 构造时传入的消息正文，不含位置信息
         */
        [[nodiscard]] const std::string &message() const noexcept
        {
            return m_message;
        }

        /**
         * @brief 取抛出点所在的源文件
         * @return 源文件路径字面量，指向静态存储，调用方无须释放
         */
        [[nodiscard]] const char *file() const noexcept
        {
            return m_file;
        }

        /**
         * @brief 取抛出点所在的行号
         * @return 源码行号
         */
        [[nodiscard]] int sourceLine() const noexcept
        {
            return m_sourceLine;
        }

        /**
         * @brief 取抛出点所在的函数名
         * @return 函数名字面量，指向静态存储，调用方无须释放
         */
        [[nodiscard]] const char *function() const noexcept
        {
            return m_function;
        }

        /**
         * @brief 拼出「消息（文件:行 in 函数）」的完整描述
         * @return 供日志与终端输出的单行文本
         */
        [[nodiscard]] std::string toString() const;

    private:
        std::string m_message;    ///< 消息正文
        ErrorKind   m_kind;       ///< 故障类别，与值通道里的 kind 同源
        const char *m_file;       ///< 源文件名，指向字面量，不持所有权
        int         m_sourceLine; ///< 抛出点行号
        const char *m_function;   ///< 函数名，指向字面量，不持所有权
    };

    /**
     * @brief 文本解析失败
     * @details 表达式或数量文本存在词法/语法错误；调用方应把消息报告给用户去改文本，而不是重试同一份输入。
     */
    class ParserError : public Exception
    {
    public:
        /**
         * @brief 构造文本解析失败异常
         * @param message 中文可操作文案
         * @param kind 故障类别；词法语法错留默认值，输入为空、嵌套过深、单位表缺符号这类
         *              处置方式不同的抛出点显式给出
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit ParserError(std::string message, ErrorKind kind = ErrorKind::Parser, const std::source_location &location = std::source_location::current()) : Exception(std::move(message), kind, location)
        {
        }
    };

    /**
     * @brief 单位不匹配
     * @details 加、减、比较等运算的两侧单位不同；调用方应改成同一单位的量再运算。
     */
    class UnitsMismatchError : public Exception
    {
    public:
        /**
         * @brief 构造单位不匹配异常
         * @param message 中文可操作文案
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit UnitsMismatchError(std::string message, const std::source_location &location = std::source_location::current()) : Exception(std::move(message), ErrorKind::UnitsMismatch, location)
        {
        }
    };

    /**
     * @brief 数值溢出
     * @details 指数或数值超出可表示范围；调用方应缩小量级后再试。
     */
    class OverflowError : public Exception
    {
    public:
        /**
         * @brief 构造数值溢出异常
         * @param message 中文可操作文案
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit OverflowError(std::string message, const std::source_location &location = std::source_location::current()) : Exception(std::move(message), ErrorKind::Overflow, location)
        {
        }
    };

    /**
     * @brief 数值下溢
     * @details 指数或数值低于可表示范围；调用方应放大量级后再试。
     */
    class UnderflowError : public Exception
    {
    public:
        /**
         * @brief 构造数值下溢异常
         * @param message 中文可操作文案
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit UnderflowError(std::string message, const std::source_location &location = std::source_location::current()) : Exception(std::move(message), ErrorKind::Underflow, location)
        {
        }
    };

    /**
     * @brief 类型不符
     * @details 运算符或函数收到不能参与该运算的值类型；调用方应改表达式。
     */
    class TypeError : public Exception
    {
    public:
        /**
         * @brief 构造类型不符异常
         * @param message 中文可操作文案
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit TypeError(std::string message, const std::source_location &location = std::source_location::current()) : Exception(std::move(message), ErrorKind::Type, location)
        {
        }
    };

    /**
     * @brief 取值非法
     * @details 值类型正确但内容不被接受（如除数为零、零向量无法归一化）；调用方应改表达式。
     */
    class ValueError : public Exception
    {
    public:
        /**
         * @brief 构造取值非法异常
         * @param message 中文可操作文案
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit ValueError(std::string message, const std::source_location &location = std::source_location::current()) : Exception(std::move(message), ErrorKind::Value, location)
        {
        }
    };

    /**
     * @brief 索引越界
     * @details 分量下标超出容器范围；调用方应改下标。
     */
    class IndexError : public Exception
    {
    public:
        /**
         * @brief 构造索引越界异常
         * @param message 中文可操作文案
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit IndexError(std::string message, const std::source_location &location = std::source_location::current()) : Exception(std::move(message), ErrorKind::Index, location)
        {
        }
    };

    /**
     * @brief 属性不存在
     * @details 对象上找不到被引用的属性或分量；调用方应改引用路径。
     */
    class AttributeError : public Exception
    {
    public:
        /**
         * @brief 构造属性不存在异常
         * @param message 中文可操作文案
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit AttributeError(std::string message, const std::source_location &location = std::source_location::current()) : Exception(std::move(message), ErrorKind::Attribute, location)
        {
        }
    };

    /**
     * @brief 标识符无法解析
     * @details 变量名既不是已注册对象也不是已定义参数；调用方应补上定义或改名字。
     */
    class NameError : public Exception
    {
    public:
        /**
         * @brief 构造标识符无法解析异常
         * @param message 中文可操作文案
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit NameError(std::string message, const std::source_location &location = std::source_location::current()) : Exception(std::move(message), ErrorKind::Name, location)
        {
        }
    };

    /**
     * @brief 表达式引擎运行期故障
     * @details 循环引用、求值深度超限等；调用方应修正表达式结构。
     */
    class ExpressionError : public Exception
    {
    public:
        /**
         * @brief 构造表达式引擎故障异常
         * @param message 中文可操作文案
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit ExpressionError(std::string message, const std::source_location &location = std::source_location::current()) : Exception(std::move(message), ErrorKind::Expression, location)
        {
        }
    };
} // namespace ExpressionEngine::Base
