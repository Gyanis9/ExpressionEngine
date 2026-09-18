/**
 * @file UnitsSchemasData.h
 * @brief 内置单位方案的原始数据表与特殊换算函数
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <ExpressionEngine/Units/UnitsConvData.h>
#include <ExpressionEngine/Units/UnitsSchemasSpecs.h>

#ifndef QT_TRANSLATE_NOOP
#define QT_TRANSLATE_NOOP(context, sourceText) sourceText
#endif

/**
 * UnitSchemas raw data
 */

namespace ExpressionEngine::Units::UnitsSchemasData
{
    /// 内置方案数据包的默认小数位数
    constexpr std::size_t defaultDecimals{2};

    /// 内置方案数据包的默认分数分母
    constexpr std::size_t defaultDenominator{8};

    using namespace UnitsConvData;

// NOLINTBEGIN
// clang-format off
/// 编号 6 方案 MmMin：毫米与毫米每分钟，CNC 小件与公制小尺寸用
inline const UnitsSchemaSpec s0
{ 6, "MmMin", "mm" , false, false , QT_TRANSLATE_NOOP("UnitsApi", "Metric small parts & CNC (mm, mm/min)"), false,
    {
        { "Length",   {{ 0 , "mm"     , 1.0        }}},
        { "Angle",    {{ 0 , "°"      , 1.0        }}},
        { "Velocity", {{ 0 , "mm/min" , 1.0 / 60.0 }}}
    }
};

/// 编号 9 方案 MeterDecimal：米制小数（m、m²、m³）
inline const UnitsSchemaSpec s1
{ 9, "MeterDecimal", "m", false, false, QT_TRANSLATE_NOOP("UnitsApi", "Meter decimal (m, m², m³)"), false,
    {
        { "Length",             {{ 0 , "m"    , 1e3 }}},
        { "Area",               {{ 0 , "m^2"  , 1e6 }}},
        { "Volume",             {{ 0 , "m^3"  , 1e9 }}},
        { "Inertia",            {{ 0 , "kg*m^2", 1e6 }}},
        { "Power",              {{ 0 , "W"    , 1e6 }}},
        { "ElectricPotential",  {{ 0 , "V"    , 1e6 }}},
        { "HeatFlux",           {{ 0 , "W/m^2", 1.0 }}},
        { "Velocity",           {{ 0 , "m/s"  , 1e3 }}}
    }
};

/// 编号 3 方案 ImperialDecimal：英制小数（in、lb）
inline const UnitsSchemaSpec s2
{ 3, "ImperialDecimal", "in", false, false, QT_TRANSLATE_NOOP("UnitsApi", "Imperial decimal (in, lb)"), false,
    {
        { "Length",       {{ 0 , "in"      , inch                }}},
        { "Angle",        {{ 0 , "°"       , 1.0               }}},
        { "Area",         {{ 0 , "in^2"    , inch * inch           }}},
        { "Volume",       {{ 0 , "in^3"    , inch * inch * inch      }}},
        { "Mass",         {{ 0 , "lb"      , pound                }}},
        { "Inertia",      {{ 0 , "lb*in^2", pound * inch * inch       }}},
        { "Pressure",     {{ 0 , "psi"     , psi               }}},
        { "Stiffness",    {{ 0 , "lbf/in"  , poundForce / inch * 1000   }}},
        { "Velocity",     {{ 0 , "in/min"  , inch / 60           }}},
        { "Acceleration", {{ 0 , "in/min^2", inch / 3600         }}}
    }
};

/// 编号 0 方案 Internal：默认方案，mm、kg、s、°
inline const UnitsSchemaSpec s3
{ 0, "Internal", "mm", false, false, QT_TRANSLATE_NOOP("UnitsApi", "Standard (mm, kg, s, °)"), true,
    {
        { "Length", {
            { 1e-6            , "mm"         , 1.0             },
            { 1e-3            , "nm"         , 1e-6            },
            { 1e-1            , "\xC2\xB5m"  , 1e-3            },
            { 1e4             , "mm"         , 1.0             },
            { 1e7             , "m"          , 1e3             },
            { 1e10            , "km"         , 1e6             },
            { 0               , "m"          , 1e3             }}
        },
        { "Area", {
            { 1e2             , "mm^2"       , 1.0             },
            { 1e6             , "cm^2"       , 1e2             },
            { 1e12            , "m^2"        , 1e6             },
            { 0               , "km^2"       , 1e12            }}
        },
        { "Volume", {
            { 1e3             , "mm^3"       , 1.0             },
            { 1e6             , "ml"         , 1e3             },
            { 1e9             , "l"          , 1e6             },
            { 0               , "m^3"        , 1e9             }}
        },
        { "Angle", {
            { 0               , "°"          , 1.0             }}
        },
        { "ElectricCurrent", {
            { 1e-6            , "nA"         , 1e-9            },
            { 1e-3            , "\xC2\xB5""A", 1e-6            },
            { 1               , "mA"         , 1e-3            },
            { 0               , "A"          , 1.0             }}
        },
        { "Mass", {
            { 1e-6            , "\xC2\xB5g"  , 1e-9            },
            { 1e-3            , "mg"         , 1e-6            },
            { 1.0             , "g"          , 1e-3            },
            { 1e3             , "kg"         , 1.0             },
            { 0               , "t"          , 1e3             }}
        },
        { "Inertia", {
            { 0               , "kg*mm^2"    , 1.0             }}
        },
        { "Density", {
            { 1e-4            , "kg/m^3"     , 1e-9            },
            { 1.0             , "kg/cm^3"    , 1e-3            },
            { 0               , "kg/mm^3"    , 1.0             }}
        },
        { "Concentration", {
            { 1e-9            , "\xC2\xB5mol/l", 1e-12         },
            { 1e-6            , "mmol/l"     , 1e-9            },
            { 0               , "mol/l"      , 1e-6            }}
        },
        { "AmountOfSubstance", {
            { 1e-6            , "nmol"       , 1e-9            },
            { 1e-3            , "\xC2\xB5mol", 1e-6            },
            { 1               , "mmol"       , 1e-3            },
            { 0               , "mol"        , 1.0             }}
        },
        { "ThermalConductivity", {
            { 1e6             , "W/m/K"      , 1e3             },
            { 0               , "W/mm/K"     , 1e6             }}
        },
        { "ThermalExpansionCoefficient", {
            { 1e-3            , "\xC2\xB5m/m/K" , 1e-6         },
            { 0               , "mm/mm/K"    , 1.0             }}
        },
        { "VolumetricThermalExpansionCoefficient", {
            { 1e-3            , "mm^3/m^3/K" , 1e-9            },
            { 0               , "m^3/m^3/K"  , 1.0             }}
        },
        { "SpecificHeat", {
            { 0               , "J/kg/K"     , 1e6             }}
        },
        { "ThermalTransferCoefficient", {
            { 0               , "W/m^2/K"    , 1.0             }}
        },
        { "Pressure", {
            { 10.0            , "Pa"         , 1e-3            },
            { 1e4             , "kPa"        , 1.0             },
            { 1e7             , "MPa"        , 1e3             },
            { 1e10            , "GPa"        , 1e6             },
            { 0               , "Pa"         , 1e-3            }}
        },
        { "Stress", {
            { 10.0            , "Pa"         , 1e-3            },
            { 1e4             , "kPa"        , 1.0             },
            { 1e7             , "MPa"        , 1e3             },
            { 1e10            , "GPa"        , 1e6             },
            { 0               , "Pa"         , 1e-3            }}
        },
        { "Stiffness", {
            { 1               , "mN/m"       , 1e-3            },
            { 1e3             , "N/m"        , 1.0             },
            { 1e6             , "kN/m"       , 1e3             },
            { 0               , "MN/m"       , 1e6             }}
        },
        { "StiffnessDensity", {
            { 1e-3            , "Pa/m"       , 1e-6            },
            { 1               , "kPa/m"      , 1e-3            },
            { 1e3             , "MPa/m"      , 1.0             },
            { 0               , "GPa/m"      , 1e3             }}
        },
        { "Force", {
            { 1e3             , "mN"         , 1.0             },
            { 1e6             , "N"          , 1e3             },
            { 1e9             , "kN"         , 1e6             },
            { 0               , "MN"         , 1e9             }}
        },
        { "Power", {
            { 1               , "nW"         , 1e-3            },
            { 1e3             , "\xC2\xB5W"  , 1               },
            { 1e6             , "mW"         , 1e3             },
            { 1e9             , "W"          , 1e6             },
            { 0               , "kW"         , 1e9             }}
        },
        { "ElectricPotential", {
            { 1e6             , "mV"         , 1e3             },
            { 1e9             , "V"          , 1e6             },
            { 1e12            , "kV"         , 1e9             },
            { 0               , "V"          , 1e6             }}
        },
        { "Work", {
            { 1.602176634e-10 , "eV"         , 1.602176634e-13 },
            { 1.602176634e-7  , "keV"        , 1.602176634e-10 },
            { 1.602176634e-4  , "MeV"        , 1.602176634e-7  },
            { 1e6             , "mJ"         , 1e3             },
            { 1e9             , "J"          , 1e6             },
            { 1e12            , "kJ"         , 1e9             },
            { 3.6e+15         , "kWh"        , 3.6e+12         },
            { 0               , "J"          , 1e6             }}
        },
        { "Moment", {
            { 0               , "Nm"         , 1e6             }}
        },
        { "SpecificEnergy", {
            { 0               , "m^2/s^2"    , 1e6             }}
        },
        { "HeatFlux", {
            { 0               , "W/m^2"      , 1.0             }}
        },
        { "ElectricCharge", {
            { 0               , "C"          , 1.0             }}
        },
        { "SurfaceChargeDensity", {
            { 1e-2            , "C/m^2"      , 1e-6            },
            { 1.0             , "C/cm^2"     , 1e-2            },
            { 0               , "C/mm^2"     , 1.0             }}
        },
        { "VolumeChargeDensity", {
            { 1e-3            , "C/m^3"      , 1e-9            },
            { 1.0             , "C/cm^3"     , 1e-3            },
            { 0               , "C/mm^3"     , 1.0             }}
        },
        { "CurrentDensity", {
            { 1e-2            , "A/m^2"      , 1e-6            },
            { 1.0             , "A/cm^2"     , 1e-2            },
            { 0               , "A/mm^2"     , 1               }}
        },
        { "MagneticFluxDensity", {
            { 1.0             , "mT"         , 1e-3            },
            { 0               , "T"          , 1.0             }}
        },
        { "MagneticFieldStrength", {
            { 0               , "A/m"        , 1e-3            }}
        },
        { "MagneticFlux", {
            { 0               , "Wb"         , 1e6             }}
        },
        { "Magnetization", {
            { 0               , "A/m"        , 1e-3            }}
        },
        { "ElectromagneticPotential", {
            { 0               , "Wb/m"        , 1e3            }}
        },
        { "ElectricalConductance", {
            { 1e-9            , "\xC2\xB5S"  , 1e-12           },
            { 1e-6            , "mS"         , 1e-9            },
            { 0               , "S"          , 1e-6            }}
        },
        { "ElectricalResistance", {
            { 1e9             , "Ohm"        , 1e6             },
            { 1e12            , "kOhm"       , 1e9             },
            { 0               , "MOhm"       , 1e12            }}
        },
        { "ElectricalConductivity", {
            { 1e-9            , "mS/m"       , 1e-12           },
            { 1e-6            , "S/m"        , 1e-9            },
            { 1e-3            , "kS/m"       , 1e-6            },
            { 0               , "MS/m"       , 1e-3            }}
        },
        { "ElectricalCapacitance", {
            { 1e-15           , "pF"         , 1e-18           },
            { 1e-12           , "nF"         , 1e-15           },
            { 1e-9            , "\xC2\xB5""F", 1e-12           },
            { 1e-6            , "mF"         , 1e-9            },
            { 0               , "F"          , 1e-6            }}
        },
        { "ElectricalInductance", {
            { 1.0             , "nH"         , 1e-3            },
            { 1e3             , "\xC2\xB5H"  , 1.0             },
            { 1e6             , "mH"         , 1e3             },
            { 0               , "H"          , 1e6             }}
        },
        { "VacuumPermittivity", {
            { 0               , "F/m"        , 1e-9            }}
        },
        { "Frequency", {
            { 1e3             , "Hz"         , 1.0             },
            { 1e6             , "kHz"        , 1e3             },
            { 1e9             , "MHz"        , 1e6             },
            { 1e12            , "GHz"        , 1e9             },
            { 0               , "THz"        , 1e12            }}
        },
        { "Velocity", {
            { 0               , "mm/s"       , 1.0             }}
        },
        { "DynamicViscosity", {
            { 0               , "Pa*s"       , 1e-3            }}
        },
        { "KinematicViscosity", {
            { 1e3             , "mm^2/s"     , 1.0             },
            { 0               , "m^2/s"      , 1e6             }}
        },
        { "VolumeFlowRate", {
            { 1e3             , "mm^3/s"     , 1.0             },
            { 1e6             , "ml/s"       , 1e3             },
            { 1e9             , "l/s"        , 1e6             },
            { 0               , "m^3/s"      , 1e9             }}
        },
        { "DissipationRate", {
            { 0               , "W/kg"       , 1e6             }}
        },
        { "InverseLength", {
            { 1e-6            , "1/m"        , 1e-3            },
            { 1e-3            , "1/km"       , 1e-6            },
            { 1.0             , "1/m"        , 1e-3            },
            { 1e3             , "1/mm"       , 1.0             },
            { 1e6             , "1/\xC2\xB5m", 1e3             },
            { 1e9             , "1/nm"       , 1e6             },
            { 0               , "1/m"        , 1e-3            }}
        },
        { "InverseArea", {
            { 1e-12           , "1/m^2"      , 1e-6            },
            { 1e-6            , "1/km^2"     , 1e-12           },
            { 1.0             , "1/m^2"      , 1e-6            },
            { 1e2             , "1/cm^2"     , 1e-2            },
            { 0               , "1/mm^2"     , 1.0             }}
        },
        { "InverseVolume", {
            { 1e-6            , "1/m^3"      , 1e-9            },
            { 1e-3            , "1/l"        , 1e-6            },
            { 1.0             , "1/ml"       , 1e-3            },
            { 0               , "1/mm^3"     , 1.0             }}
        }
    }
};

/// 编号 1 方案 MKS：m、kg、s、°
inline const UnitsSchemaSpec s4
{ 1, "MKS", "m", false, false, QT_TRANSLATE_NOOP("UnitsApi", "MKS (m, kg, s, °)") , false,
    {
        { "Length", {
            { 1e-6            , "mm"         , 1.0             },
            { 1e-3            , "nm"         , 1e-6            },
            { 0.1             , "\xC2\xB5m"  , 1e-3            },
            { 1e4             , "mm"         , 1.0             },
            { 1e7             , "m"          , 1e3             },
            { 1e10            , "km"         , 1e6             },
            { 0               , "m"          , 1e3             }}
        },
        { "Area", {
            { 100             , "mm^2"       , 1.0             },
            { 1e6             , "cm^2"       , 100             },
            { 1e12            , "m^2"        , 1e6             },
            { 0               , "km^2"       , 1e12            }}
        },
        { "Volume", {
            { 1e3             , "mm^3"       , 1.0             },
            { 1e6             , "ml"         , 1e3             },
            { 1e9             , "l"          , 1e6             },
            { 0               , "m^3"        , 1e9             }}
        },
        { "Mass", {
            { 1e-6            , "\xC2\xB5g"  , 1e-9            },
            { 1e-3            , "mg"         , 1e-6            },
            { 1.0             , "g"          , 1e-3            },
            { 1e3             , "kg"         , 1.0             },
            { 0               , "t"          , 1e3             }}
        },
        { "Inertia", {
            { 0               , "kg*m^2"     , 1e6             }}
        },
        { "Density", {
            { 0.0001          , "kg/m^3"     , 0.000000001     },
            { 1.0             , "kg/cm^3"    , 0.001           },
            { 0               , "kg/mm^3"    , 1.0             }}
        },
        { "Acceleration", {
            { 0               , "m/s^2"      , 1000.0          }}
        },
        { "Pressure", {
            { 10.0             , "Pa"        , 0.001           },
            { 10'000.0         , "kPa"       , 1.0             },
            { 10'000'000.0     , "MPa"       , 1'000.0         },
            { 10'000'000'000.0 , "GPa"       , 1'000'000.0     },
            { 0                , "Pa"        , 0.001           }}
        },
        { "Stress", {
            { 10.0             , "Pa"        , 0.001           },
            { 10'000.0         , "kPa"       , 1.0             },
            { 10'000'000.0     , "MPa"       , 1'000.0         },
            { 10'000'000'000.0 , "GPa"       , 1'000'000.0     },
            { 0                , "Pa"        , 0.001           }}
        },
        { "Stiffness", {
            { 1               , "mN/m"       , 1e-3            },
            { 1e3             , "N/m"        , 1.0             },
            { 1e6             , "kN/m"       , 1e3             },
            { 0               , "MN/m"       , 1e6             }}
        },
        { "StiffnessDensity", {
            { 1e-3            , "Pa/m"       , 1e-6            },
            { 1               , "kPa/m"      , 1e-3            },
            { 1e3             , "MPa/m"      , 1.0             },
            { 0               , "GPa/m"      , 1e3             }}
        },
        { "ThermalConductivity", {
            { 1'000'000       , "W/m/K"      , 1'000.0         },
            { 0               , "W/mm/K"     , 1'000'000.0     }}
        },
        { "ThermalExpansionCoefficient", {
            { 0.001           , "\xC2\xB5m/m/K" , 0.000001     },
            { 0               , "m/m/K"      , 1.0             }}
        },
        { "VolumetricThermalExpansionCoefficient", {
            { 0.001           , "mm^3/m^3/K" , 1e-9            },
            { 0               , "m^3/m^3/K"  , 1.0             }}
        },
        { "SpecificHeat", {
            { 0               , "J/kg/K"     , 1'000'000.0     }}
        },
        { "ThermalTransferCoefficient", {
            { 0               , "W/m^2/K"    , 1.0             }}
        },
        { "Force", {
            { 1e3             , "mN"         , 1.0             },
            { 1e6             , "N"          , 1e3             },
            { 1e9             , "kN"         , 1e6             },
            { 0               , "MN"         , 1e9             }}
        },
        { "Power", {
            { 1e6             , "mW"         , 1e3             },
            { 1e9             , "W"          , 1e6             },
            { 0               , "kW"         , 1e9             }}
        },
        { "ElectricPotential", {
            { 1e6             , "mV"         , 1e3             },
            { 1e9             , "V"          , 1e6             },
            { 1e12            , "kV"         , 1e9             },
            { 0               , "V"          , 1e6             }}
        },
        { "ElectricCharge", {
            { 0               , "C"          , 1.0             }}
        },
        { "SurfaceChargeDensity", {
            { 0               , "C/m^2"      , 1e-6            }}
        },
        { "VolumeChargeDensity", {
            { 0               , "C/m^3"      , 1e-9            }}
        },
        { "CurrentDensity", {
            { 1.0             , "A/m^2"      , 1e-6            },
            { 0               , "A/mm^2"     , 1.0             }}
        },
        { "MagneticFluxDensity", {
            { 1.0             , "mT"         , 1e-3            },
            { 0               , "T"          , 1.0             }}
        },
        { "MagneticFieldStrength", {
            { 0               , "A/m"        , 1e-3            }}
        },
        { "MagneticFlux", {
            { 0               , "Wb"         , 1e6             }}
        },
        { "Magnetization", {
            { 0               , "A/m"        , 1e-3            }}
        },
        { "ElectromagneticPotential", {
            { 0               , "Wb/m"        , 1e3            }}
        },
        { "ElectricalConductance", {
            { 1e-9            , "\xC2\xB5S"  , 1e-12           },
            { 1e-6            , "mS"         , 1e-9            },
            { 0               , "S"          , 1e-6            }}
        },
        { "ElectricalResistance", {
            { 1e9             , "Ohm"        , 1e6             },
            { 1e12            , "kOhm"       , 1e9             },
            { 0               , "MOhm"       , 1e12            }}
        },
        { "ElectricalConductivity", {
            { 1e-9            , "mS/m"       , 1e-12           },
            { 1e-6            , "S/m"        , 1e-9            },
            { 1e-3            , "kS/m"       , 1e-6            },
            { 0               , "MS/m"       , 1e-3            }}
        },
        { "ElectricalCapacitance", {
            { 1e-15           , "pF"         , 1e-18           },
            { 1e-12           , "nF"         , 1e-15           },
            { 1e-9            , "\xC2\xB5""F", 1e-12           },
            { 1e-6            , "mF"         , 1e-9            },
            { 0               , "F"          , 1e-6            }}
        },
        { "ElectricalInductance", {
            { 1.0             , "nH"         , 1e-3            },
            { 1e3             , "\xC2\xB5H"  , 1.0             },
            { 1e6             , "mH"         , 1e3             },
            { 0               , "H"          , 1e6             }}
        },
        { "VacuumPermittivity", {
            { 0               , "F/m"        , 1e-9            }}
        },
        { "Work", {
            { 1.602176634e-10 , "eV"         , 1.602176634e-13 },
            { 1.602176634e-7  , "keV"        , 1.602176634e-10 },
            { 1.602176634e-4  , "MeV"        , 1.602176634e-7  },
            { 1e6             , "mJ"         , 1e3             },
            { 1e9             , "J"          , 1e6             },
            { 1e12            , "kJ"         , 1e9             },
            { 3.6e+15         , "kWh"        , 3.6e+12         },
            { 0               , "J"          , 1e6             }}
        },
        { "SpecificEnergy", {
            { 0               , "m^2/s^2"    , 1000000         }}
        },
        { "HeatFlux", {
            { 0               , "W/m^2"      , 1.0             }}
        },
        { "Frequency", {
            { 1e3             , "Hz"         , 1.0             },
            { 1e6             , "kHz"        , 1e3             },
            { 1e9             , "MHz"        , 1e6             },
            { 1e12            , "GHz"        , 1e9             },
            { 0               , "THz"        , 1e12            }}
        },
        { "Velocity", {
            { 0               , "m/s"        , 1000.0          }}
        },
        { "DynamicViscosity", {
            { 0               , "Pa*s"       , 0.001           }}
        },
        { "KinematicViscosity", {
            { 0               , "m^2/s"      , 1e6             }}
        },
        { "VolumeFlowRate", {
            { 1e-3            , "m^3/s"      , 1e9             },
            { 1e3             , "mm^3/s"     , 1.0             },
            { 1e6             , "ml/s"       , 1e3             },
            { 1e9             , "l/s"        , 1e6             },
            { 0               , "m^3/s"      , 1e9             }}
        },
        { "DissipationRate", {
            { 0               , "W/kg"       , 1e6             }}
        },
        { "InverseLength", {
            { 1e-6            , "1/m"        , 1e-3             },
            { 1e-3            , "1/km"       , 1e-6             },
            { 1.0             , "1/m"        , 1e-3             },
            { 1e3             , "1/mm"       , 1.0              },
            { 1e6             , "1/\xC2\xB5m", 1e3              },
            { 1e9             , "1/nm"       , 1e6              },
            { 0               , "1/m"        , 1e-3             }}
        },
        { "InverseArea", {
            { 1e-12           , "1/m^2"      , 1e-6             },
            { 1e-6            , "1/km^2"     , 1e-12            },
            { 1.0             , "1/m^2"      , 1e-6             },
            { 1e2             , "1/cm^2"     , 1e-2             },
            { 0               , "1/mm^2"     , 1.0              }}
        },
        { "InverseVolume", {
            { 1e-6            , "1/m^3"      , 1e-9             },
            { 1e-3            , "1/l"        , 1e-6             },
            { 1.0             , "1/ml"       , 1e-3             },
            { 0               , "1/mm^3"     , 1.0              }}
        }
    }
};

/// 编号 4 方案 Centimeter：建筑欧标（cm、m²、m³）
inline const UnitsSchemaSpec s5
{ 4, "Centimeter", "cm", false, false, QT_TRANSLATE_NOOP("UnitsApi", "Building Euro (cm, m², m³)") , false,
    {
        { "Length", {
            { 0              , "cm"          , 10.0             }}
        },
        { "Area", {
            { 0              , "m^2"         , 1e6              }}
        },
        { "Volume", {
            { 0              , "m^3"         , 1e9              }}
        },
        { "Inertia", {
            { 0              , "kg*cm^2"     , 100.0            }}
        },
        { "Power", {
            { 0              , "W"           , 1e6              }}
        },
        { "ElectricPotential", {
            { 0              , "V"           , 1e6              }}
        },
        { "HeatFlux", {
            { 0              , "W/m^2"       , 1.0              }}
        },
        { "Velocity", {
            { 0              , "mm/min"      , 1.0 / 60         }}
        }
    }
};

/// 编号 8 方案 FEM：mm、N、s
inline const UnitsSchemaSpec s6
{ 8, "FEM", "mm", false , false , QT_TRANSLATE_NOOP("UnitsApi", "FEM (mm, N, s)"), false,
    {
        { "Length", {
            { 0             , "mm"           , 1.0               }}
        },
        { "Mass",   {
            { 0             , "t"            , 1e3               }}
        }
    }
};

/// 编号 2 方案 Imperial：美制惯用（in、lb）
inline const UnitsSchemaSpec s7
{ 2, "Imperial", "in", false, false, QT_TRANSLATE_NOOP("UnitsApi", "US customary (in, lb)"), false,
    {
        { "Length", {
            { 0.00000254      , "in"       , inch                },
            { 2.54            , "thou"     , inch / 1000         },
            { 304.8           , "\""       , inch                },
            { 914.4           , "'"        , foot                },
            { 1'609'344.0     , "yd"       , yard                },
            { 1'609'344'000.0 , "mi"       , mile                },
            { 0               , "in"       , inch                }}
        },
        { "Angle", {
            { 0               , "°"        , 1.0               }}
        },
        { "Area", {
            { 0               , "in^2"     , inch * inch           }}
        },
        { "Density", {
            { 0               , "lb/in^3"  , pound / (inch * inch * inch)}}
        },
        { "Volume", {
            { 0               , "in^3"     , inch * inch * inch      }}
        },
        { "Mass", {
            { 0               , "lb"       , pound                }}
        },
        { "Inertia", {
            { 0               , "lb*in^2"  , pound * inch * inch       }}
        },
        { "Pressure", {
            { 1000 * psi      , "psi"      , psi               },
            { 1000000 * psi   , "ksi"      , 1000 * psi        },
            { 0               , "psi"      , psi               }}
        },
        { "Stiffness", {
            { 0               , "lbf/in"   , poundForce / inch * 1000   }}
        },
        { "Velocity", {
            { 0               , "in/min"   , inch / 60           }}
        }
    }
};

/// 编号 5 方案 ImperialBuilding：建筑美制（ft-in、sqft、cft）
inline const UnitsSchemaSpec s8
{ 5, "ImperialBuilding", "ft", true, false , QT_TRANSLATE_NOOP("UnitsApi", "Building US (ft-in, sqft, cft)"), false,
    {
        { "Length"   , {{ 0   , "toFractional"    , 0              }}},  // <== !
        { "Angle"    , {{ 0   , "°"               , 1.0            }}},
        { "Area"     , {{ 0   , "sqft"            , foot * foot        }}},
        { "Volume"   , {{ 0   , "cft"             , foot * foot * foot   }}},
        { "Density"  , {{ 0   , "lb/ft^3"         , pound / (foot * foot * foot) }}},
        { "Pressure" , {{ 0   , "psi"             , psi            }}},
        { "Velocity" , {{ 0   , "in/min"          , inch / 60        }}}
    }
};

/// 编号 7 方案 ImperialCivil：土木英制（ft、lb、mph）
inline const UnitsSchemaSpec s9
{ 7, "ImperialCivil", "ft", false, true, QT_TRANSLATE_NOOP("UnitsApi", "Imperial for Civil Eng (ft, lb, mph)"), false,
    {
        { "Length"   , {{ 0   , "ft"    , foot                       }}},
        { "Area"     , {{ 0   , "ft^2"  , foot * foot                  }}},
        { "Volume"   , {{ 0   , "ft^3"  , foot * foot * foot             }}},
        { "Mass"     , {{ 0   , "lb"    , pound                       }}},
        { "Inertia"  , {{ 0   , "lb*ft^2", pound * foot * foot            }}},
        { "Density"  , {{ 0   , "lb/ft^3", pound / (foot * foot * foot)     }}},
        { "Pressure" , {{ 0   , "psi"   , psi                      }}},
        { "Stiffness", {{ 0   , "lbf/in", poundForce / inch * 1000          }}},
        { "Velocity" , {{ 0   , "mph"   , mile / 3600                }}},
        { "Angle"    , {{ 0   , "toDMS" , 0                        }}}  // <== !
    }
};

    // clang-format on
    // NOLINTEND
    /// 内置方案列表：按编号排列，供 UnitsSchemas 按序号或名字查找
    inline const std::vector schemaSpecs{s3, s4, s5, s6, s7, s8, s9, s0, s1, s2};

    /**
     * 特殊换算函数
     *
     * 方案的某个单位条目可以指定「换算因子为 0 + unitString 为函数名」，
     * 由下面的函数接管格式化；新增函数需同时登记进 specials 表。
     */

    /// 求最大公约数，欧几里得算法
    inline std::size_t greatestCommonDenominator(const std::size_t first, const std::size_t second)
    {
        return second == 0 ? first : greatestCommonDenominator(second, first % second);
    }

    /**
     * @brief 把毫米值写成「英尺' 英寸" 分数"」的形式
     * @param value 以毫米为单位的数值，可为负
     * @param denominator 分数的分母，如 8 表示精确到 1/8 英寸
     * @return 如 3' 4" + 3/8" 的文本
     */
    inline std::string toFractional(const double value, std::size_t denominator)
    {
        constexpr auto inchPerFoot{12};
        constexpr auto millimetrePerInch{25.4};

        // 先把毫米值换算成分数单位的整数个数，避免后续逐级取整时累积误差
        auto fractionalUnitCount = static_cast<std::size_t>(std::round(std::abs(value) / millimetrePerInch * static_cast<double>(denominator)));
        if (fractionalUnitCount == 0)
        {
            return "0";
        }

        const auto feet = static_cast<std::size_t>(std::floor(static_cast<double>(fractionalUnitCount) / (inchPerFoot * static_cast<double>(denominator))));
        fractionalUnitCount -= inchPerFoot * denominator * feet;

        const auto  inches    = static_cast<std::size_t>(std::floor(static_cast<double>(fractionalUnitCount) / static_cast<double>(denominator)));
        std::size_t numerator = fractionalUnitCount - (denominator * inches);

        // 分数要约到最简，否则 4/8" 这类写法会让显示结果不可读
        const std::size_t commonDenominator = greatestCommonDenominator(numerator, denominator);
        numerator /= commonDenominator;
        denominator /= commonDenominator;

        bool        addSpace{false};
        std::string result;

        if (value < 0)
        {
            result += "-";
        }

        if (feet > 0)
        {
            result += std::format("{}'", feet);
            addSpace = true;
        }

        if (inches > 0)
        {
            result += std::format("{}{}\"", addSpace ? " " : "", inches);
            addSpace = false;
        }

        if (numerator > 0)
        {
            // 英寸与分数之间补一个加减号，明确它是叠加在英寸上的余量
            if (inches > 0)
            {
                result += std::format(" {} ", value < 0 ? "-" : "+");
                addSpace = false;
            }
            result += std::format("{}{}/{}\"", addSpace ? " " : "", numerator, denominator);
        }

        return result;
    }

    /**
     * @brief 把十进制度数写成「度°分′秒″」的形式
     * @param value 十进制度数
     * @return 如 12°30′45″ 的文本，分秒为零时省略对应部分
     */
    inline std::string toDms(const double value)
    {
        constexpr auto degreeMinuteSecondRatio{60.0};

        // 逐级取整：度取整后余量乘 60 得分，分取整后余量乘 60 得秒
        const auto splitWholeAndRemainder = [](const double total) -> std::pair<int, double>
        {
            const double whole = std::floor(total);
            return {static_cast<int>(whole), degreeMinuteSecondRatio * (total - whole)};
        };

        const auto [degrees, totalMinutes] = splitWholeAndRemainder(value);
        std::string out                    = std::format("{}°", degrees);

        if (totalMinutes > 0)
        {
            const auto [minutes, totalSeconds] = splitWholeAndRemainder(totalMinutes);
            out += std::format("{}′", minutes);

            if (totalSeconds > 0)
            {
                out += std::format("{}″", std::lround(totalSeconds));
            }
        }

        return out;
    }

/// 特殊换算函数的登记表：函数名 → 实现
// clang-format off
inline const std::map<std::string, std::function<std::string(double, std::size_t, std::size_t, double&, std::string&)>> specials
{
    {
        { "toDMS"        , [](const double value, [[maybe_unused]] const std::size_t precision, [[maybe_unused]] const std::size_t denominator,
                              double& factor, std::string& unitString) {
            factor = 1.0;
            unitString = "deg";
            return toDms(value);
        }},
        { "toFractional" , [](const double value, [[maybe_unused]] const std::size_t precision, const std::size_t denominator,
                              double& factor, std::string& unitString) {
            factor = 25.4;
            unitString = "in";
            return toFractional(value, denominator);
        }}
    }
}; // clang-format on

    /**
     * @brief 按名字调用特殊换算函数
     * @param name 函数名，来自方案条目的 unitString
     * @param value 以基准单位表示的数值
     * @param precision 保留的小数位数
     * @param denominator 分数分母
     * @param factor 输出参数，写回换算因子
     * @param unitString 输出参数，写回单位串
     * @return 格式化文本；名字未登记时返回空串，由调用方决定如何降级
     */
    inline std::string runSpecial(const std::string &name, const double value, const std::size_t precision, const std::size_t denominator, double &factor, std::string &unitString)
    {
        return specials.contains(name) ? specials.at(name)(value, precision, denominator, factor, unitString) : "";
    }

    /// 内置方案数据包：全部方案、默认小数位数与默认分数分母
    inline const UnitsSchemasDataPack unitSchemasDataPack{schemaSpecs, defaultDecimals, defaultDenominator};
} // namespace ExpressionEngine::Units::UnitsSchemasData
