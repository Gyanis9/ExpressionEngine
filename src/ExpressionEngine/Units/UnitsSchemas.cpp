#include <ExpressionEngine/Units/UnitsSchemas.h>

#include <algorithm>
#include <format>
#include <iterator>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Units
{
    UnitsSchemas::UnitsSchemas(const UnitsSchemasDataPack &pack) :
        m_pack{pack}, m_currentSchema{std::make_unique<UnitsSchema>(specification())}, m_fractionDenominator{pack.defaultDenominator}
    {
    }

    std::size_t UnitsSchemas::count() const
    {
        return m_pack.specifications.size();
    }

    std::vector<std::string> UnitsSchemas::collect(const std::function<std::string(UnitsSchemaSpecification)> &projector)
    {
        // 对外列表按方案编号排序，保证调用方看到的顺序与编号一致
        auto sortedSpecifications = m_pack.specifications;
        std::sort(sortedSpecifications.begin(), sortedSpecifications.end(),
                  [](const UnitsSchemaSpecification &left, const UnitsSchemaSpecification &right) { return left.number < right.number; });

        std::vector<std::string> values;
        values.reserve(sortedSpecifications.size());
        std::transform(sortedSpecifications.begin(), sortedSpecifications.end(), std::back_inserter(values), projector);

        return values;
    }

    std::vector<std::string> UnitsSchemas::names()
    {
        return collect([](const UnitsSchemaSpecification &specification) { return specification.name; });
    }

    std::vector<std::string> UnitsSchemas::descriptions()
    {
        // 本库不带翻译体系，描述按数据表原文返回；需要本地化的宿主可自行包装
        return collect([](const UnitsSchemaSpecification &specification) { return specification.description == nullptr ? std::string{} : std::string{specification.description}; });
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
        makeCurrent(specification());
    }

    void UnitsSchemas::select(const std::string_view name)
    {
        makeCurrent(specification(name));
    }

    void UnitsSchemas::select(const std::size_t schemaNumber)
    {
        makeCurrent(specification(schemaNumber));
    }

    UnitsSchema *UnitsSchemas::currentSchema() const
    {
        return m_currentSchema.get();
    }

    void UnitsSchemas::makeCurrent(const UnitsSchemaSpecification &specification)
    {
        m_currentSchema = std::make_unique<UnitsSchema>(specification);
    }

    UnitsSchemaSpecification UnitsSchemas::findSpecification(const std::function<bool(UnitsSchemaSpecification)> &predicate)
    {
        const auto found = std::find_if(m_pack.specifications.begin(), m_pack.specifications.end(), predicate);

        if (found == m_pack.specifications.end())
        {
            throw Base::NameError("找不到匹配的单位方案，请用 names() 取可用方案名后重试");
        }

        return *found;
    }

    UnitsSchemaSpecification UnitsSchemas::specification()
    {
        return findSpecification([](const UnitsSchemaSpecification &specification) { return specification.isDefault; });
    }

    UnitsSchemaSpecification UnitsSchemas::specification(const std::string_view name)
    {
        return findSpecification([name](const UnitsSchemaSpecification &specification) { return specification.name == name; });
    }

    UnitsSchemaSpecification UnitsSchemas::specification(const std::size_t schemaNumber)
    {
        return findSpecification([schemaNumber](const UnitsSchemaSpecification &specification) { return specification.number == schemaNumber; });
    }
} // namespace ExpressionEngine::Units
