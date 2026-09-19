/**
 * @file Unit.h
 * @brief 量纲单位及其指数运算
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later；出处与上游版权见 NOTICE
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
         * @param other 待比较的单位
         * @return 各指数都相同时为 true
         */
        bool operator==(const Unit &other) const;

        /**
         * @brief 判断各量纲指数是否不全等
         * @param other 待比较的单位
         * @return 任一指数组不同为 true
         */
        bool operator!=(const Unit &other) const;

        /**
         * @brief 就地相乘，指数相加
         * @param other 右乘的单位
         * @return 自身引用
         * @throws OverflowError/UnderflowError 结果指数越界
         */
        Unit &operator*=(const Unit &other);

        /**
         * @brief 就地相除，指数相减
         * @param other 右除的单位
         * @return 自身引用
         * @throws OverflowError/UnderflowError 结果指数越界
         */
        Unit &operator/=(const Unit &other);

        /**
         * @brief 相乘，指数相加
         * @param other 右乘的单位
         * @return 新的单位
         * @throws OverflowError/UnderflowError 结果指数越界
         */
        Unit operator*(const Unit &other) const;

        /**
         * @brief 相除，指数相减
         * @param other 右除的单位
         * @return 新的单位
         * @throws OverflowError/UnderflowError 结果指数越界
         */
        Unit operator/(const Unit &other) const;

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
        //@{
        /** 预定义的单位类型：量纲以基准单位表示（如 Length 为 mm、Area 为 mm^2）。 */
        static const Unit Acceleration;                          ///< 加速度
        static const Unit AmountOfSubstance;                     ///< 物质的量
        static const Unit Angle;                                 ///< 角度
        static const Unit AngleOfFriction;                       ///< 摩擦角
        static const Unit Area;                                  ///< 面积
        static const Unit CompressiveStrength;                   ///< 抗压强度
        static const Unit Concentration;                         ///< 浓度
        static const Unit CurrentDensity;                        ///< 电流密度
        static const Unit Density;                               ///< 密度
        static const Unit DissipationRate;                       ///< 耗散率
        static const Unit DynamicViscosity;                      ///< 动力黏度
        static const Unit ElectricalCapacitance;                 ///< 电容
        static const Unit ElectricalConductance;                 ///< 电导
        static const Unit ElectricalConductivity;                ///< 电导率
        static const Unit ElectricalInductance;                  ///< 电感
        static const Unit ElectricalResistance;                  ///< 电阻
        static const Unit ElectricCharge;                        ///< 电荷量
        static const Unit ElectricCurrent;                       ///< 电流
        static const Unit ElectricPotential;                     ///< 电势
        static const Unit ElectromagneticPotential;              ///< 电磁势
        static const Unit Force;                                 ///< 力
        static const Unit Frequency;                             ///< 频率
        static const Unit HeatFlux;                              ///< 热流密度
        static const Unit Inertia;                               ///< 转动惯量
        static const Unit InverseArea;                           ///< 每单位面积
        static const Unit InverseLength;                         ///< 每单位长度
        static const Unit InverseVolume;                         ///< 每单位体积
        static const Unit KinematicViscosity;                    ///< 运动黏度
        static const Unit Length;                                ///< 长度
        static const Unit LuminousIntensity;                     ///< 发光强度
        static const Unit MagneticFieldStrength;                 ///< 磁场强度
        static const Unit MagneticFlux;                          ///< 磁通量
        static const Unit MagneticFluxDensity;                   ///< 磁通密度
        static const Unit Magnetization;                         ///< 磁化强度
        static const Unit Mass;                                  ///< 质量
        static const Unit Moment;                                ///< 力矩
        static const Unit One;                                   ///< 无量纲
        static const Unit Pressure;                              ///< 压强
        static const Unit Power;                                 ///< 功率
        static const Unit ShearModulus;                          ///< 剪切模量
        static const Unit SpecificEnergy;                        ///< 比能
        static const Unit SpecificHeat;                          ///< 比热容
        static const Unit Stiffness;                             ///< 刚度
        static const Unit StiffnessDensity;                      ///< 比刚度
        static const Unit Stress;                                ///< 应力
        static const Unit SurfaceChargeDensity;                  ///< 面电荷密度
        static const Unit Temperature;                           ///< 温度
        static const Unit TimeSpan;                              ///< 时间
        static const Unit ThermalConductivity;                   ///< 导热系数
        static const Unit ThermalExpansionCoefficient;           ///< 线膨胀系数
        static const Unit ThermalTransferCoefficient;            ///< 传热系数
        static const Unit UltimateTensileStrength;               ///< 极限抗拉强度
        static const Unit VacuumPermittivity;                    ///< 真空电容率
        static const Unit Velocity;                              ///< 速度
        static const Unit Volume;                                ///< 体积
        static const Unit VolumeChargeDensity;                   ///< 体电荷密度
        static const Unit VolumeFlowRate;                        ///< 体积流量
        static const Unit VolumetricThermalExpansionCoefficient; ///< 体积膨胀系数
        static const Unit Work;                                  ///< 功
        static const Unit YieldStrength;                         ///< 屈服强度
        static const Unit YoungsModulus;                         ///< 弹性模量
        //@}
    };
} // namespace ExpressionEngine::Units
