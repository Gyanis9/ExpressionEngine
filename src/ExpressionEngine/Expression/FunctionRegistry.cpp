#include <ExpressionEngine/Expression/FunctionRegistry.h>

#include <algorithm>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Expression/ExpressionLexer.h>

namespace ExpressionEngine::Expression
{

    namespace
    {

        /// 把登记要求写成报错文案里的参数个数说明
        [[nodiscard]] std::string argumentRequirement(const CustomFunctionSpec &spec)
        {
            if (!spec.maxArguments.has_value())
            {
                // 上限未登记即不限个数
                return spec.minArguments == 0 ? "任意个参数" : std::format("至少 {} 个参数", spec.minArguments);
            }
            if (*spec.maxArguments == spec.minArguments)
            {
                return std::format("恰好 {} 个参数", spec.minArguments);
            }
            return std::format("{} 到 {} 个参数", spec.minArguments, *spec.maxArguments);
        }

    } // namespace

    //
    // 调用上下文
    //

    FunctionCall::FunctionCall(const std::string_view name, const std::vector<ExpressionPtr> &arguments, IObjectResolver *resolver) : m_name(name), m_arguments(&arguments), m_resolver(resolver)
    {
    }

    std::string_view FunctionCall::name() const noexcept
    {
        return m_name;
    }

    std::size_t FunctionCall::argumentCount() const noexcept
    {
        return m_arguments->size();
    }

    const Expression *FunctionCall::argument(const std::size_t index) const noexcept
    {
        // 越界返回空指针：回调拿子表达式时要先判个数，这里不替它抛错，避免惰性求值路径上多一次异常
        if (index >= m_arguments->size())
        {
            return nullptr;
        }
        return (*m_arguments)[index].get();
    }

    Value FunctionCall::argumentValue(const std::size_t index) const
    {
        if (index >= m_arguments->size())
        {
            throw Base::IndexError(std::format("函数 '{}' 只有 {} 个实参，却要取第 {} 个；请按实参个数取用", m_name, m_arguments->size(), index + 1));
        }
        return (*m_arguments)[index]->evaluate();
    }

    std::vector<Value> FunctionCall::argumentValues() const
    {
        std::vector<Value> values;
        values.reserve(m_arguments->size());
        for (const auto &argument: *m_arguments)
        {
            values.push_back(argument->evaluate());
        }
        return values;
    }

    IObjectResolver *FunctionCall::resolver() const noexcept
    {
        return m_resolver;
    }

    //
    // 自定义函数节点
    //

    CustomFunctionExpression::CustomFunctionExpression(IObjectResolver *resolver, std::shared_ptr<const CustomFunctionSpec> spec, std::vector<ExpressionPtr> arguments) :
        Expression(resolver), m_spec(std::move(spec)), m_arguments(std::move(arguments))
    {
        if (m_spec == nullptr)
        {
            throw std::invalid_argument("CustomFunctionExpression: 登记内容为空；请传入 FunctionRegistry::find() 的结果");
        }
        if (!m_spec->function)
        {
            throw std::invalid_argument(std::format("CustomFunctionExpression: 函数 '{}' 的回调未设置；请在登记时给出求值回调", m_spec->name));
        }
        checkArgumentCount(*m_spec, m_arguments.size());
    }

    CustomFunctionExpression::~CustomFunctionExpression() = default;

    const CustomFunctionSpec &CustomFunctionExpression::spec() const noexcept
    {
        return *m_spec;
    }

    std::string CustomFunctionExpression::name() const
    {
        return m_spec->name;
    }

    const std::vector<ExpressionPtr> &CustomFunctionExpression::getArguments() const noexcept
    {
        return m_arguments;
    }

    void CustomFunctionExpression::checkArgumentCount(const CustomFunctionSpec &spec, const std::size_t argumentCount)
    {
        const std::size_t maximum = spec.maxArguments.value_or(std::numeric_limits<std::size_t>::max());
        if (argumentCount >= spec.minArguments && argumentCount <= maximum)
        {
            return;
        }
        // 与内建函数同一口径：个数不符在登记要求不满足时报出，文案说明需要的个数
        throw EvaluationError(std::format("{}() 的参数个数不正确：需要{}，实际收到 {} 个；请调整调用处的参数个数", spec.name, argumentRequirement(spec), argumentCount));
    }

    ExpressionPtr CustomFunctionExpression::simplify() const
    {
        bool                       allConstant = true;
        std::vector<ExpressionPtr> simplifiedArguments;
        simplifiedArguments.reserve(m_arguments.size());

        for (const auto &argument: m_arguments)
        {
            ExpressionPtr simplified = argument->simplify();
            if (!simplified->isConstantNumeric())
            {
                allConstant = false;
            }
            simplifiedArguments.push_back(std::move(simplified));
        }

        if (allConstant)
        {
            // 实参全为常量时结果必然恒定，折叠成一个常量节点
            return evaluateToConstantNode();
        }
        return carryComponents(std::make_unique<CustomFunctionExpression>(resolver(), m_spec, std::move(simplifiedArguments)));
    }

    std::string_view CustomFunctionExpression::nodeName() const
    {
        return "CustomFunction";
    }

    Value CustomFunctionExpression::evaluateNode() const
    {
        // 分量由基类统一作用在本节点求出的取值上：myVector(1; 2; 3)[1] 取到 y
        const FunctionCall call(m_spec->name, m_arguments, resolver());
        return m_spec->function(call);
    }

    void CustomFunctionExpression::appendText(std::string &text, const bool persistent, int) const
    {
        text += m_spec->name;
        text += '(';
        for (std::size_t index = 0; index < m_arguments.size(); ++index)
        {
            if (index != 0)
            {
                // 实参分隔符与内建函数一致，用分号避免与小数点逗号混淆
                text += "; ";
            }
            text += m_arguments[index]->toString(persistent);
        }
        text += ')';
    }

    ExpressionPtr CustomFunctionExpression::copyNode() const
    {
        std::vector<ExpressionPtr> arguments;
        arguments.reserve(m_arguments.size());
        for (const auto &argument: m_arguments)
        {
            arguments.push_back(argument->copy());
        }
        return std::make_unique<CustomFunctionExpression>(resolver(), m_spec, std::move(arguments));
    }

    bool CustomFunctionExpression::isIndexable() const
    {
        // 回调可以返回向量或文本，其值能按下标继续取子值
        return true;
    }

    void CustomFunctionExpression::collectReferencesInto(std::vector<VariableReference> &collectedReferences) const
    {
        for (const auto &argument: m_arguments)
        {
            collectReferencesFrom(argument.get(), collectedReferences);
        }
    }

    //
    // 函数注册表
    //

    FunctionRegistry &FunctionRegistry::global()
    {
        static FunctionRegistry globalRegistry;
        return globalRegistry;
    }

    std::expected<void, std::string> FunctionRegistry::registerFunction(CustomFunctionSpec spec)
    {
        if (!spec.function)
        {
            return std::unexpected(std::format("自定义函数 '{}' 的回调未设置；请给出 Value(const FunctionCall&) 的可调用对象", spec.name));
        }
        if (!isFunctionNameText(spec.name))
        {
            return std::unexpected(std::format("函数名 '{}' 不是词法器可用的函数名：首字符须是字母（不能是下划线或数字），其后允许字母、数字、下划线与非 ASCII 字母，"
                                               "不允许空格、'@' 与减号；请改名后重新登记",
                                               spec.name));
        }
        if (FunctionExpression::functionFromName(spec.name) != FunctionExpression::Function::None)
        {
            return std::unexpected(std::format("函数名 '{}' 与内置函数重名；内置函数不可覆盖，请改用其它名字", spec.name));
        }
        if (spec.maxArguments.has_value() && *spec.maxArguments < spec.minArguments)
        {
            return std::unexpected(std::format("自定义函数 '{}' 的参数个数要求不成立：最多 {} 个却要求至少 {} 个；请修正登记", spec.name, *spec.maxArguments, spec.minArguments));
        }

        const std::unique_lock lock(m_mutex);
        if (m_entries.contains(spec.name))
        {
            return std::unexpected(std::format("自定义函数 '{}' 已登记；要替换实现请先 unregisterFunction(\"{}\")", spec.name, spec.name));
        }
        // 先固化成 shared_ptr 再取键：同一表达式里实参求值顺序未定，两边都 move(spec) 会读空
        auto sharedSpec = std::make_shared<const CustomFunctionSpec>(std::move(spec));
        m_entries.emplace(sharedSpec->name, std::move(sharedSpec));
        return {};
    }

    bool FunctionRegistry::unregisterFunction(const std::string_view name)
    {
        const std::unique_lock lock(m_mutex);
        // 键是 std::string 且哈希不支持异构查找，这里显式物化一次；查询只发生在解析期
        return m_entries.erase(std::string{name}) != 0;
    }

    std::shared_ptr<const CustomFunctionSpec> FunctionRegistry::find(const std::string_view name) const
    {
        const std::shared_lock lock(m_mutex);
        const auto             entry = m_entries.find(std::string{name});
        return entry == m_entries.end() ? nullptr : entry->second;
    }

    bool FunctionRegistry::contains(const std::string_view name) const
    {
        return find(name) != nullptr;
    }

    std::vector<std::string> FunctionRegistry::names() const
    {
        const std::shared_lock lock(m_mutex);

        std::vector<std::string> collected;
        collected.reserve(m_entries.size());
        for (const auto &[name, spec]: m_entries)
        {
            collected.push_back(name);
        }
        // 字典序输出，宿主展示可用函数时顺序稳定
        std::sort(collected.begin(), collected.end());
        return collected;
    }

    std::size_t FunctionRegistry::size() const
    {
        const std::shared_lock lock(m_mutex);
        return m_entries.size();
    }

    void FunctionRegistry::clear()
    {
        const std::unique_lock lock(m_mutex);
        m_entries.clear();
    }

} // namespace ExpressionEngine::Expression
