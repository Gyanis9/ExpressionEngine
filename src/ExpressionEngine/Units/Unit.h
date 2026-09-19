/**
 * @file Unit.h
 * @brief 量纲单位及其指数运算
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Units
{
    /**
     * @brief 七个基本量纲加角度的符号表
     * @details 顺序即指数数组的分量顺序：长度 mm、质量 kg、时间 s、电流 A、温度 K、
     *          物质的量 mol、发光强度 cd、角度 deg。
     */
    constexpr auto unitSymbols = std::to_array<std::string_view>({"mm", "kg", "s", "A", "K", "mol", "cd", "deg"});

    /// 基本量纲的个数，即指数数组的长度
    constexpr auto unitNumberExponents{unitSymbols.size()};

    /**
     * @brief 各基本量纲的指数数组
     * @details 分量顺序与 unitSymbols 一致；用 int8 存放是因为指数上限只有个位数。
     */
    using UnitExponents = std::array<std::int8_t, unitNumberExponents>;

    /**
     * @brief 单个量纲指数的绝对值上限
     * @details 超出即认为运算已越界：继续乘下去指数会溢出 int8，结果不可信。
     */
    constexpr auto unitExponentLimit{8};

    /**
     * @brief 由量纲指数向量表示的单位
     * @details 单位只记量纲与指数，不记比例；换算系数由 Quantity 承载。等号比较只看指数，
     *          因此 "mm" 与 "m" 在单位层面相等，量级差异体现在数值上。
     */
    class Unit final
    {
    public:
        Unit() = default;

        /**
         * @brief 以指数向量构造单位
         * @param exponents 各基本量纲的指数，顺序与 unitSymbols 一致
         * @param name 类型名（如 "Length"），留空时由指数反查
         * @throws OverflowError 任一指数达到上限
         * @throws UnderflowError 任一指数低于下限
         */
        explicit constexpr Unit(const UnitExponents exponents, const std::string_view name = "") :
            m_exponents{exponents}, m_name{name}
        {
            checkRange();
        }

        /**
         * @brief 以各量纲指数逐个构造单位
         * @details 便于宿主按分量书写单位；超出 int8 可表示范围的值会被夹到边界后再校验。
         * @param length 长度指数
         * @param mass 质量指数
         * @param time 时间指数
         * @param electricCurrent 电流指数
         * @param thermodynamicTemperature 热力学温度指数
         * @param amountOfSubstance 物质的量指数
         * @param luminousIntensity 发光强度指数
         * @param angle 角度指数
         */
        explicit Unit(int length, int mass  = 0, int time              = 0, int electricCurrent = 0, int thermodynamicTemperature = 0,
                      int amountOfSubstance = 0, int luminousIntensity = 0, int angle           = 0);

        /**
         * @brief 判断各量纲指数是否全等，不比较比例
         * @param that 待比较的单位
         * @return 各指数都相同时为 true
         */
        bool operator==(const Unit &that) const;

        /**
         * @brief 判断各量纲指数是否不全等
         * @param that 待比较的单位
         * @return 任一指数组不同为 true
         */
        bool operator!=(const Unit &that) const;

        /**
         * @brief 就地相乘，指数相加
         * @param that 右乘的单位
         * @return 自身引用
         * @throws OverflowError/UnderflowError 结果指数越界
         */
        Unit &operator*=(const Unit &that);

        /**
         * @brief 就地相除，指数相减
         * @param that 右除的单位
         * @return 自身引用
         * @throws OverflowError/UnderflowError 结果指数越界
         */
        Unit &operator/=(const Unit &that);

        /**
         * @brief 相乘，指数相加
         * @param that 右乘的单位
         * @return 新的单位
         * @throws OverflowError/UnderflowError 结果指数越界
         */
        Unit operator*(const Unit &that) const;

        /**
         * @brief 相除，指数相减
         * @param that 右除的单位
         * @return 新的单位
         * @throws OverflowError/UnderflowError 结果指数越界
         */
        Unit operator/(const Unit &that) const;

        /**
         * @brief 求单位的幂
         * @param exponent 指数，必须使所有量纲指数乘完后仍为整数
         * @return 新的单位
         * @throws UnitsMismatchError 指数使某个量纲出现分数次幂
         * @throws OverflowError/UnderflowError 结果指数越界
         */
        [[nodiscard]] Unit pow( double exponent) const;

        /**
         * @brief 求单位的整数次根
         * @param rootDegree 根次数，必须大于 0
         * @return 新的单位
         * @throws UnitsMismatchError rootDegree 等于 0，或某个量纲指数不能被 rootDegree 整除
         * @throws OverflowError/UnderflowError 结果指数越界
         */
        [[nodiscard]] Unit root( uint8_t rootDegree) const;

        /// 取各量纲指数
        [[nodiscard]] UnitExponents exponents() const;

        /// 取长度量纲的指数，用于快速判定是否属于长度类量
        [[nodiscard]] int length() const;

        /// 取单位的紧凑写法，如 "mm"、"mm^2"、"mm*kg/s^2"
        [[nodiscard]] std::string getString() const;

        /// 取类型名，如 "Area"、"Length"；无法归类时返回空串
        [[nodiscard]] std::string getTypeString() const;

        /// 取带指数与类型名的完整描述，如 "Unit: mm (1,0,0,0,0,0,0,0) [Length]"
        [[nodiscard]] std::string representation() const;

        /// 取平方根，等价于 root(2)
        [[nodiscard]] Unit sqrt() const
        {
            return root(2);
        }

        /// 取立方根，等价于 root(3)
        [[nodiscard]] Unit cbrt() const
        {
            return root(3);
        }

    private:
        UnitExponents    m_exponents{}; ///< 各基本量纲的指数
        std::string_view m_name;        ///< 类型名；为空时由指数反查

        /// 校验各指数都落在 [−limit, limit) 之内
        constexpr void checkRange() const
        {
            for (const auto exponent: m_exponents)
            {
                // 越界说明运算已不可信，宁可报错也不静默截断
                if (exponent >= unitExponentLimit)
                {
                    throw Base::OverflowError("单位指数超出上限，请缩小量纲的幂次后重试");
                }
                if (exponent < -unitExponentLimit)
                {
                    throw Base::UnderflowError("单位指数低于下限，请增大量纲的幂次后重试");
                }
            }
        }

        /// 分别返回指数为正与为负的分量下标
        [[nodiscard]] std::pair<std::vector<std::size_t>, std::vector<std::size_t> > nonZeroValueIndexes() const;

    public:
        static const Unit Acceleration;
        static const Unit AmountOfSubstance;
        static const Unit Angle;
        static const Unit AngleOfFriction;
        static const Unit Area;
        static const Unit CompressiveStrength;
        static const Unit Concentration;
        static const Unit CurrentDensity;
        static const Unit Density;
        static const Unit DissipationRate;
        static const Unit DynamicViscosity;
        static const Unit ElectricalCapacitance;
        static const Unit ElectricalConductance;
        static const Unit ElectricalConductivity;
        static const Unit ElectricalInductance;
        static const Unit ElectricalResistance;
        static const Unit ElectricCharge;
        static const Unit ElectricCurrent;
        static const Unit ElectricPotential;
        static const Unit ElectromagneticPotential;
        static const Unit Force;
        static const Unit Frequency;
        static const Unit HeatFlux;
        static const Unit Inertia;
        static const Unit InverseArea;
        static const Unit InverseLength;
        static const Unit InverseVolume;
        static const Unit KinematicViscosity;
        static const Unit Length;
        static const Unit LuminousIntensity;
        static const Unit MagneticFieldStrength;
        static const Unit MagneticFlux;
        static const Unit MagneticFluxDensity;
        static const Unit Magnetization;
        static const Unit Mass;
        static const Unit Moment;
        static const Unit One;
        static const Unit Pressure;
        static const Unit Power;
        static const Unit ShearModulus;
        static const Unit SpecificEnergy;
        static const Unit SpecificHeat;
        static const Unit Stiffness;
        static const Unit StiffnessDensity;
        static const Unit Stress;
        static const Unit SurfaceChargeDensity;
        static const Unit Temperature;
        static const Unit TimeSpan;
        static const Unit ThermalConductivity;
        static const Unit ThermalExpansionCoefficient;
        static const Unit ThermalTransferCoefficient;
        static const Unit UltimateTensileStrength;
        static const Unit VacuumPermittivity;
        static const Unit Velocity;
        static const Unit Volume;
        static const Unit VolumeChargeDensity;
        static const Unit VolumeFlowRate;
        static const Unit VolumetricThermalExpansionCoefficient;
        static const Unit Work;
        static const Unit YieldStrength;
        static const Unit YoungsModulus;
    };
} // namespace ExpressionEngine::Units
