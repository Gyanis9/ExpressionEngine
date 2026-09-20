// 本文件覆盖 Unit 的量纲运算、文本表示与越界拒绝面。

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <cstdint>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Units
{
    namespace
    {


        /// 失败时把指数向量摊平进日志，省得逐个猜是哪一位不对
        std::string describe(const UnitExponents &exponents)
        {
            std::string text;
            for (const std::int8_t exponent: exponents)
            {
                if (!text.empty())
                {
                    text += ", ";
                }
                text += std::to_string(static_cast<int>(exponent));
            }
            return text;
        }

        /**
         * @brief 钉住：单位相等只看量纲指数，与书写形式无关
         */
        TEST(UnitTest, EqualityComparesExponentsOnly)
        {
            EXPECT_EQ(Unit::Length, Unit::Length);
            EXPECT_NE(Unit::Length, Unit::Area);
            EXPECT_EQ(Unit(1, 0, 0), Unit::Length);
            EXPECT_EQ(Unit(), Unit::One);
        }

        /**
         * @brief 钉住：乘除运算按分量合并指数
         */
        TEST(UnitTest, MultiplicationAndDivisionCombineExponents)
        {
            const Unit area = Unit::Length * Unit::Length;
            EXPECT_EQ(area, Unit::Area);
            EXPECT_EQ(area.exponents().at(0), 2);

            const Unit velocity = Unit::Length / Unit::TimeSpan;
            EXPECT_EQ(velocity, Unit::Velocity);
            EXPECT_EQ(velocity.exponents().at(0), 1);
            EXPECT_EQ(velocity.exponents().at(2), -1);

            // 全量纲相除回到无量纲
            EXPECT_EQ(Unit::Length / Unit::Length, Unit::One);
        }

        /**
         * @brief 钉住：复合写法按分子分母拼接，分母多分量要加括号
         */
        TEST(UnitTest, StringRepresentation)
        {
            EXPECT_EQ(Unit::Length.getString(), "mm");
            EXPECT_EQ(Unit::Area.getString(), "mm^2");
            EXPECT_EQ((Unit::Length / Unit::TimeSpan).getString(), "mm/s");
            EXPECT_EQ(Unit::Velocity.getTypeString(), "Velocity");
            // 无量纲单位的紧凑写法是空串，量纲信息全在数值里
            EXPECT_EQ(Unit::One.getString(), "");
            EXPECT_EQ(Unit::One.getTypeString(), "1");
        }

        /**
         * @brief 钉住：开方要求每个指数都能整除，否则报错而不是给出近似结果
         */
        TEST(UnitTest, RootRejectsNonDivisibleExponent)
        {
            EXPECT_EQ(Unit::Area.root(2), Unit::Length);
            EXPECT_EQ(Unit::Volume.root(3), Unit::Length);

            // 长度指数是 1，开二次方无法表示
            EXPECT_THROW(static_cast<void>(Unit::Length.root(2)), Base::UnitsMismatchError);
            // 开方次数为 0 直接拒绝
            EXPECT_THROW(static_cast<void>(Unit::Length.root(0)), Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：幂次必须让每个指数落在整数格点上，分数次幂报错
         */
        TEST(UnitTest, PowRejectsFractionalExponent)
        {
            EXPECT_EQ(Unit::Length.pow(2), Unit::Area);

            // 0.5 次幂会让长度为 1 的指数变成 0.5
            EXPECT_THROW(static_cast<void>(Unit::Length.pow(0.5)), Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：指数越界时报错而不是静默截断
         */
        TEST(UnitTest, ExponentOverflowIsRejected)
        {
            // 上限判定沿用原实现：正方向「达到上限」即越界，负方向「低于下限」才越界，因此 -8 仍可表示
            EXPECT_THROW(static_cast<void>(Unit::Length.pow(8)), Base::OverflowError);
            EXPECT_THROW(static_cast<void>(Unit::Length.pow(-9)), Base::UnderflowError);
            EXPECT_THROW(static_cast<void>(Unit(9, 0, 0)), Base::OverflowError);

            // 边界内的幂次正常返回
            EXPECT_EQ(Unit::Length.pow(-8).exponents().at(0), -8);
            EXPECT_EQ(Unit::Length.pow(7).exponents().at(0), 7);
        }

        /**
         * @brief 钉住：带类型名构造时反查不再生效，类型名按构造时给出的值返回
         */
        TEST(UnitTest, ExplicitTypeNameOverridesLookup)
        {
            constexpr Unit custom{UnitExponents{1, 0, 0, 0, 0, 0, 0, 0}, "CustomLength"};
            EXPECT_EQ(custom.getTypeString(), "CustomLength");
        }

        /**
         * @brief 钉住：完整描述文本同时给出符号、指数与类型名；立方根是开三次方
         */
        TEST(UnitTest, DescriptionAndCubeRoot)
        {
            EXPECT_EQ(Unit::Length.representation(), "Unit: mm (1,0,0,0,0,0,0,0) [Length]");

            EXPECT_EQ(Unit::Volume.cbrt(), Unit::Length);
            EXPECT_EQ(Unit::Length.pow(3).cbrt(), Unit::Length);
            // 指数不能被 3 整除时立方根无法表示，报错而不是给近似单位
            EXPECT_THROW(static_cast<void>(Unit::Length.cbrt()), Base::UnitsMismatchError);
        }


        /**
         * @brief 钉住：每个具名量纲的指数向量与物理定义一致
         * @details 期望值按「这个量是什么」独立写出（顺序：长度、质量、时间、电流、温度、物质的量、
         *          发光强度、角度），不是从实现的对照表里抄的——抄来的断言只会把笔误一起钉死。
         */
        TEST(UnitTest, NamedUnitsMatchTheirPhysicalDimensions)
        {
            const std::vector<std::pair<std::string_view, std::pair<const Unit, UnitExponents> > > dimensions{
                    {"Acceleration", {Unit::Acceleration, {1,0,-2}}},
                    {"AmountOfSubstance", {Unit::AmountOfSubstance, {0,0,0,0,0,1}}},
                    {"Angle", {Unit::Angle, {0,0,0,0,0,0,0,1}}},
                    {"AngleOfFriction", {Unit::AngleOfFriction, {0,0,0,0,0,0,0,1}}},
                    {"Area", {Unit::Area, {2}}},
                    {"CompressiveStrength", {Unit::CompressiveStrength, {-1,1,-2}}},
                    {"Concentration", {Unit::Concentration, {-3,0,0,0,0,1}}},
                    {"CurrentDensity", {Unit::CurrentDensity, {-2,0,0,1}}},
                    {"Density", {Unit::Density, {-3,1}}},
                    {"DissipationRate", {Unit::DissipationRate, {2,0,-3}}},
                    {"DynamicViscosity", {Unit::DynamicViscosity, {-1,1,-1}}},
                    {"ElectricalCapacitance", {Unit::ElectricalCapacitance, {-2,-1,4,2}}},
                    {"ElectricalConductance", {Unit::ElectricalConductance, {-2,-1,3,2}}},
                    {"ElectricalConductivity", {Unit::ElectricalConductivity, {-3,-1,3,2}}},
                    {"ElectricalInductance", {Unit::ElectricalInductance, {2,1,-2,-2}}},
                    {"ElectricalResistance", {Unit::ElectricalResistance, {2,1,-3,-2}}},
                    {"ElectricCharge", {Unit::ElectricCharge, {0,0,1,1}}},
                    {"ElectricCurrent", {Unit::ElectricCurrent, {0,0,0,1}}},
                    {"ElectricPotential", {Unit::ElectricPotential, {2,1,-3,-1}}},
                    {"ElectromagneticPotential", {Unit::ElectromagneticPotential, {1,1,-2,-1}}},
                    {"Force", {Unit::Force, {1,1,-2}}},
                    {"Frequency", {Unit::Frequency, {0,0,-1}}},
                    {"HeatFlux", {Unit::HeatFlux, {0,1,-3}}},
                    {"Inertia", {Unit::Inertia, {2,1}}},
                    {"InverseArea", {Unit::InverseArea, {-2}}},
                    {"InverseLength", {Unit::InverseLength, {-1}}},
                    {"InverseVolume", {Unit::InverseVolume, {-3}}},
                    {"KinematicViscosity", {Unit::KinematicViscosity, {2,0,-1}}},
                    {"Length", {Unit::Length, {1}}},
                    {"LuminousIntensity", {Unit::LuminousIntensity, {0,0,0,0,0,0,1}}},
                    {"MagneticFieldStrength", {Unit::MagneticFieldStrength, {-1,0,0,1}}},
                    {"MagneticFlux", {Unit::MagneticFlux, {2,1,-2,-1}}},
                    {"MagneticFluxDensity", {Unit::MagneticFluxDensity, {0,1,-2,-1}}},
                    {"Magnetization", {Unit::Magnetization, {-1,0,0,1}}},
                    {"Mass", {Unit::Mass, {0,1}}},
                    {"Moment", {Unit::Moment, {2,1,-2}}},
                    {"One", {Unit::One, {0}}},
                    {"Pressure", {Unit::Pressure, {-1,1,-2}}},
                    {"Power", {Unit::Power, {2,1,-3}}},
                    {"ShearModulus", {Unit::ShearModulus, {-1,1,-2}}},
                    {"SpecificEnergy", {Unit::SpecificEnergy, {2,0,-2}}},
                    {"SpecificHeat", {Unit::SpecificHeat, {2,0,-2,0,-1}}},
                    {"Stiffness", {Unit::Stiffness, {0,1,-2}}},
                    {"StiffnessDensity", {Unit::StiffnessDensity, {-2,1,-2}}},
                    {"Stress", {Unit::Stress, {-1,1,-2}}},
                    {"SurfaceChargeDensity", {Unit::SurfaceChargeDensity, {-2,0,1,1}}},
                    {"Temperature", {Unit::Temperature, {0,0,0,0,1}}},
                    {"TimeSpan", {Unit::TimeSpan, {0,0,1}}},
                    {"ThermalConductivity", {Unit::ThermalConductivity, {1,1,-3,0,-1}}},
                    {"ThermalExpansionCoefficient", {Unit::ThermalExpansionCoefficient, {0,0,0,0,-1}}},
                    {"ThermalTransferCoefficient", {Unit::ThermalTransferCoefficient, {0,1,-3,0,-1}}},
                    {"UltimateTensileStrength", {Unit::UltimateTensileStrength, {-1,1,-2}}},
                    {"VacuumPermittivity", {Unit::VacuumPermittivity, {-3,-1,4,2}}},
                    {"Velocity", {Unit::Velocity, {1,0,-1}}},
                    {"Volume", {Unit::Volume, {3}}},
                    {"VolumeChargeDensity", {Unit::VolumeChargeDensity, {-3,0,1,1}}},
                    {"VolumeFlowRate", {Unit::VolumeFlowRate, {3,0,-1}}},
                    {"VolumetricThermalExpansionCoefficient", {Unit::VolumetricThermalExpansionCoefficient, {0,0,0,0,-1}}},
                    {"Work", {Unit::Work, {2,1,-2}}},
                    {"YieldStrength", {Unit::YieldStrength, {-1,1,-2}}},
                    {"YoungsModulus", {Unit::YoungsModulus, {-1,1,-2}}},
            };

            for (const auto &[name, unitAndExponents]: dimensions)
            {
                const Unit          &unit      = unitAndExponents.first;
                const UnitExponents &expected  = unitAndExponents.second;
                const UnitExponents &actual    = unit.exponents();

                EXPECT_TRUE(actual == expected) << name << " 实得 [" << describe(actual) << "]";
            }
        }

        /**
         * @brief 钉住：同量纲的名字别名如何被解析——计算出来的单位取对照表里最先出现的名字
         * @details 排版按类型名查方案的换算条目，所以「算出来的能量会被叫成 Moment」是宿主看得见
         *          的行为；对照表一旦重排就会静默改变排版结果，这里钉住当前的先后关系。
         */
        TEST(UnitTest, ComputedUnitsResolveToTheFirstTypeNameWithSameDimensions)
        {
            EXPECT_EQ(Unit::Work.getTypeString(), "Work");
            EXPECT_EQ(Unit::Moment.getTypeString(), "Moment");
            EXPECT_EQ(Unit::Work, Unit::Moment);

            // 乘出来的单位没有显式名字，反查对照表取最先命中的那条
            const Unit energy = Unit::Force * Unit::Length;
            EXPECT_EQ(energy.getTypeString(), "Moment");
            EXPECT_TRUE(energy == Unit::Work);

            const Unit inverseTemperature = Unit(0, 0, 0, 0, -1);
            EXPECT_EQ(inverseTemperature.getTypeString(), "ThermalExpansionCoefficient");
            EXPECT_EQ(Unit::VolumetricThermalExpansionCoefficient.getTypeString(), "VolumetricThermalExpansionCoefficient");

            // 抗压强度是「借 Pressure 之名」构造的别名：类型名报的是 Pressure
            EXPECT_EQ(Unit::CompressiveStrength.getTypeString(), "Pressure");
            EXPECT_EQ(Unit::CompressiveStrength, Unit::Pressure);
        }

    } // namespace
}     // namespace ExpressionEngine::Units
