#include <ExpressionEngine/Units/UnitsSchema.h>

#include <algorithm>
#include <format>
#include <utility>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Units/UnitsSchemasData.h>

namespace ExpressionEngine::Units
{
    UnitsSchema::UnitsSchema(UnitsSchemaSpecification specification) : m_specification{std::move(specification)}
    {
    }

    std::string UnitsSchema::translate(const Quantity &quant) const
    {
        double      unusedFactor{};
        std::string unusedUnitString;
        return translate(quant, unusedFactor, unusedUnitString);
    }

    std::string UnitsSchema::translate(const Quantity &quant, double &factor, std::string &unitString) const
    {
        return translate(quant, Base::currentNumericLocaleContext(), factor, unitString);
    }

    std::string UnitsSchema::translate(const Quantity &quant, const Base::NumericLocaleContext &formatting, double &factor, std::string &unitString) const
    {
        // 先给出「不换算」的默认结果，方案里没有对应条目时直接沿用
        factor     = 1.0;
        unitString = quant.getUnit().getString();

        if (m_specification.translationSpecifications.empty())
        {
            return toLocale(quant, formatting, factor, unitString);
        }

        const auto unitTypeName = quant.getUnit().getTypeString();
        if (!m_specification.translationSpecifications.contains(unitTypeName))
        {
            return toLocale(quant, formatting, factor, unitString);
        }

        const auto value     = quant.getValue();
        const auto magnitude = std::abs(value);

        // 取第一个「阈值大于待换算值」的条目；阈值 0 是兜底条目，必须排在最后
        const auto isApplicable = [magnitude](const UnitTranslationSpecification &row)
        {
            // 阈值边界上的值（如 1e-9 S/m 正好等于阈值 1e-9）应落到下一个单位，
            // 因此把阈值略微收缩后再比较
            constexpr double relativeEpsilon = 1e-12;
            return row.threshold * (1.0 - relativeEpsilon) > magnitude || row.threshold == 0;
        };

        const auto &candidates        = m_specification.translationSpecifications.at(unitTypeName);
        const auto  unitSpecification = std::find_if(candidates.begin(), candidates.end(), isApplicable);
        if (unitSpecification == candidates.end())
        {
            throw Base::ExpressionError(std::format("单位方案 {} 的 {} 换算表里没有匹配条目，也没有阈值 0 "
                                                    "的兜底条目，请补一条兜底条目后重试",
                                                    m_specification.name, unitTypeName));
        }

        // 换算因子为 0 表示 unitString 写的是特殊函数名，交给特殊函数接管
        if (unitSpecification->factor == 0)
        {
            const QuantityFormat &format = quant.getFormat();
            return UnitsSchemasData::runSpecial(unitSpecification->unitString, value, static_cast<std::size_t>(format.getPrecision()),
                                                static_cast<std::size_t>(format.getDenominator()), factor, unitString);
        }

        factor     = unitSpecification->factor;
        unitString = unitSpecification->unitString;

        return toLocale(quant, formatting, factor, unitString);
    }

    std::string UnitsSchema::toLocale(const Quantity &quant, const Base::NumericLocaleContext &formatting, const double factor, const std::string &unitString)
    {
        const QuantityFormat &format         = quant.getFormat();
        const double          convertedValue = quant.getValue() / factor;

        // 非有限值无法套用区域数字格式，直接按最短往返写法输出，避免排版成 "nan"
        const std::string valueString = std::isfinite(convertedValue) ? formatNumericValue(convertedValue, format.getPrecision(), format.format,
                                                                                           (format.option & QuantityFormat::OmitGroupSeparator) != 0, formatting)
                                                                      : std::to_string(convertedValue);

        // 角度与英制的单位符号是上标式记号，与数值之间不留空格才符合书写习惯
        const auto needsSeparator = [](const std::string &unit) { return !unit.empty() && unit != "°" && unit != "″" && unit != "′" && unit != "\"" && unit != "'"; };

        return std::format("{}{}{}", valueString, needsSeparator(unitString) ? " " : "", unitString);
    }

    bool UnitsSchema::isMultiUnitLength() const
    {
        return m_specification.isMultiUnitLength;
    }

    bool UnitsSchema::isMultiUnitAngle() const
    {
        return m_specification.isMultiUnitAngle;
    }

    std::string UnitsSchema::getBasicLengthUnit() const
    {
        return m_specification.basicLengthUnitString;
    }

    std::string UnitsSchema::getName() const
    {
        return m_specification.name;
    }

    std::string UnitsSchema::getDescription() const
    {
        return m_specification.description == nullptr ? std::string{} : std::string{m_specification.description};
    }

    int UnitsSchema::getNumber() const
    {
        return static_cast<int>(m_specification.number);
    }
} // namespace ExpressionEngine::Units
