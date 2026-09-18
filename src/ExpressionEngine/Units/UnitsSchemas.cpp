#include <ExpressionEngine/Units/UnitsSchemas.h>

#include <algorithm>
#include <format>
#include <iterator>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Units
{
    UnitsSchemas::UnitsSchemas(const UnitsSchemasDataPack &pack) :
        m_pack{pack}, m_currentSchema{std::make_unique<UnitsSchema>(spec())}, m_fractionDenominator{pack.defaultDenominator}
    {
    }

    std::size_t UnitsSchemas::count() const
    {
        return m_pack.specs.size();
    }

    std::vector<std::string> UnitsSchemas::collect(const std::function<std::string(UnitsSchemaSpec)> &projector)
    {
        // 对外列表按方案编号排序，保证调用方看到的顺序与编号一致
        auto sortedSpecs = m_pack.specs;
        std::sort(sortedSpecs.begin(), sortedSpecs.end(), [](const UnitsSchemaSpec &left, const UnitsSchemaSpec &right) { return left.num < right.num; });

        std::vector<std::string> values;
        values.reserve(sortedSpecs.size());
        std::transform(sortedSpecs.begin(), sortedSpecs.end(), std::back_inserter(values), projector);

        return values;
    }

    std::vector<std::string> UnitsSchemas::names()
    {
        return collect([](const UnitsSchemaSpec &spec) { return spec.name; });
    }

    std::vector<std::string> UnitsSchemas::descriptions()
    {
        // 本库不带翻译体系，描述按数据表原文返回；需要本地化的宿主可自行包装
        return collect([](const UnitsSchemaSpec &spec) { return spec.description == nullptr ? std::string{} : std::string{spec.description}; });
    }

    std::size_t UnitsSchemas::getDecimals() const
    {
        return m_pack.defaultDecimals;
    }

    std::size_t UnitsSchemas::defaultFractionDenominator() const
    {
        return m_fractionDenominator;
    }

    void UnitsSchemas::setDefaultFractionDenominator(const std::size_t denominator)
    {
        m_fractionDenominator = denominator;
    }

    void UnitsSchemas::select()
    {
        makeCurrent(spec());
    }

    void UnitsSchemas::select(const std::string_view name)
    {
        makeCurrent(spec(name));
    }

    void UnitsSchemas::select(const std::size_t num)
    {
        makeCurrent(spec(num));
    }

    UnitsSchema *UnitsSchemas::currentSchema() const
    {
        return m_currentSchema.get();
    }

    void UnitsSchemas::makeCurrent(const UnitsSchemaSpec &spec)
    {
        m_currentSchema = std::make_unique<UnitsSchema>(spec);
    }

    UnitsSchemaSpec UnitsSchemas::findSpec(const std::function<bool(UnitsSchemaSpec)> &predicate)
    {
        const auto found = std::find_if(m_pack.specs.begin(), m_pack.specs.end(), predicate);

        if (found == m_pack.specs.end())
        {
            throw Base::NameError("找不到匹配的单位方案，请用 names() 取可用方案名后重试");
        }

        return *found;
    }

    UnitsSchemaSpec UnitsSchemas::spec()
    {
        return findSpec([](const UnitsSchemaSpec &spec) { return spec.isDefault; });
    }

    UnitsSchemaSpec UnitsSchemas::spec(const std::string_view name)
    {
        return findSpec([name](const UnitsSchemaSpec &spec) { return spec.name == name; });
    }

    UnitsSchemaSpec UnitsSchemas::spec(const std::size_t num)
    {
        return findSpec([num](const UnitsSchemaSpec &spec) { return spec.num == num; });
    }
} // namespace ExpressionEngine::Units
