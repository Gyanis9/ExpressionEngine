#include <ExpressionEngine/Units/UnitsApi.h>

#include <utility>

namespace ExpressionEngine::Units
{
    std::vector<std::string> UnitsApi::getDescriptions()
    {
        return s_schemas->descriptions();
    }

    std::vector<std::string> UnitsApi::getNames()
    {
        return s_schemas->names();
    }

    std::size_t UnitsApi::count()
    {
        return s_schemas->count();
    }

    bool UnitsApi::isMultiUnitAngle()
    {
        return s_schemas->currentSchema()->isMultiUnitAngle();
    }

    bool UnitsApi::isMultiUnitLength()
    {
        return s_schemas->currentSchema()->isMultiUnitLength();
    }

    std::string UnitsApi::getBasicLengthUnit()
    {
        return s_schemas->currentSchema()->getBasicLengthUnit();
    }

    void UnitsApi::setDecimals(const int precision)
    {
        s_decimals = precision;
    }

    int UnitsApi::getDecimals()
    {
        // 未显式设置时回落到方案默认值，保证调用方总能拿到一个可用精度
        return s_decimals < 0 ? static_cast<int>(s_schemas->getDecimals()) : s_decimals;
    }

    void UnitsApi::setDenominator(const int denominator)
    {
        s_denominator = denominator;
    }

    int UnitsApi::getDenominator()
    {
        return s_denominator < 0 ? static_cast<int>(s_schemas->defaultFractionDenominator()) : s_denominator;
    }

    std::unique_ptr<UnitsSchema> UnitsApi::createSchema(const std::size_t num)
    {
        return std::make_unique<UnitsSchema>(s_schemas->spec(num));
    }

    void UnitsApi::setSchema(const std::string &name)
    {
        s_schemas->select(name);
    }

    void UnitsApi::setSchema(const std::size_t num)
    {
        s_schemas->select(num);
    }

    std::string UnitsApi::schemaTranslate(const Quantity &quant, double &factor, std::string &unitString)
    {
        return s_schemas->currentSchema()->translate(quant, factor, unitString);
    }

    std::string UnitsApi::schemaTranslate(const Quantity &quant, const Base::NumericLocaleContext &formatting, double &factor, std::string &unitString)
    {
        return s_schemas->currentSchema()->translate(quant, formatting, factor, unitString);
    }

    std::string UnitsApi::schemaTranslate(const Quantity &quant)
    {
        double      unusedFactor{};
        std::string unusedUnitString;
        return s_schemas->currentSchema()->translate(quant, unusedFactor, unusedUnitString);
    }

    std::string UnitsApi::schemaTranslate(const Quantity &quant, const Base::NumericLocaleContext &formatting)
    {
        double      unusedFactor{};
        std::string unusedUnitString;
        return s_schemas->currentSchema()->translate(quant, formatting, unusedFactor, unusedUnitString);
    }

    std::size_t UnitsApi::getDefaultSchemaNum()
    {
        return s_schemas->spec().num;
    }
} // namespace ExpressionEngine::Units
