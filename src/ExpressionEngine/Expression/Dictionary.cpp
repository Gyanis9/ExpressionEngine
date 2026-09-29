#include <ExpressionEngine/Expression/Dictionary.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ExpressionEngine::Expression
{


    Dictionary::Entry::Entry(std::string name, Value value) : m_name(std::move(name)), m_value(std::move(value)), m_readOnly(false)
    {
    }

    std::string_view Dictionary::Entry::name() const
    {
        return m_name;
    }

    std::string_view Dictionary::Entry::typeName() const
    {
        return valueTypeName(m_value);
    }

    std::optional<Value> Dictionary::Entry::value() const
    {
        return m_value;
    }

    bool Dictionary::Entry::setValue(const Value &newValue)
    {
        if (m_readOnly)
        {
            return false;
        }
        m_value = newValue;
        return true;
    }

    bool Dictionary::Entry::isReadOnly() const
    {
        return m_readOnly;
    }

    void Dictionary::Entry::setReadOnly(bool readOnly)
    {
        m_readOnly = readOnly;
    }

    // 键和 Entry 内部都要放这个名字，两份拷贝省不掉；按值传参是公开签名的一部分（宿主可以传临时串），
    // 不改成 const 引用。也不在这里 move 进 try_emplace：两个实参读的是同一个 name，而函数实参的
    // 求值顺序未定序，移动其中一个就可能让另一个读到空串。
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    Dictionary::Entry &Dictionary::define(std::string name, Value value)
    {
        const auto [position, inserted] = m_entries.try_emplace(name, Entry{name, value});
        if (!inserted)
        {
            // 已定义过就只覆盖取值；只读标记保持不变，重新 define 不会悄悄放开写入
            position->second.m_value = std::move(value);
        }
        return position->second;
    }

    Dictionary::Entry *Dictionary::find(const std::string_view name)
    {
        const auto position = m_entries.find(std::string{name});
        return position == m_entries.end() ? nullptr : &position->second;
    }

    bool Dictionary::contains(const std::string_view name) const
    {
        return m_entries.contains(std::string{name});
    }

    bool Dictionary::erase(const std::string_view name)
    {
        return m_entries.erase(std::string{name}) != 0;
    }

    Dictionary &Dictionary::addObject(std::string name, std::unique_ptr<Dictionary> child)
    {
        Dictionary &stored = *child;
        stored.m_name      = std::move(name);
        m_objects.insert_or_assign(std::string{stored.m_name}, std::move(child));
        // std::map 的节点地址稳定，交完所有权后这个引用仍然有效
        return stored;
    }

    Dictionary *Dictionary::findObject(const std::string_view name)
    {
        const auto position = m_objects.find(std::string{name});
        return position == m_objects.end() ? nullptr : position->second.get();
    }

    void Dictionary::clear()
    {
        m_entries.clear();
        m_objects.clear();
    }

    std::string_view Dictionary::name() const
    {
        return m_name;
    }

    std::string_view Dictionary::label() const
    {
        return {};
    }

    IProperty *Dictionary::findProperty(const std::string_view propertyName)
    {
        return find(propertyName);
    }

    std::vector<std::string> Dictionary::propertyNames() const
    {
        std::vector<std::string> collected;
        collected.reserve(m_entries.size());
        for (const auto &[key, entry]: m_entries)
        {
            collected.push_back(key);
        }
        return collected;
    }

    std::string_view Dictionary::documentName() const
    {
        return {};
    }

    IObject *Dictionary::resolve(const std::string_view documentName, const std::string_view objectName)
    {
        if (!documentName.empty())
        {
            // 字典不分文档，带文档名的引用交给宿主自己的解析器
            return nullptr;
        }
        if (objectName.empty())
        {
            return this;
        }
        return findObject(objectName);
    }

    std::vector<std::string> Dictionary::objectNames(const std::string_view) const
    {
        std::vector<std::string> collected;
        collected.reserve(m_objects.size());
        for (const auto &[key, child]: m_objects)
        {
            collected.push_back(key);
        }
        return collected;
    }

} // namespace ExpressionEngine::Expression
