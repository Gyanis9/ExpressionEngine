#include <ExpressionEngine/Units/Quantity.h>

#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <string_view>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/NumericInput.h>
#include <ExpressionEngine/Base/Tools.h>
#include <ExpressionEngine/Units/QuantityParser.h>
#include <ExpressionEngine/Units/UnitsApi.h>
#include <ExpressionEngine/Units/UnitsConvData.h>
#include <ExpressionEngine/Units/UnitsSchema.h>

namespace ExpressionEngine::Units {
QuantityFormat::QuantityFormat()
    // 默认不输出也不接受分组分隔符：数值以机器可读为先
    : option(OmitGroupSeparator | RejectGroupSeparator), format(NumberFormat::Fixed),
      m_precision(-1), m_denominator(-1) {
}

QuantityFormat::QuantityFormat(QuantityFormat::NumberFormat format, int decimals)
    : option(OmitGroupSeparator | RejectGroupSeparator), format(format), m_precision(decimals),
      m_denominator(-1) {
}

int QuantityFormat::getPrecision() const {
    return m_precision < 0 ? UnitsApi::getDecimals() : m_precision;
}

int QuantityFormat::getDenominator() const {
    return m_denominator < 0 ? UnitsApi::getDenominator() : m_denominator;
}

Quantity::Quantity() : m_value{0.0} {
}

Quantity::Quantity(double value, const Unit& unit) : m_value{value}, m_unit{unit} {
}

Quantity::Quantity(double value, const std::string& unit) {
    if (unit.empty()) {
        m_value = value;
        m_unit = Unit();
        return;
    }

    // 单位文本按表达式解析；解析失败时退化为无量纲，调用方需要感知失败请直接用 parse()
    try {
        const auto parsedUnit = parse(unit);
        m_value = value * parsedUnit.getValue();
        m_unit = parsedUnit.getUnit();
    } catch (const Base::ParserError&) {
        m_value = 0.0;
        m_unit = Unit();
    }
}

double Quantity::getValueAs(const Quantity& other) const {
    return m_value / other.getValue();
}

bool Quantity::operator==(const Quantity& that) const {
    return m_value == that.m_value && m_unit == that.m_unit;
}

bool Quantity::operator!=(const Quantity& that) const {
    return !(*this == that);
}

bool Quantity::operator<(const Quantity& that) const {
    // 量纲不同时大小无意义，宁可报错也不按数值硬比
    if (m_unit != that.m_unit) {
        throw Base::UnitsMismatchError("比较两个量的大小时单位必须一致，请先换算成同一单位");
    }

    return m_value < that.m_value;
}

bool Quantity::operator>(const Quantity& that) const {
    if (m_unit != that.m_unit) {
        throw Base::UnitsMismatchError("比较两个量的大小时单位必须一致，请先换算成同一单位");
    }

    return m_value > that.m_value;
}

bool Quantity::operator<=(const Quantity& that) const {
    if (m_unit != that.m_unit) {
        throw Base::UnitsMismatchError("比较两个量的大小时单位必须一致，请先换算成同一单位");
    }

    return m_value <= that.m_value;
}

bool Quantity::operator>=(const Quantity& that) const {
    if (m_unit != that.m_unit) {
        throw Base::UnitsMismatchError("比较两个量的大小时单位必须一致，请先换算成同一单位");
    }

    return m_value >= that.m_value;
}

Quantity Quantity::operator*(const Quantity& other) const {
    return Quantity(m_value * other.m_value, m_unit * other.m_unit);
}

Quantity Quantity::operator*(double factor) const {
    return Quantity(m_value * factor, m_unit);
}

Quantity Quantity::operator/(const Quantity& other) const {
    return Quantity(m_value / other.m_value, m_unit / other.m_unit);
}

Quantity Quantity::operator/(double factor) const {
    return Quantity(m_value / factor, m_unit);
}

Quantity Quantity::pow(const Quantity& exponent) const {
    if (!exponent.isDimensionless()) {
        throw Base::UnitsMismatchError("幂次必须是无量纲的纯数字，请去掉指数上的单位");
    }

    // 幂次交给 Unit::pow() 按实数处理：不是整数时它会报错，不在这里静默取整
    return Quantity(std::pow(m_value, exponent.m_value), m_unit.pow(exponent.m_value));
}

Quantity Quantity::pow(double exponent) const {
    return Quantity(std::pow(m_value, exponent), m_unit.pow(exponent));
}

Quantity Quantity::operator+(const Quantity& other) const {
    if (m_unit != other.m_unit) {
        throw Base::UnitsMismatchError("相加的两个量单位必须一致，请先换算成同一单位");
    }

    return Quantity(m_value + other.m_value, m_unit);
}

Quantity& Quantity::operator+=(const Quantity& other) {
    if (m_unit != other.m_unit) {
        throw Base::UnitsMismatchError("相加的两个量单位必须一致，请先换算成同一单位");
    }

    m_value += other.m_value;
    return *this;
}

Quantity Quantity::operator-(const Quantity& other) const {
    if (m_unit != other.m_unit) {
        throw Base::UnitsMismatchError("相减的两个量单位必须一致，请先换算成同一单位");
    }

    return Quantity(m_value - other.m_value, m_unit);
}

Quantity& Quantity::operator-=(const Quantity& other) {
    if (m_unit != other.m_unit) {
        throw Base::UnitsMismatchError("相减的两个量单位必须一致，请先换算成同一单位");
    }

    m_value -= other.m_value;
    return *this;
}

Quantity Quantity::operator-() const {
    return Quantity(-m_value, m_unit);
}

std::string Quantity::toString(const QuantityFormat& format) const {
    return std::format("'{} {}'", toNumber(format), m_unit.getString());
}

std::string Quantity::toNumber(const QuantityFormat& format) const {
    // 记数法直接映射到 std::format 的呈现方式，精度取自格式设置
    switch (format.format) {
    case QuantityFormat::NumberFormat::Fixed:
        return std::format("{:.{}f}", m_value, format.getPrecision());
    case QuantityFormat::NumberFormat::Scientific:
        return std::format("{:.{}e}", m_value, format.getPrecision());
    default:
        return std::format("{:.{}g}", m_value, format.getPrecision());
    }
}

std::string Quantity::getUserString() const {
    double unusedFactor{};
    std::string unusedUnitString;
    return getUserString(unusedFactor, unusedUnitString);
}

std::string Quantity::getUserString(double& factor, std::string& unitString) const {
    return UnitsApi::schemaTranslate(*this, factor, unitString);
}

std::string
Quantity::getUserString(UnitsSchema* schema, double& factor, std::string& unitString) const {
    return schema->translate(*this, factor, unitString);
}

std::string Quantity::getSafeUserString() const {
    auto userString = getUserString();
    if (m_value != 0.0) {
        // 用户串必须能被自己解析回来，否则回退到基准单位写法，避免回填表达式时失真
        bool needsFallback{false};
        try {
            needsFallback =
                parseUserInput(userString, Base::currentNumericLocaleContext()).getValue() == 0;
        } catch (const Base::ParserError&) {
            needsFallback = true;
        }

        if (needsFallback) {
            const auto unitText = m_unit.getString();
            userString = std::format("{}{}{}", m_value, unitText.empty() ? "" : " ", unitText);
        }
    }

    return Base::Tools::escapeQuotesFromString(userString);
}

bool Quantity::isDimensionless() const {
    return m_unit == Unit::One;
}

bool Quantity::isDimensionlessOrUnit(const Unit& unit) const {
    return isDimensionless() || m_unit == unit;
}

bool Quantity::isValid() const {
    return !std::isnan(m_value);
}

void Quantity::setInvalid() {
    m_value = std::numeric_limits<double>::quiet_NaN();
}

namespace {
/// 判断从 position 起是否为给定片段
[[nodiscard]] bool
startsAt(const std::string_view input, const std::size_t position, const std::string_view value) {
    return !value.empty() && position + value.size() <= input.size() &&
           input.substr(position, value.size()) == value;
}

/// 判断从 position 起是否是一个区域化数字的开头（数字、带符号数字或以小数点开头）
[[nodiscard]] bool startsNumericToken(const std::string_view input,
                                      const std::size_t position,
                                      const Base::NumericLocaleContext& locale) {
    if (position >= input.size()) {
        return false;
    }

    int digit = 0;
    std::size_t digitLength = 0;
    if (Base::localizedDigitAt(input, position, locale, digit, digitLength)) {
        return true;
    }

    const auto digitFollows = [&input, &locale](const std::size_t offset) {
        int nextDigit = 0;
        std::size_t nextDigitLength = 0;
        return offset < input.size() &&
               Base::localizedDigitAt(input, offset, locale, nextDigit, nextDigitLength);
    };

    if (input[position] == '.') {
        return digitFollows(position + 1);
    }
    if (startsAt(input, position, locale.decimalSeparator)) {
        return digitFollows(position + locale.decimalSeparator.size());
    }

    // 符号后面必须紧跟数字或小数点，否则符号属于表达式运算符
    for (const std::string_view sign : {std::string_view{"+"},
                                        std::string_view{"-"},
                                        std::string_view{locale.positiveSign},
                                        std::string_view{locale.negativeSign}}) {
        if (startsAt(input, position, sign)) {
            const auto next = position + sign.size();
            return digitFollows(next) || (next < input.size() && input[next] == '.') ||
                   startsAt(input, next, locale.decimalSeparator);
        }
    }

    return false;
}

/// 把用户输入里的区域化数字改写成与区域无关的标准写法，其余字节原样保留
[[nodiscard]] std::string normalizeQuantityInput(const std::string_view input,
                                                 const Base::NumericLocaleContext& locale) {
    std::string normalized;
    normalized.reserve(input.size());

    std::size_t position = 0;
    while (position < input.size()) {
        // 方括号注释整段照抄：里面即使是「看起来像数字」的内容也不扫描
        if (input[position] == '[') {
            const auto closing = input.find(']', position + 1);
            if (closing == std::string_view::npos) {
                normalized.append(input.substr(position));
                break;
            }
            const auto length = closing + 1 - position;
            normalized.append(input.substr(position, length));
            position += length;
            continue;
        }

        if (!startsNumericToken(input, position, locale)) {
            normalized.push_back(input[position++]);
            continue;
        }

        const auto result = Base::scanLocalizedNumber(
            input.substr(position), locale, Base::NumericSyntaxContext::Standalone);
        if (result.status != Base::LocalizedNumberResult::Status::Complete) {
            throw Base::ParserError(std::format(
                "用户输入第 {} "
                "个字符处的数字不符合当前区域的写法（{}），请按该区域的小数点与分组分隔符重新输入",
                position + 1,
                result.diagnostic.has_value() ? "分隔符或分组位数不正确" : "数字不完整"));
        }

        normalized += result.canonicalText;
        position += result.consumedBytes;
    }

    return normalized;
}
}  // namespace

Quantity Quantity::parseUserInput(const std::string& text,
                                  const Base::NumericLocaleContext& locale) {
    return parse(normalizeQuantityInput(text, locale));
}

Quantity Quantity::parse(const std::string& text) {
    return QuantityParser::parse(text);
}

// === 预定义量 ==============================================================
// clang-format off
using namespace UnitsConvData;

const Quantity Quantity::NanoMetre              ( 1.0e-6                , Unit::Length                  );
const Quantity Quantity::MicroMetre             ( 1.0e-3                , Unit::Length                  );
const Quantity Quantity::MilliMetre             ( 1.0                   , Unit::Length                  );
const Quantity Quantity::CentiMetre             ( 10.0                  , Unit::Length                  );
const Quantity Quantity::DeciMetre              ( 100.0                 , Unit::Length                  );
const Quantity Quantity::Metre                  ( 1.0e3                 , Unit::Length                  );
const Quantity Quantity::KiloMetre              ( 1.0e6                 , Unit::Length                  );

const Quantity Quantity::MilliLiter             ( 1000.0                , Unit::Volume                  );
const Quantity Quantity::Liter                  ( 1.0e6                 , Unit::Volume                  );

const Quantity Quantity::Hertz                  ( 1.0                   , Unit::Frequency               );
const Quantity Quantity::KiloHertz              ( 1.0e3                 , Unit::Frequency               );
const Quantity Quantity::MegaHertz              ( 1.0e6                 , Unit::Frequency               );
const Quantity Quantity::GigaHertz              ( 1.0e9                 , Unit::Frequency               );
const Quantity Quantity::TeraHertz              ( 1.0e12                , Unit::Frequency               );

const Quantity Quantity::MicroGram              ( 1.0e-9                , Unit::Mass                    );
const Quantity Quantity::MilliGram              ( 1.0e-6                , Unit::Mass                    );
const Quantity Quantity::Gram                   ( 1.0e-3                , Unit::Mass                    );
const Quantity Quantity::KiloGram               ( 1.0                   , Unit::Mass                    );
const Quantity Quantity::Ton                    ( 1.0e3                 , Unit::Mass                    );

const Quantity Quantity::Second                 ( 1.0                   , Unit::TimeSpan                );
const Quantity Quantity::Minute                 ( 60.0                  , Unit::TimeSpan                );
const Quantity Quantity::Hour                   ( 3600.0                , Unit::TimeSpan                );

const Quantity Quantity::Ampere                 ( 1.0                   , Unit::ElectricCurrent         );
const Quantity Quantity::NanoAmpere             ( 1.0e-9                , Unit::ElectricCurrent         );
const Quantity Quantity::MicroAmpere            ( 1.0e-6                , Unit::ElectricCurrent         );
const Quantity Quantity::MilliAmpere            ( 0.001                 , Unit::ElectricCurrent         );
const Quantity Quantity::KiloAmpere             ( 1000.0                , Unit::ElectricCurrent         );
const Quantity Quantity::MegaAmpere             ( 1.0e6                 , Unit::ElectricCurrent         );

const Quantity Quantity::Kelvin                 ( 1.0                   , Unit::Temperature             );
const Quantity Quantity::MilliKelvin            ( 0.001                 , Unit::Temperature             );
const Quantity Quantity::MicroKelvin            ( 0.000001              , Unit::Temperature             );

const Quantity Quantity::NanoMole               ( 1e-9                  , Unit::AmountOfSubstance       );
const Quantity Quantity::MicroMole              ( 1e-6                  , Unit::AmountOfSubstance       );
const Quantity Quantity::MilliMole              ( 0.001                 , Unit::AmountOfSubstance       );
const Quantity Quantity::Mole                   ( 1.0                   , Unit::AmountOfSubstance       );

const Quantity Quantity::Candela                ( 1.0                   , Unit::LuminousIntensity       );

const Quantity Quantity::Inch                   ( inch                  , Unit::Length                  );
const Quantity Quantity::Foot                   ( foot                  , Unit::Length                  );
const Quantity Quantity::Thou                   ( inch / 1000           , Unit::Length                  );
const Quantity Quantity::Yard                   ( yard                  , Unit::Length                  );
const Quantity Quantity::Mile                   ( mile                  , Unit::Length                  );

const Quantity Quantity::MilePerHour            ( mile / 3600           , Unit::Velocity                );

const Quantity Quantity::SquareFoot             ( foot * foot           , Unit::Area                    );
const Quantity Quantity::CubicFoot              ( foot * foot * foot    , Unit::Volume                  );

const Quantity Quantity::Pound                  ( pound                 , Unit::Mass                    );
const Quantity Quantity::Ounce                  ( pound / 16            , Unit::Mass                    );
const Quantity Quantity::Stone                  ( pound * 14            , Unit::Mass                    );
const Quantity Quantity::Hundredweights         ( pound * 112           , Unit::Mass                    );

const Quantity Quantity::PoundForce             ( 1000 * poundForce     , Unit::Force                   );

const Quantity Quantity::Newton                 ( 1000.0                , Unit::Force                   );
const Quantity Quantity::MilliNewton            ( 1.0                   , Unit::Force                   );
const Quantity Quantity::KiloNewton             ( 1e+6                  , Unit::Force                   );
const Quantity Quantity::MegaNewton             ( 1e+9                  , Unit::Force                   );

const Quantity Quantity::NewtonPerMeter         ( 1.00                  , Unit::Stiffness               );
const Quantity Quantity::MilliNewtonPerMeter    ( 1e-3                  , Unit::Stiffness               );
const Quantity Quantity::KiloNewtonPerMeter     ( 1e3                   , Unit::Stiffness               );
const Quantity Quantity::MegaNewtonPerMeter     ( 1e6                   , Unit::Stiffness               );

const Quantity Quantity::Pascal                 ( 0.001                 , Unit::Pressure                );
const Quantity Quantity::KiloPascal             ( 1.00                  , Unit::Pressure                );
const Quantity Quantity::MegaPascal             ( 1000.0                , Unit::Pressure                );
const Quantity Quantity::GigaPascal             ( 1e+6                  , Unit::Pressure                );

const Quantity Quantity::MilliBar               ( 0.1                   , Unit::Pressure                );
const Quantity Quantity::Bar                    ( 100.0                 , Unit::Pressure                );

const Quantity Quantity::Torr                   ( 101.325 / 760.0       , Unit::Pressure                );
const Quantity Quantity::mTorr                  ( 101.325 / 760.0 / 1e3 , Unit::Pressure                );
const Quantity Quantity::yTorr                  ( 101.325 / 760.0 / 1e6 , Unit::Pressure                );

const Quantity Quantity::PSI                    ( psi                   , Unit::Pressure                );
const Quantity Quantity::KSI                    ( psi * 1000            , Unit::Pressure                );
const Quantity Quantity::MPSI                   ( psi * 1000000         , Unit::Pressure                );

const Quantity Quantity::Watt                   ( 1e+6                  , Unit::Power                   );
const Quantity Quantity::NanoWatt               ( 1e-3                  , Unit::Power                   );
const Quantity Quantity::MicroWatt              ( 1.0                   , Unit::Power                   );
const Quantity Quantity::MilliWatt              ( 1e+3                  , Unit::Power                   );
const Quantity Quantity::KiloWatt               ( 1e+9                  , Unit::Power                   );
const Quantity Quantity::VoltAmpere             ( 1e+6                  , Unit::Power                   );

const Quantity Quantity::Volt                   ( 1e+6                  , Unit::ElectricPotential       );
const Quantity Quantity::MilliVolt              ( 1e+3                  , Unit::ElectricPotential       );
const Quantity Quantity::KiloVolt               ( 1e+9                  , Unit::ElectricPotential       );

const Quantity Quantity::MegaSiemens            ( 1.0                   , Unit::ElectricalConductance   );
const Quantity Quantity::KiloSiemens            ( 1e-3                  , Unit::ElectricalConductance   );
const Quantity Quantity::Siemens                ( 1e-6                  , Unit::ElectricalConductance   );
const Quantity Quantity::MilliSiemens           ( 1e-9                  , Unit::ElectricalConductance   );
const Quantity Quantity::MicroSiemens           ( 1e-12                 , Unit::ElectricalConductance   );

const Quantity Quantity::Ohm                    ( 1e+6                  , Unit::ElectricalResistance    );
const Quantity Quantity::KiloOhm                ( 1e+9                  , Unit::ElectricalResistance    );
const Quantity Quantity::MegaOhm                ( 1e+12                 , Unit::ElectricalResistance    );

const Quantity Quantity::Coulomb                ( 1.0                   , Unit::ElectricCharge          );

const Quantity Quantity::Tesla                  ( 1.0                   , Unit::MagneticFluxDensity     );
const Quantity Quantity::MilliTesla             ( 1e-3                  , Unit::MagneticFluxDensity     );
const Quantity Quantity::Gauss                  ( 1e-4                  , Unit::MagneticFluxDensity     );

const Quantity Quantity::Weber                  ( 1e6                   , Unit::MagneticFlux            );

const Quantity Quantity::PicoFarad              ( 1e-18                 , Unit::ElectricalCapacitance   );
const Quantity Quantity::NanoFarad              ( 1e-15                 , Unit::ElectricalCapacitance   );
const Quantity Quantity::MicroFarad             ( 1e-12                 , Unit::ElectricalCapacitance   );
const Quantity Quantity::MilliFarad             ( 1e-9                  , Unit::ElectricalCapacitance   );
const Quantity Quantity::Farad                  ( 1e-6                  , Unit::ElectricalCapacitance   );

const Quantity Quantity::NanoHenry              ( 1e-3                  , Unit::ElectricalInductance    );
const Quantity Quantity::MicroHenry             ( 1.0                   , Unit::ElectricalInductance    );
const Quantity Quantity::MilliHenry             ( 1e+3                  , Unit::ElectricalInductance    );
const Quantity Quantity::Henry                  ( 1e+6                  , Unit::ElectricalInductance    );

const Quantity Quantity::Joule                  ( 1e+6                  , Unit::Work                    );
const Quantity Quantity::MilliJoule             ( 1e+3                  , Unit::Work                    );
const Quantity Quantity::KiloJoule              ( 1e+9                  , Unit::Work                    );
const Quantity Quantity::VoltAmpereSecond       ( 1e+6                  , Unit::Work                    );
const Quantity Quantity::WattSecond             ( 1e+6                  , Unit::Work                    );
const Quantity Quantity::KiloWattHour           ( 3.6e+12               , Unit::Work                    );
const Quantity Quantity::ElectronVolt           ( 1.602176634e-13       , Unit::Work                    );
const Quantity Quantity::KiloElectronVolt       ( 1.602176634e-10       , Unit::Work                    );
const Quantity Quantity::MegaElectronVolt       ( 1.602176634e-7        , Unit::Work                    );
const Quantity Quantity::Calorie                ( 4.1868e+6             , Unit::Work                    );
const Quantity Quantity::KiloCalorie            ( 4.1868e+9             , Unit::Work                    );
const Quantity Quantity::NewtonMeter            ( 1e+6                  , Unit::Moment                  );

const Quantity Quantity::KMH                    ( 1e+6 / 3600           , Unit::Velocity                );
const Quantity Quantity::MPH                    ( mile / 3600           , Unit::Velocity                );

const Quantity Quantity::AngMinute              ( 1.0 / 60.0            , Unit::Angle                   );
const Quantity Quantity::AngSecond              ( 1.0 / 3600.0          , Unit::Angle                   );
const Quantity Quantity::Degree                 ( 1.0                   , Unit::Angle                   );
const Quantity Quantity::Radian                 ( 180 / std::numbers::pi, Unit::Angle                   );
const Quantity Quantity::Gon                    ( 360.0 / 400.0         , Unit::Angle                   );
// clang-format on
}  // namespace ExpressionEngine::Units
