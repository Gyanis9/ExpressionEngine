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

    std::vector<std::string> UnitsSchemas::collect(const std::function<std::string(UnitsSchemaSpecification)> &projector) const
    {
        // 对外列表按方案编号排序，保证调用方看到的顺序与编号一致
        auto sortedSpecifications = m_pack.specifications;
        std::ranges::sort(sortedSpecifications,
                          [](const UnitsSchemaSpecification &left, const UnitsSchemaSpecification &right)
                          {
                              return left.number < right.number;
                          });

        std::vector<std::string> values;
        values.reserve(sortedSpecifications.size());
        std::ranges::transform(sortedSpecifications, std::back_inserter(values), projector);

        return values;
    }

    std::vector<std::string> UnitsSchemas::names() const
    {
        return collect([](const UnitsSchemaSpecification &specification)
        {
            return specification.name;
        });
    }

    std::vector<std::string> UnitsSchemas::descriptions() const
    {
        // 本库不带翻译体系，描述按数据表原文返回；需要本地化的宿主可自行包装
        return collect([](const UnitsSchemaSpecification &specification)
        {
            return specification.description == nullptr ? std::string{} : std::string{specification.description};
        });
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

    UnitsSchemaSpecification UnitsSchemas::findSpecification(const std::function<bool(UnitsSchemaSpecification)> &predicate, const std::string &subject)
    {
        const auto found = std::ranges::find_if(m_pack.specifications, predicate);

        if (found == m_pack.specifications.end())
        {
            // 报错文本要带上查的是什么，否则宿主拿到一段「找不到」也不知道自己传错了哪个名字或编号
            throw Base::NameError(std::format("找不到{}的单位方案，请用 names() 取可用方案名、count() 取方案总数后重试", subject));
        }

        return *found;
    }

    UnitsSchemaSpecification UnitsSchemas::specification()
    {
        const auto isMarkedDefault = [](const UnitsSchemaSpecification &specification)
        {
            return specification.isDefault;
        };

        if (const auto defaultSpecification = std::ranges::find_if(m_pack.specifications, isMarkedDefault);
            defaultSpecification != m_pack.specifications.end())
        {
            return *defaultSpecification;
        }

        // 一个 isDefault 都没有标记时按列表顺序取第一个：宿主只提供单个方案时不必为了构造成功而补标记
        if (m_pack.specifications.empty())
        {
            throw Base::NameError("单位方案数据包里没有任何方案，请至少给出一个方案后重试");
        }

        return m_pack.specifications.front();
    }

    UnitsSchemaSpecification UnitsSchemas::specification(const std::string_view name)
    {
        return findSpecification([name](const UnitsSchemaSpecification &specification)
        {
            return specification.name == name;
        }, std::format("名为 \"{}\"", name));
    }

    UnitsSchemaSpecification UnitsSchemas::specification(const std::size_t schemaNumber)
    {
        return findSpecification([schemaNumber](const UnitsSchemaSpecification &specification)
        {
            return specification.number == schemaNumber;
        }, std::format("编号为 {}", schemaNumber));
    }
} // namespace ExpressionEngine::Units
