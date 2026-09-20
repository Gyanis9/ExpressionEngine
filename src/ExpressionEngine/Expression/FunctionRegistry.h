/**
 * @file FunctionRegistry.h
 * @brief 宿主可扩展的函数注册表与自定义函数节点
 * @author Gyanis
 * @date 2026-09-20
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <ExpressionEngine/Expression/Expression.h>

namespace ExpressionEngine::Expression
{

    /**
     * @brief 自定义函数的调用上下文
     * @details 回调拿到的是实参表达式而不是算好的值：函数可以自己决定求哪几支，从而做出
     *          条件、短路、惰性求值一类的语义。只要结果就调用 argumentValue()，需要子表达式
     *          时调用 argument()。求值实参时抛出的引擎异常按原样向上传播，回调无须包装。
     */
    class FunctionCall
    {
    public:
        /**
         * @brief 取被调用的函数名
         * @return 函数名，与登记时一致
         */
        [[nodiscard]] std::string_view name() const noexcept;

        /**
         * @brief 取实参个数
         * @return 实参个数
         */
        [[nodiscard]] std::size_t argumentCount() const noexcept;

        /**
         * @brief 取第 index 个实参的表达式
         * @details 供惰性求值：未被求值的实参不会产生任何副作用。
         * @param index 实参下标，0 起
         * @return 实参表达式；下标越界时返回 nullptr，不抛错也不越界读
         */
        [[nodiscard]] const Expression *argument(std::size_t index) const noexcept;

        /**
         * @brief 求第 index 个实参的取值
         * @param index 实参下标，0 起
         * @return 实参取值
         * @throws Base::IndexError 下标越界
         * @throws Base::Exception 实参自身求值失败
         */
        [[nodiscard]] Value argumentValue(std::size_t index) const;

        /**
         * @brief 求出全部实参的取值
         * @return 按书写顺序排列的取值
         * @throws Base::Exception 任一实参求值失败
         */
        [[nodiscard]] std::vector<Value> argumentValues() const;

        /**
         * @brief 取宿主注入的对象解析器
         * @details 回调可据此读取宿主对象，实现需要访问文档模型的函数。
         * @return 对象解析器；未绑定时为空
         */
        [[nodiscard]] IObjectResolver *resolver() const noexcept;

    private:
        friend class CustomFunctionExpression;

        /**
         * @brief 由自定义函数节点构造
         * @param name 函数名
         * @param arguments 实参表达式列表，调用期间有效
         * @param resolver 对象解析器，可为空
         */
        FunctionCall(std::string_view name, const std::vector<ExpressionPtr> &arguments, IObjectResolver *resolver);

        std::string_view                  m_name;      ///< 函数名
        const std::vector<ExpressionPtr> *m_arguments; ///< 实参表达式，所有权在调用节点
        IObjectResolver                  *m_resolver;  ///< 对象解析器，可为空
    };

    /// 宿主自定义函数的求值回调
    using CustomFunction = std::function<Value(const FunctionCall &)>;

    /**
     * @brief 一条自定义函数的登记内容
     * @details 实参个数在建节点时校验，与内建函数同一口径；回调内部的类型与量纲错误由它
     *          自己抛引擎异常，本结构不代为检查。
     */
    struct CustomFunctionSpec
    {
        std::string                name{};          ///< 函数名，区分大小写，须是词法器认可的函数名写法
        CustomFunction             function{};      ///< 求值回调；为空视为非法登记
        std::size_t                minArguments{0}; ///< 最少实参个数
        std::optional<std::size_t> maxArguments{};  ///< 最多实参个数；空表示不限
        std::string                usage{};         ///< 一句话用途说明，供宿主做提示与补全
    };

    /**
     * @brief 自定义函数节点
     * @details 与内建函数节点分开的独立节点：内建函数是一张编译期确定的枚举表，自定义函数
     *          的取值由宿主回调给出，两者的求值路径不该混在一个 switch 里。回调用共享指针持有，
     *          因此节点拷贝后仍指向同一实现；从注册表注销某条函数也不影响已解析出的表达式。
     */
    class CustomFunctionExpression : public Expression
    {
    public:
        /**
         * @brief 构造自定义函数节点
         * @param resolver 对象解析器，可为空；回调要读宿主对象时需要它
         * @param spec 登记内容，通常来自 FunctionRegistry::find()；为空是调用方缺陷
         * @param arguments 实参，按值接收所有权
         * @throws EvaluationError 实参个数与登记要求不符
         * @throws std::invalid_argument spec 为空或回调未设置
         */
        explicit CustomFunctionExpression(IObjectResolver *resolver, std::shared_ptr<const CustomFunctionSpec> spec, std::vector<ExpressionPtr> arguments = std::vector<ExpressionPtr>());

        ~CustomFunctionExpression() override;

        /**
         * @brief 取登记内容
         * @return 登记内容，含函数名与回调
         */
        [[nodiscard]] const CustomFunctionSpec &spec() const noexcept;

        /**
         * @brief 取函数名
         * @return 函数名
         */
        [[nodiscard]] std::string name() const;

        /**
         * @brief 取实参
         * @return 实参列表
         */
        [[nodiscard]] const std::vector<ExpressionPtr> &getArguments() const noexcept;

        /**
         * @brief 校验实参个数并给出可操作文案
         * @details 供解析器在建节点前先校验，也使节点构造与解析期报出同一份文案。
         * @param spec 登记内容
         * @param argumentCount 实参个数
         * @throws EvaluationError 实参个数与登记要求不符
         */
        static void checkArgumentCount(const CustomFunctionSpec &spec, std::size_t argumentCount);

        /**
         * @brief 化简本节点
         * @details 重写 Expression::simplify()：全部实参都能化简成常量数值时直接求值折叠，
         *          否则用简化后的实参重建节点；宿主回调可能有副作用，因此不提前求值。
         * @return 常量节点或重建的自定义函数节点
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /**
         * @brief 节点种类名
         * @details 重写 Expression::nodeName()：固定返回 "CustomFunction"，使自定义函数
         *          不与内建函数节点在相等判定中混同。
         * @return "CustomFunction"
         */
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 重写 Expression::evaluateNode()：组装调用上下文后转交宿主回调，
         *          分量由基类统一作用在回调给出的取值上，使 myVector(x; y; z)[0] 这类写法可用。
         * @return 回调给出的取值
         * @throws Base::Exception 回调抛出的异常
         */
        [[nodiscard]] Value evaluateNode() const override;

        /**
         * @brief 追加本节点的文本
         * @details 重写 Expression::appendText()：写成「函数名(实参; …)」，与内建函数同一
         *          分隔符约定，使文本能在同一注册表下解析回同一棵树。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 Expression::copyNode()：登记内容按共享指针复制，实参由本节点逐一深拷贝。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;

        /**
         * @brief 本节点后面能否直接跟分量
         * @details 重写 Expression::isIndexable()：回调可以返回向量或文本，其值能按下标取
         *          子值，因此返回 true（基类默认为 false）。
         * @return 恒为 true
         */
        [[nodiscard]] bool isIndexable() const override;

        /**
         * @brief 递归收集全部实参里的变量引用
         * @details 覆写基类的空实现：自定义函数的实参同样可能引用宿主属性，漏收会让宿主
         *          少建依赖；按实参顺序追加。
         * @param collectedReferences 输出：追加各实参的引用
         */
        void collectReferencesInto(std::vector<VariableReference> &collectedReferences) const override;

    private:
        std::shared_ptr<const CustomFunctionSpec> m_spec;      ///< 登记内容，与来源注册表解耦
        std::vector<ExpressionPtr>                m_arguments; ///< 实参
    };

    /**
     * @brief 宿主自定义函数的注册表
     * @details 解析器在内置函数表里查不到名字时来此查询，因此宿主无需改动库源码就能扩充
     *          函数集。内置函数名不可被覆盖或遮蔽：同一个名字有两套求值路径会让表达式文本
     *          的含义取决于登记顺序，不利于排查。
     *          登记与查询可跨线程调用；节点一旦建好就自持实现，之后注销不影响已解析的表达式。
     *          库内解析器的默认重载走 global()；需要隔离（如插件、单元测试）时构造自己的
     *          实例并显式传给解析器。
     */
    class FunctionRegistry
    {
    public:
        FunctionRegistry() = default;

        FunctionRegistry(const FunctionRegistry &) = delete;

        FunctionRegistry &operator=(const FunctionRegistry &) = delete;

        FunctionRegistry(FunctionRegistry &&) = delete;

        FunctionRegistry &operator=(FunctionRegistry &&) = delete;

        /**
         * @brief 取进程级默认注册表
         * @details 不带注册表参数的 parse() 与 tryParse() 重载查询本表。
         * @return 默认注册表，进程生命周期内有效
         */
        [[nodiscard]] static FunctionRegistry &global();

        /**
         * @brief 登记一条自定义函数
         * @details 名字合法性以词法器的函数名规则为准：首字符须是字母类字符，其后允许
         *          字母、下划线、数字与非 ASCII 字母；同时要求不与内置函数重名、不与已登记
         *          的自定义函数重名（要换实现先 unregisterFunction()）。
         * @param spec 登记内容
         * @return 成功返回空值；失败返回中文可操作文案
         */
        [[nodiscard]] std::expected<void, std::string> registerFunction(CustomFunctionSpec spec);

        /**
         * @brief 注销一条自定义函数
         * @details 只影响之后解析的表达式，已解析出的节点仍持有原实现。
         * @param name 函数名
         * @return 确有该条目并删除时返回 true
         */
        [[nodiscard]] bool unregisterFunction(std::string_view name);

        /**
         * @brief 按名查询登记内容
         * @param name 函数名，区分大小写
         * @return 登记内容；未登记时返回空指针
         */
        [[nodiscard]] std::shared_ptr<const CustomFunctionSpec> find(std::string_view name) const;

        /**
         * @brief 是否登记过该名字
         * @param name 函数名
         * @return 已登记时返回 true
         */
        [[nodiscard]] bool contains(std::string_view name) const;

        /**
         * @brief 列出全部已登记的函数名
         * @return 按字典序排列的函数名，供宿主展示可用函数
         */
        [[nodiscard]] std::vector<std::string> names() const;

        /**
         * @brief 取已登记的函数条数
         * @return 条数
         */
        [[nodiscard]] std::size_t size() const;

        /**
         * @brief 清空注册表
         * @details 供宿主卸载插件与用例收尾使用；已解析出的表达式不受影响。
         */
        void clear();

    private:
        std::unordered_map<std::string, std::shared_ptr<const CustomFunctionSpec>> m_entries; ///< 函数名到登记内容
        mutable std::shared_mutex                                                  m_mutex;   ///< 保护 m_entries，读共享写独占
    };

} // namespace ExpressionEngine::Expression
