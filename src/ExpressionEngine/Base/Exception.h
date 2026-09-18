/**
 * @file Exception.h
 * @brief 库的统一异常体系
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <source_location>
#include <stdexcept>
#include <string>

namespace ExpressionEngine::Base
{
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
         * @param location 抛出位置，默认由编译器在调用点填入
         */
        explicit Exception(std::string message, const std::source_location &location = std::source_location::current());

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
        using Exception::Exception;
    };

    /**
     * @brief 单位不匹配
     * @details 加、减、比较等运算的两侧单位不同；调用方应改成同一单位的量再运算。
     */
    class UnitsMismatchError : public Exception
    {
    public:
        using Exception::Exception;
    };

    /**
     * @brief 数值溢出
     * @details 指数或数值超出可表示范围；调用方应缩小量级后再试。
     */
    class OverflowError : public Exception
    {
    public:
        using Exception::Exception;
    };

    /**
     * @brief 数值下溢
     * @details 指数或数值低于可表示范围；调用方应放大量级后再试。
     */
    class UnderflowError : public Exception
    {
    public:
        using Exception::Exception;
    };

    /**
     * @brief 类型不符
     * @details 运算符或函数收到不能参与该运算的值类型；调用方应改表达式。
     */
    class TypeError : public Exception
    {
    public:
        using Exception::Exception;
    };

    /**
     * @brief 取值非法
     * @details 值类型正确但内容不被接受（如除数为零、零向量无法归一化）；调用方应改表达式。
     */
    class ValueError : public Exception
    {
    public:
        using Exception::Exception;
    };

    /**
     * @brief 索引越界
     * @details 分量下标超出容器范围；调用方应改下标。
     */
    class IndexError : public Exception
    {
    public:
        using Exception::Exception;
    };

    /**
     * @brief 属性不存在
     * @details 对象上找不到被引用的属性或分量；调用方应改引用路径。
     */
    class AttributeError : public Exception
    {
    public:
        using Exception::Exception;
    };

    /**
     * @brief 标识符无法解析
     * @details 变量名既不是已注册对象也不是已定义参数；调用方应补上定义或改名字。
     */
    class NameError : public Exception
    {
    public:
        using Exception::Exception;
    };

    /**
     * @brief 表达式引擎运行期故障
     * @details 循环引用、求值深度超限等；调用方应修正表达式结构。
     */
    class ExpressionError : public Exception
    {
    public:
        using Exception::Exception;
    };
} // namespace ExpressionEngine::Base
