/**
 * @file Quantity.h
 * @brief 带单位的数值
 * @author Gyanis
 * @date 2026-09-18
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cstdint>
#include <string>

#include <ExpressionEngine/Base/NumericFormatting.h>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Units {
class UnitsSchema;

/**
 * @brief 数量显示时的数字格式设置
 * @details 选项位与 Qt 的 QLocale::NumberOptions 数值对齐，便于沿用既有的持久化配置值，
 *          但本类型不依赖 Qt。
 */
struct QuantityFormat {
    using NumberOptions = std::uint32_t;                              /// 数字格式选项位
    static constexpr NumberOptions None = 0x00;                       ///< 不做特殊处理
    static constexpr NumberOptions OmitGroupSeparator = 0x01;         ///< 不输出分组分隔符
    static constexpr NumberOptions RejectGroupSeparator = 0x02;       ///< 拒绝输入中的分组分隔符
    static constexpr NumberOptions OmitLeadingZeroInExponent = 0x04;  ///< 指数不写前导零（预留）
    static constexpr NumberOptions IncludeTrailingZeroesAfterDot =
        0x08;  ///< 保留小数点后的尾零（预留）

    /// 记数法；直接用基础层的枚举，避免两套同义枚举互相转换
    using NumberFormat = Base::NumberNotation;

    NumberOptions option;  ///< 选项位组合
    NumberFormat format;   ///< 记数法

    /// 取小数位数，未显式设置时回落到全局默认值
    [[nodiscard]] int getPrecision() const;

    /// 设置小数位数
    void setPrecision(int precision) {
        m_precision = precision;
    }

    /// 取分数分母，未显式设置时回落到全局默认值
    [[nodiscard]] int getDenominator() const;

    /// 设置分数分母
    void setDenominator(int denominator) {
        m_denominator = denominator;
    }

    /// 默认构造：定点、不分组、精度与分母取全局默认
    QuantityFormat();

    /**
     * @brief 以记数法与小数位数构造
     * @param format 记数法
     * @param decimals 小数位数，负数表示取全局默认值
     */
    explicit QuantityFormat(NumberFormat format, int decimals = -1);

    /// 取 printf 风格的类型字符
    [[nodiscard]] char toFormat() const {
        switch (format) {
        case NumberFormat::Fixed:
            return 'f';
        case NumberFormat::Scientific:
            return 'e';
        default:
            return 'g';
        }
    }

    /**
     * @brief 由 printf 风格的类型字符反推记数法
     * @param character 类型字符 'f'、'e'、'g'
     * @param isValid 输出参数，可为 nullptr；非空时写入字符是否被识别
     * @return 识别出的记数法；字符不被识别时返回 Default
     */
    static NumberFormat toFormat(char character, bool* isValid = nullptr) {
        if (isValid != nullptr) {
            *isValid = true;
        }
        switch (character) {
        case 'f':
            return NumberFormat::Fixed;
        case 'e':
            return NumberFormat::Scientific;
        case 'g':
            return NumberFormat::Default;
        default:
            if (isValid != nullptr) {
                *isValid = false;
            }
            return NumberFormat::Default;
        }
    }

private:
    int m_precision;    ///< 小数位数，负数表示取全局默认
    int m_denominator;  ///< 分数分母，负数表示取全局默认
};

/**
 * @brief 带单位的数值
 * @details 数值以基准量纲（mm/kg/s/…）表示，单位只记量纲；同一物理量在不同单位下的差异
 *          体现在数值上，例如 1 in 与 25.4 mm 的数值不同而单位相同。
 */
class Quantity {
public:
    /// 默认构造：值为 0、无量纲
    Quantity();

    Quantity(const Quantity&) = default;

    Quantity(Quantity&&) = default;

    /**
     * @brief 以数值与单位构造
     * @param value 数值
     * @param unit 单位，默认无量纲
     */
    explicit Quantity(double value, const Unit& unit = Unit());

    /**
     * @brief 以数值与单位文本构造
     * @details 单位文本经 parse() 解析；文本无法解析时退化为无量纲且数值归零，
     *          需要感知失败请改用 parse() 并捕获 ParserError。
     * @param value 数值
     * @param unit 单位文本，如 "mm"、"kg/m^3"
     */
    explicit Quantity(double value, const std::string& unit);

    ~Quantity() = default;

    /** 四则运算与比较。 */
    //@{
    Quantity operator*(const Quantity& other) const;

    Quantity operator*(double factor) const;

    Quantity operator+(const Quantity& other) const;

    Quantity& operator+=(const Quantity& other);

    Quantity operator-(const Quantity& other) const;

    Quantity& operator-=(const Quantity& other);

    Quantity operator-() const;

    Quantity operator/(const Quantity& other) const;

    Quantity operator/(double factor) const;

    bool operator==(const Quantity& other) const;

    bool operator!=(const Quantity& other) const;

    bool operator<(const Quantity& other) const;

    bool operator>(const Quantity& other) const;

    bool operator<=(const Quantity& other) const;

    bool operator>=(const Quantity& other) const;

    Quantity& operator=(const Quantity&) = default;

    Quantity& operator=(Quantity&&) = default;

    /**
     * @brief 幂运算
     * @param exponent 指数，必须无量纲
     * @return 数值与单位同时取幂的结果
     * @throws UnitsMismatchError 指数带单位，或指数使量纲出现分数次幂
     */
    Quantity pow(const Quantity& exponent) const;

    /**
     * @brief 幂运算
     * @param exponent 指数
     * @return 数值与单位同时取幂的结果
     * @throws UnitsMismatchError 指数使量纲出现分数次幂
     */
    Quantity pow(double exponent) const;

    //@}

    /// 取格式设置
    [[nodiscard]] const QuantityFormat& getFormat() const {
        return m_format;
    }

    /// 设置格式设置
    void setFormat(const QuantityFormat& format) {
        m_format = format;
    }

    /// 取形如 "'25.40 mm'" 的带引号文本，便于回填到表达式
    [[nodiscard]] std::string toString(
        const QuantityFormat& format = QuantityFormat(QuantityFormat::NumberFormat::Default)) const;

    /// 只取数值部分的文本
    [[nodiscard]] std::string toNumber(
        const QuantityFormat& format = QuantityFormat(QuantityFormat::NumberFormat::Default)) const;

    /// 按当前单位方案换算成用户偏好的单位并排版
    [[nodiscard]] std::string getUserString() const;

    /**
     * @brief 按当前单位方案换算成用户偏好的单位并排版
     * @param factor 输出参数，实际使用的换算因子
     * @param unitString 输出参数，实际使用的单位串
     * @return 排版后的文本
     */
    [[nodiscard]] std::string getUserString(double& factor, std::string& unitString) const;

    /**
     * @brief 按指定单位方案换算并排版
     * @param schema 单位方案
     * @param factor 输出参数，实际使用的换算因子
     * @param unitString 输出参数，实际使用的单位串
     * @return 排版后的文本
     */
    [[nodiscard]] std::string
    getUserString(UnitsSchema* schema, double& factor, std::string& unitString) const;

    /// 取可安全回填的文本；用户串无法被自己解析回来时回落到基准单位写法
    [[nodiscard]] std::string getSafeUserString() const;

    /**
     * @brief 解析数量文本
     * @param text 形如 "1.5 mm"、"5'6\""、"1/2 kg" 的文本
     * @return 解析结果
     * @throws ParserError 文本存在词法或语法错误
     */
    [[nodiscard]] static Quantity parse(const std::string& text);

    /**
     * @brief 按区域设置解析用户输入的数量文本
     * @details 先把输入按区域分隔符规范化成标准写法，再交由 parse() 解析。
     * @param text 用户输入文本
     * @param locale 区域快照
     * @return 解析结果
     * @throws ParserError 文本存在词法或语法错误，或数字不符合该区域的分组规则
     */
    [[nodiscard]] static Quantity parseUserInput(const std::string& text,
                                                 const Base::NumericLocaleContext& locale);

    /// 取单位
    [[nodiscard]] const Unit& getUnit() const {
        return m_unit;
    }

    /// 设置单位
    void setUnit(const Unit& unit) {
        m_unit = unit;
    }

    /// 取数值（以基准量纲表示）
    [[nodiscard]] double getValue() const {
        return m_value;
    }

    /// 设置数值
    void setValue(double value) {
        m_value = value;
    }

    /**
     * @brief 取本量相对于参照量的比值
     * @param other 参照量，其数值不能为 0
     * @return 本量的数值除以参照量数值
     */
    [[nodiscard]] double getValueAs(const Quantity& other) const;

    /// 是否为无量纲量
    [[nodiscard]] bool isDimensionless() const;

    /// 是否为无量纲或指定单位的量
    [[nodiscard]] bool isDimensionlessOrUnit(const Unit& unit) const;

    /// 数值是否为有效数字（非 NaN）
    [[nodiscard]] bool isValid() const;

    /// 把数值置为 NaN，表示无效值
    void setInvalid();

    /** 预定义单位下的常用量。 */
    //@{
    static const Quantity NanoMetre;
    static const Quantity MicroMetre;
    static const Quantity CentiMetre;
    static const Quantity DeciMetre;
    static const Quantity Metre;
    static const Quantity MilliMetre;
    static const Quantity KiloMetre;

    static const Quantity Liter;
    static const Quantity MilliLiter;

    static const Quantity Hertz;
    static const Quantity KiloHertz;
    static const Quantity MegaHertz;
    static const Quantity GigaHertz;
    static const Quantity TeraHertz;

    static const Quantity MicroGram;
    static const Quantity MilliGram;
    static const Quantity Gram;
    static const Quantity KiloGram;
    static const Quantity Ton;

    static const Quantity Second;
    static const Quantity Minute;
    static const Quantity Hour;

    static const Quantity Ampere;
    static const Quantity NanoAmpere;
    static const Quantity MicroAmpere;
    static const Quantity MilliAmpere;
    static const Quantity KiloAmpere;
    static const Quantity MegaAmpere;

    static const Quantity Kelvin;
    static const Quantity MilliKelvin;
    static const Quantity MicroKelvin;

    static const Quantity NanoMole;
    static const Quantity MicroMole;
    static const Quantity MilliMole;
    static const Quantity Mole;

    static const Quantity Candela;

    static const Quantity Inch;
    static const Quantity Foot;
    static const Quantity Thou;
    static const Quantity Yard;
    static const Quantity Mile;

    static const Quantity MilePerHour;

    static const Quantity Pound;
    static const Quantity Ounce;
    static const Quantity Stone;
    static const Quantity Hundredweights;

    static const Quantity SquareFoot;
    static const Quantity CubicFoot;

    static const Quantity PoundForce;

    static const Quantity Newton;
    static const Quantity MilliNewton;
    static const Quantity KiloNewton;
    static const Quantity MegaNewton;

    static const Quantity NewtonPerMeter;
    static const Quantity MilliNewtonPerMeter;
    static const Quantity KiloNewtonPerMeter;
    static const Quantity MegaNewtonPerMeter;

    static const Quantity Pascal;
    static const Quantity KiloPascal;
    static const Quantity MegaPascal;
    static const Quantity GigaPascal;

    static const Quantity Bar;
    static const Quantity MilliBar;

    static const Quantity Torr;
    static const Quantity mTorr;
    static const Quantity yTorr;

    static const Quantity PSI;
    static const Quantity KSI;
    static const Quantity MPSI;

    static const Quantity Watt;
    static const Quantity NanoWatt;
    static const Quantity MicroWatt;
    static const Quantity MilliWatt;
    static const Quantity KiloWatt;
    static const Quantity VoltAmpere;

    static const Quantity Volt;
    static const Quantity MilliVolt;
    static const Quantity KiloVolt;

    static const Quantity MegaSiemens;
    static const Quantity KiloSiemens;
    static const Quantity Siemens;
    static const Quantity MilliSiemens;
    static const Quantity MicroSiemens;

    static const Quantity Ohm;
    static const Quantity KiloOhm;
    static const Quantity MegaOhm;

    static const Quantity Coulomb;

    static const Quantity Tesla;
    static const Quantity MilliTesla;
    static const Quantity Gauss;

    static const Quantity Weber;

    static const Quantity Farad;
    static const Quantity MilliFarad;
    static const Quantity MicroFarad;
    static const Quantity NanoFarad;
    static const Quantity PicoFarad;

    static const Quantity Henry;
    static const Quantity MilliHenry;
    static const Quantity MicroHenry;
    static const Quantity NanoHenry;

    static const Quantity Joule;
    static const Quantity MilliJoule;
    static const Quantity KiloJoule;
    static const Quantity NewtonMeter;
    static const Quantity VoltAmpereSecond;
    static const Quantity WattSecond;
    static const Quantity KiloWattHour;
    static const Quantity ElectronVolt;
    static const Quantity KiloElectronVolt;
    static const Quantity MegaElectronVolt;
    static const Quantity Calorie;
    static const Quantity KiloCalorie;

    static const Quantity KMH;
    static const Quantity MPH;

    static const Quantity Degree;
    static const Quantity Radian;
    static const Quantity Gon;
    static const Quantity AngMinute;
    static const Quantity AngSecond;
    //@}

private:
    double m_value;           ///< 数值，以基准量纲表示
    Unit m_unit;              ///< 单位（量纲）
    QuantityFormat m_format;  ///< 显示格式
};
}  // namespace ExpressionEngine::Units
