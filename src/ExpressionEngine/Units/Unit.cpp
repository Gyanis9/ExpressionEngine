#include <ExpressionEngine/Units/Unit.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <ranges>

namespace ExpressionEngine::Units
{
    namespace
    {
        /// 类型名与量纲指数的对照表，既供反查类型名，也供按名构造静态单位
        struct UnitSpecification
        {
            std::string_view name;
            UnitExponents    exponents;
        };

        /// 类型名与量纲指数对照表的全部条目，顺序与 UnitSpecification 的声明一致
        constexpr auto unitSpecifications = std::to_array<UnitSpecification>({
                {.name = "1", .exponents = {0, 0, 0, 0, 0, 0, 0, 0}},
                {.name = "Length", .exponents = {1}},
                {.name = "Mass", .exponents = {0, 1}},
                {.name = "TimeSpan", .exponents = {0, 0, 1}},
                {.name = "ElectricCurrent", .exponents = {0, 0, 0, 1}},
                {.name = "Temperature", .exponents = {0, 0, 0, 0, 1}},
                {.name = "AmountOfSubstance", .exponents = {0, 0, 0, 0, 0, 1}},
                {.name = "LuminousIntensity", .exponents = {0, 0, 0, 0, 0, 0, 1}},
                {.name = "Angle", .exponents = {0, 0, 0, 0, 0, 0, 0, 1}},
                {.name = "Acceleration", .exponents = {1, 0, -2}},
                {.name = "AngleOfFriction", .exponents = {0, 0, 0, 0, 0, 0, 0, 1}},
                {.name = "Area", .exponents = {2}},
                {.name = "CurrentDensity", .exponents = {-2, 0, 0, 1}},
                {.name = "Density", .exponents = {-3, 1}},
                {.name = "DissipationRate", .exponents = {2, 0, -3}},
                {.name = "DynamicViscosity", .exponents = {-1, 1, -1}},
                {.name = "ElectricalCapacitance", .exponents = {-2, -1, 4, 2}},
                {.name = "ElectricalConductance", .exponents = {-2, -1, 3, 2}},
                {.name = "ElectricalConductivity", .exponents = {-3, -1, 3, 2}},
                {.name = "ElectricalInductance", .exponents = {2, 1, -2, -2}},
                {.name = "ElectricalResistance", .exponents = {2, 1, -3, -2}},
                {.name = "ElectricCharge", .exponents = {0, 0, 1, 1}},
                {.name = "ElectricPotential", .exponents = {2, 1, -3, -1}},
                {.name = "ElectromagneticPotential", .exponents = {1, 1, -2, -1}},
                {.name = "Force", .exponents = {1, 1, -2}},
                {.name = "Frequency", .exponents = {0, 0, -1}},
                {.name = "HeatFlux", .exponents = {0, 1, -3}},
                {.name = "Inertia", .exponents = {2, 1}},
                {.name = "InverseArea", .exponents = {-2}},
                {.name = "InverseLength", .exponents = {-1}},
                {.name = "InverseVolume", .exponents = {-3}},
                {.name = "KinematicViscosity", .exponents = {2, 0, -1}},
                {.name = "MagneticFieldStrength", .exponents = {-1, 0, 0, 1}},
                {.name = "MagneticFlux", .exponents = {2, 1, -2, -1}},
                {.name = "MagneticFluxDensity", .exponents = {0, 1, -2, -1}},
                {.name = "Magnetization", .exponents = {-1, 0, 0, 1}},
                {.name = "Moment", .exponents = {2, 1, -2}},
                {.name = "Pressure", .exponents = {-1, 1, -2}},
                {.name = "Power", .exponents = {2, 1, -3}},
                {.name = "ShearModulus", .exponents = {-1, 1, -2}},
                {.name = "SpecificEnergy", .exponents = {2, 0, -2}},
                {.name = "SpecificHeat", .exponents = {2, 0, -2, 0, -1}},
                {.name = "Stiffness", .exponents = {0, 1, -2}},
                {.name = "StiffnessDensity", .exponents = {-2, 1, -2}},
                {.name = "Stress", .exponents = {-1, 1, -2}},
                {.name = "SurfaceChargeDensity", .exponents = {-2, 0, 1, 1}},
                {.name = "ThermalConductivity", .exponents = {1, 1, -3, 0, -1}},
                {.name = "ThermalExpansionCoefficient", .exponents = {0, 0, 0, 0, -1}},
                {.name = "ThermalTransferCoefficient", .exponents = {0, 1, -3, 0, -1}},
                {.name = "UltimateTensileStrength", .exponents = {-1, 1, -2}},
                {.name = "VacuumPermittivity", .exponents = {-3, -1, 4, 2}},
                {.name = "Velocity", .exponents = {1, 0, -1}},
                {.name = "Volume", .exponents = {3}},
                {.name = "Concentration", .exponents = {-3, 0, 0, 0, 0, 1}},
                {.name = "VolumeChargeDensity", .exponents = {-3, 0, 1, 1}},
                {.name = "VolumeFlowRate", .exponents = {3, 0, -1}},
                {.name = "VolumetricThermalExpansionCoefficient", .exponents = {0, 0, 0, 0, -1}},
                {.name = "Work", .exponents = {2, 1, -2}},
                {.name = "YieldStrength", .exponents = {-1, 1, -2}},
                {.name = "YoungsModulus", .exponents = {-1, 1, -2}},
        }); // clang-format on

        /// 按类型名构造静态单位；名字必须存在于对照表中
        constexpr Unit makeUnit(const std::string_view name)
        {
            if (const auto specification = std::ranges::find(unitSpecifications, name, &UnitSpecification::name); specification != unitSpecifications.end())
            {
                return Unit{specification->exponents, specification->name};
            }
            throw Base::NameError("单位类型名不在对照表中，可用 getTypeString() 取当前单位支持的名称");
        }
    } // namespace

    Unit::Unit(const int length, const int mass, const int time, const int electricCurrent, const int thermodynamicTemperature, const int amountOfSubstance, const int luminousIntensity, const int angle)
    {
        // 先夹到 int8 可表示范围再写入：越界值由 checkRange() 统一报错，不在这里静默回绕
        const auto clampToExponent = [](const int value)
        {
            return static_cast<std::int8_t>(
                std::clamp(value, static_cast<int>(std::numeric_limits<std::int8_t>::min()), static_cast<int>(std::numeric_limits<std::int8_t>::max())));
        };

        m_exponents[0] = clampToExponent(length);
        m_exponents[1] = clampToExponent(mass);
        m_exponents[2] = clampToExponent(time);
        m_exponents[3] = clampToExponent(electricCurrent);
        m_exponents[4] = clampToExponent(thermodynamicTemperature);
        m_exponents[5] = clampToExponent(amountOfSubstance);
        m_exponents[6] = clampToExponent(luminousIntensity);
        m_exponents[7] = clampToExponent(angle);

        checkRange();
    }

    bool Unit::operator==(const Unit &that) const
    {
        return m_exponents == that.m_exponents;
    }

    bool Unit::operator!=(const Unit &that) const
    {
        return m_exponents != that.m_exponents;
    }

    Unit &Unit::operator*=(const Unit &that)
    {
        *this = *this * that;
        return *this;
    }

    Unit &Unit::operator/=(const Unit &that)
    {
        *this = *this / that;
        return *this;
    }

    Unit Unit::operator*(const Unit &that) const
    {
        UnitExponents result{};
        std::transform(m_exponents.begin(), m_exponents.end(), that.m_exponents.begin(), result.begin(),
                       [](const auto leftExponent, const auto rightExponent)
                       {
                           return static_cast<std::int8_t>(leftExponent + rightExponent);
                       });

        // 结果构造时校验指数范围，乘法溢出一律报错而非截断
        return Unit{result};
    }

    Unit Unit::operator/(const Unit &that) const
    {
        UnitExponents result{};
        std::transform(m_exponents.begin(), m_exponents.end(), that.m_exponents.begin(), result.begin(),
                       [](const auto leftExponent, const auto rightExponent)
                       {
                           return static_cast<std::int8_t>(leftExponent - rightExponent);
                       });

        return Unit{result};
    }

    Unit Unit::root(const uint8_t rootDegree) const
    {
        if (rootDegree < 1)
        {
            throw Base::UnitsMismatchError("开方次数必须大于 0，请传入 2 表示平方根、3 表示立方根");
        }

        UnitExponents result{};
        std::ranges::transform(m_exponents, result.begin(),
                               [rootDegree](const auto exponent)
                               {
                                   // 指数必须能被开方次数整除，否则会得到分数次幂，单位无法表示
                                   if (exponent % rootDegree != 0)
                                   {
                                       throw Base::UnitsMismatchError(std::format("单位指数 {} 不能被开方次数 {} 整除，请改用 pow() 或先换算量纲", exponent, rootDegree));
                                   }
                                   return static_cast<std::int8_t>(exponent / rootDegree);
                               });

        return Unit{result};
    }

    Unit Unit::pow(const double exponent) const
    {
        UnitExponents result{};
        std::ranges::transform(m_exponents, result.begin(),
                               [exponent](const auto exponentValue)
                               {
                                   const auto scaled{exponentValue * exponent};
                                   // 允许浮点误差，但结果必须落在整数格点上，否则单位无法表示
                                   if (std::fabs(std::round(scaled) - scaled) >= std::numeric_limits<double>::epsilon())
                                   {
                                       throw Base::UnitsMismatchError(std::format("单位指数 {} 乘以幂次 {} 不是整数，请改用可整除的幂次", exponentValue, exponent));
                                   }
                                   return static_cast<std::int8_t>(std::lround(scaled));
                               });

        return Unit{result};
    }

    UnitExponents Unit::exponents() const
    {
        return m_exponents;
    }

    int Unit::length() const
    {
        return m_exponents[0];
    }

    std::string Unit::getString() const
    {
        // 单个量纲的写法：指数为 1 时省略 ^1，指数取绝对值，正负在分子分母分派
        const auto buildComponent = [this](const std::size_t index)
        {
            const std::string symbol{unitSymbols.at(index)};
            const auto        absoluteExponent{std::abs(static_cast<int>(m_exponents.at(index)))};

            return absoluteExponent <= 1 ? symbol : std::format("{}^{}", symbol, absoluteExponent);
        };

        const auto buildProduct = [&buildComponent](const std::vector<std::size_t> &indexes)
        {
            std::string product;
            for (const std::size_t index: indexes)
            {
                if (!product.empty())
                {
                    product += '*';
                }
                product += buildComponent(index);
            }
            return product;
        };

        const auto [positiveIndexes, negativeIndexes] = nonZeroValueIndexes();
        const auto numerator                          = buildProduct(positiveIndexes);
        if (negativeIndexes.empty())
        {
            return numerator;
        }

        const auto denominator = buildProduct(negativeIndexes);

        // 分母有多个量纲时加括号，避免 "kg/mm*s^2" 这类歧义写法
        return std::format("{}/{}", numerator.empty() ? "1" : numerator, negativeIndexes.size() > 1 ? std::format("({})", denominator) : denominator);
    }

    std::string Unit::representation() const
    {
        std::string exponentList;
        for (const auto exponent: m_exponents)
        {
            if (!exponentList.empty())
            {
                exponentList += ',';
            }
            exponentList += std::format("{}", exponent);
        }

        const auto withExponents = std::format("Unit: {} ({})", getString(), exponentList);
        const auto typeName      = getTypeString();

        return typeName.empty() ? withExponents : std::format("{} [{}]", withExponents, typeName);
    }

    std::string Unit::getTypeString() const
    {
        // 显式给了类型名就直接用，否则按指数反查对照表
        if (!m_name.empty())
        {
            return std::string{m_name};
        }

        const auto specification = std::ranges::find(unitSpecifications, m_exponents, &UnitSpecification::exponents);
        return std::string(specification == unitSpecifications.end() ? std::string_view{} : specification->name);
    }

    std::pair<std::vector<std::size_t>, std::vector<std::size_t> > Unit::nonZeroValueIndexes() const
    {
        std::vector<std::size_t> positiveIndexes;
        std::vector<std::size_t> negativeIndexes;

        for (std::size_t index = 0; index < m_exponents.size(); ++index)
        {
            const auto exponent = m_exponents.at(index);
            if (exponent > 0)
            {
                positiveIndexes.push_back(index);
            } else if (exponent < 0)
            {
                negativeIndexes.push_back(index);
            }
        }

        return {positiveIndexes, negativeIndexes};
    }

    constexpr Unit Unit::One = makeUnit("1");

    constexpr Unit Unit::Length                                = makeUnit("Length");
    constexpr Unit Unit::Mass                                  = makeUnit("Mass");
    constexpr Unit Unit::TimeSpan                              = makeUnit("TimeSpan");
    constexpr Unit Unit::ElectricCurrent                       = makeUnit("ElectricCurrent");
    constexpr Unit Unit::Temperature                           = makeUnit("Temperature");
    constexpr Unit Unit::AmountOfSubstance                     = makeUnit("AmountOfSubstance");
    constexpr Unit Unit::LuminousIntensity                     = makeUnit("LuminousIntensity");
    constexpr Unit Unit::Angle                                 = makeUnit("Angle");
    constexpr Unit Unit::Acceleration                          = makeUnit("Acceleration");
    constexpr Unit Unit::AngleOfFriction                       = makeUnit("Angle");
    constexpr Unit Unit::Area                                  = makeUnit("Area");
    constexpr Unit Unit::Concentration                         = makeUnit("Concentration");
    constexpr Unit Unit::CompressiveStrength                   = makeUnit("Pressure");
    constexpr Unit Unit::CurrentDensity                        = makeUnit("CurrentDensity");
    constexpr Unit Unit::Density                               = makeUnit("Density");
    constexpr Unit Unit::DissipationRate                       = makeUnit("DissipationRate");
    constexpr Unit Unit::DynamicViscosity                      = makeUnit("DynamicViscosity");
    constexpr Unit Unit::ElectricalCapacitance                 = makeUnit("ElectricalCapacitance");
    constexpr Unit Unit::ElectricalConductance                 = makeUnit("ElectricalConductance");
    constexpr Unit Unit::ElectricalConductivity                = makeUnit("ElectricalConductivity");
    constexpr Unit Unit::ElectricalInductance                  = makeUnit("ElectricalInductance");
    constexpr Unit Unit::ElectricalResistance                  = makeUnit("ElectricalResistance");
    constexpr Unit Unit::ElectricCharge                        = makeUnit("ElectricCharge");
    constexpr Unit Unit::ElectricPotential                     = makeUnit("ElectricPotential");
    constexpr Unit Unit::ElectromagneticPotential              = makeUnit("ElectromagneticPotential");
    constexpr Unit Unit::Force                                 = makeUnit("Force");
    constexpr Unit Unit::Frequency                             = makeUnit("Frequency");
    constexpr Unit Unit::HeatFlux                              = makeUnit("HeatFlux");
    constexpr Unit Unit::Inertia                               = makeUnit("Inertia");
    constexpr Unit Unit::InverseArea                           = makeUnit("InverseArea");
    constexpr Unit Unit::InverseLength                         = makeUnit("InverseLength");
    constexpr Unit Unit::InverseVolume                         = makeUnit("InverseVolume");
    constexpr Unit Unit::KinematicViscosity                    = makeUnit("KinematicViscosity");
    constexpr Unit Unit::MagneticFieldStrength                 = makeUnit("MagneticFieldStrength");
    constexpr Unit Unit::MagneticFlux                          = makeUnit("MagneticFlux");
    constexpr Unit Unit::MagneticFluxDensity                   = makeUnit("MagneticFluxDensity");
    constexpr Unit Unit::Magnetization                         = makeUnit("Magnetization");
    constexpr Unit Unit::Moment                                = makeUnit("Moment");
    constexpr Unit Unit::Pressure                              = makeUnit("Pressure");
    constexpr Unit Unit::Power                                 = makeUnit("Power");
    constexpr Unit Unit::ShearModulus                          = makeUnit("Pressure");
    constexpr Unit Unit::SpecificEnergy                        = makeUnit("SpecificEnergy");
    constexpr Unit Unit::SpecificHeat                          = makeUnit("SpecificHeat");
    constexpr Unit Unit::Stiffness                             = makeUnit("Stiffness");
    constexpr Unit Unit::StiffnessDensity                      = makeUnit("StiffnessDensity");
    constexpr Unit Unit::Stress                                = makeUnit("Pressure");
    constexpr Unit Unit::SurfaceChargeDensity                  = makeUnit("SurfaceChargeDensity");
    constexpr Unit Unit::ThermalConductivity                   = makeUnit("ThermalConductivity");
    constexpr Unit Unit::ThermalExpansionCoefficient           = makeUnit("ThermalExpansionCoefficient");
    constexpr Unit Unit::ThermalTransferCoefficient            = makeUnit("ThermalTransferCoefficient");
    constexpr Unit Unit::UltimateTensileStrength               = makeUnit("Pressure");
    constexpr Unit Unit::VacuumPermittivity                    = makeUnit("VacuumPermittivity");
    constexpr Unit Unit::Velocity                              = makeUnit("Velocity");
    constexpr Unit Unit::Volume                                = makeUnit("Volume");
    constexpr Unit Unit::VolumeChargeDensity                   = makeUnit("VolumeChargeDensity");
    constexpr Unit Unit::VolumeFlowRate                        = makeUnit("VolumeFlowRate");
    constexpr Unit Unit::VolumetricThermalExpansionCoefficient = makeUnit("ThermalExpansionCoefficient");
    constexpr Unit Unit::Work                                  = makeUnit("Work");
    constexpr Unit Unit::YieldStrength                         = makeUnit("Pressure");
    constexpr Unit Unit::YoungsModulus                         = makeUnit("Pressure");
} // namespace ExpressionEngine::Units
