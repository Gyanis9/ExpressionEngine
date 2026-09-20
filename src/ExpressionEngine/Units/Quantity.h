/**
 * @file Quantity.h
 * @brief 带单位的数值
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <compare>
#include <cstdint>
#include <string>

#include <ExpressionEngine/Base/NumericFormatting.h>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Units
{
    class UnitsSchema;

    /**
     * @brief 数量显示时的数字格式设置
     * @details 选项位与 Qt 的 QLocale::NumberOptions 数值对齐，便于沿用既有的持久化配置值，
     *          但本类型不依赖 Qt。
     */
    struct QuantityFormat
    {
        using NumberOptions                                          = std::uint32_t; ///< 数字格式选项位
        static constexpr NumberOptions None                          = 0x00;          ///< 不做特殊处理
        static constexpr NumberOptions OmitGroupSeparator            = 0x01;          ///< 不输出分组分隔符
        static constexpr NumberOptions RejectGroupSeparator          = 0x02;          ///< 拒绝输入中的分组分隔符
        static constexpr NumberOptions OmitLeadingZeroInExponent     = 0x04;          ///< 指数不写前导零（预留）
        static constexpr NumberOptions IncludeTrailingZeroesAfterDot = 0x08;          ///< 保留小数点后的尾零（预留）

        /// 记数法；直接用基础层的枚举，避免两套同义枚举互相转换
        using NumberFormat = Base::NumberNotation;

        NumberOptions option; ///< 选项位组合
        NumberFormat  format; ///< 记数法

        /// 取小数位数，未显式设置时回落到全局默认值
        [[nodiscard]] int getPrecision() const;

        /// 设置小数位数
        void setPrecision(int precision)
        {
            m_precision = precision;
        }

        /// 取分数分母，未显式设置时回落到全局默认值
        [[nodiscard]] int getDenominator() const;

        /// 设置分数分母
        void setDenominator(int denominator)
        {
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
        [[nodiscard]] char toFormat() const
        {
            switch (format)
            {
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
        static NumberFormat toFormat(char character, bool *isValid = nullptr)
        {
            if (isValid != nullptr)
            {
                *isValid = true;
            }
            switch (character)
            {
                case 'f':
                    return NumberFormat::Fixed;
                case 'e':
                    return NumberFormat::Scientific;
                case 'g':
                    return NumberFormat::Default;
                default:
                    if (isValid != nullptr)
                    {
                        *isValid = false;
                    }
                    return NumberFormat::Default;
            }
        }

    private:
        int m_precision;   ///< 小数位数，负数表示取全局默认
        int m_denominator; ///< 分数分母，负数表示取全局默认
    };

    /**
     * @brief 带单位的数值
     * @details 数值以基准量纲（mm/kg/s/…）表示，单位只记量纲；同一物理量在不同单位下的差异
     *          体现在数值上，例如 1 in 与 25.4 mm 的数值不同而单位相同。
     */
    class Quantity
    {
    public:
        /// 默认构造：值为 0、无量纲
        Quantity();

        Quantity(const Quantity &) = default;

        Quantity(Quantity &&) = default;

        /**
         * @brief 以数值与单位构造
         * @param value 数值
         * @param unit 单位，默认无量纲
         */
        explicit Quantity(double value, const Unit &unit = Unit());

        /**
         * @brief 以数值与单位文本构造
         * @details 单位文本经 parse() 解析；文本无法解析时退化为无量纲且数值归零，
         *          需要感知失败请改用 parse() 并捕获 ParserError。
         * @param value 数值
         * @param unit 单位文本，如 "mm"、"kg/m^3"
         */
        explicit Quantity(double value, const std::string &unit);

        ~Quantity() = default;

        /** 四则运算与比较。 */
        //@{
        /**
         * @brief 量相乘，量纲指数相加
         * @param other 右乘的量
         * @return 乘积
         */
        Quantity operator*(const Quantity &other) const;

        /**
         * @brief 量乘纯数，量纲不变
         * @param factor 缩放系数
         * @return 缩放后的量
         */
        Quantity operator*(double factor) const;

        /**
         * @brief 量相加，要求两侧量纲相同
         * @param other 加数
         * @return 和
         * @throws UnitsMismatchError 两侧量纲不同
         */
        Quantity operator+(const Quantity &other) const;

        /**
         * @brief 就地加上另一个量
         * @param other 加数
         * @return 自身引用
         * @throws UnitsMismatchError 两侧量纲不同
         */
        Quantity &operator+=(const Quantity &other);

        /**
         * @brief 量相减，要求两侧量纲相同
         * @param other 减数
         * @return 差
         * @throws UnitsMismatchError 两侧量纲不同
         */
        Quantity operator-(const Quantity &other) const;

        /**
         * @brief 就地减去另一个量
         * @param other 减数
         * @return 自身引用
         * @throws UnitsMismatchError 两侧量纲不同
         */
        Quantity &operator-=(const Quantity &other);

        Quantity operator-() const;

        /**
         * @brief 量相除，量纲指数相减
         * @param other 除数
         * @return 商
         */
        Quantity operator/(const Quantity &other) const;

        /**
         * @brief 量除以纯数，量纲不变
         * @param factor 除数，不能为 0
         * @return 商
         */
        Quantity operator/(double factor) const;

        /**
         * @brief 判断数值与量纲是否都相等
         * @param other 待比较的量
         * @return 相等为 true；仅量纲不同时返回 false 而不抛错
         */
        bool operator==(const Quantity &other) const;

        /**
         * @brief 判断数值或量纲是否不同
         * @param other 待比较的量
         * @return 不等为 true
         */
        bool operator!=(const Quantity &other) const;

        /**
         * @brief 按数值大小比较，要求两侧量纲相同
         * @details 返回偏序，编译器据此生成 <、>、<=、>= 四个关系；NaN 参与比较时为无序，
         *          四个关系一律为假。
         * @param other 右侧的量
         * @return 数值比较的偏序；量纲不同时抛错而不比较
         * @throws UnitsMismatchError 两侧量纲不同
         */
        std::partial_ordering operator<=>(const Quantity &other) const;

        Quantity &operator=(const Quantity &) = default;

        Quantity &operator=(Quantity &&) = default;

        /**
         * @brief 幂运算
         * @param exponent 指数，必须无量纲
         * @return 数值与单位同时取幂的结果
         * @throws UnitsMismatchError 指数带单位，或指数使量纲出现分数次幂
         */
        [[nodiscard]] Quantity pow(const Quantity &exponent) const;

        /**
         * @brief 幂运算
         * @param exponent 指数
         * @return 数值与单位同时取幂的结果
         * @throws UnitsMismatchError 指数使量纲出现分数次幂
         */
        [[nodiscard]] Quantity pow(double exponent) const;

        //@}

        /// 取格式设置
        [[nodiscard]] const QuantityFormat &getFormat() const
        {
            return m_format;
        }

        /// 设置格式设置
        void setFormat(const QuantityFormat &format)
        {
            m_format = format;
        }

        /// 取形如 "'25.40 mm'" 的带引号文本，便于回填到表达式
        [[nodiscard]] std::string toString(const QuantityFormat &format = QuantityFormat(QuantityFormat::NumberFormat::Default)) const;

        /// 只取数值部分的文本
        [[nodiscard]] std::string toNumber(const QuantityFormat &format = QuantityFormat(QuantityFormat::NumberFormat::Default)) const;

        /// 按当前单位方案换算成用户偏好的单位并排版
        [[nodiscard]] std::string getUserString() const;

        /**
         * @brief 按当前单位方案换算成用户偏好的单位并排版
         * @param factor 输出参数，实际使用的换算因子
         * @param unitString 输出参数，实际使用的单位串
         * @return 排版后的文本
         */
        [[nodiscard]] std::string getUserString(double &factor, std::string &unitString) const;

        /**
         * @brief 按指定单位方案换算并排版
         * @param schema 单位方案
         * @param factor 输出参数，实际使用的换算因子
         * @param unitString 输出参数，实际使用的单位串
         * @return 排版后的文本
         */
        [[nodiscard]] std::string getUserString(const UnitsSchema *schema, double &factor, std::string &unitString) const;

        /// 取可安全回填的文本；用户串无法被自己解析回来时回落到基准单位写法
        [[nodiscard]] std::string getSafeUserString() const;

        /**
         * @brief 解析数量文本
         * @param text 形如 "1.5 mm"、"5'6\""、"1/2 kg" 的文本
         * @return 解析结果
         * @throws ParserError 文本存在词法或语法错误
         */
        [[nodiscard]] static Quantity parse(const std::string &text);

        /**
         * @brief 按区域设置解析用户输入的数量文本
         * @details 先把输入按区域分隔符规范化成标准写法，再交由 parse() 解析。
         * @param text 用户输入文本
         * @param locale 区域快照
         * @return 解析结果
         * @throws ParserError 文本存在词法或语法错误，或数字不符合该区域的分组规则
         */
        [[nodiscard]] static Quantity parseUserInput(const std::string &text, const Base::NumericLocaleContext &locale);

        /// 取单位
        [[nodiscard]] const Unit &getUnit() const
        {
            return m_unit;
        }

        /// 设置单位
        void setUnit(const Unit &unit)
        {
            m_unit = unit;
        }

        /// 取数值（以基准量纲表示）
        [[nodiscard]] double getValue() const
        {
            return m_value;
        }

        /// 设置数值
        void setValue(const double value)
        {
            m_value = value;
        }

        /**
         * @brief 取本量相对于参照量的比值
         * @details 数值都以基准单位存储，所以相除就是换算比（1 m 相对 1 mm 得 1000）。
         *          参照量数值为 0 时按 IEEE 得到无穷大，与 operator/ 同一口径，不额外抛错。
         * @param other 参照量，量纲必须与本量一致
         * @return 本量的数值除以参照量的数值
         * @throws UnitsMismatchError 参照量与本量量纲不同
         */
        [[nodiscard]] double getValueAs(const Quantity &other) const;

        /// 是否为无量纲量
        [[nodiscard]] bool isDimensionless() const;

        /// 是否为无量纲或指定单位的量
        [[nodiscard]] bool isDimensionlessOrUnit(const Unit &unit) const;

        /// 数值是否为有效数字（非 NaN）
        [[nodiscard]] bool isValid() const;

        /// 把数值置为 NaN，表示无效值
        void setInvalid();

        /** 预定义单位下的常用量。 */
        //@{
        static const Quantity NanoMetre;  ///< 纳米
        static const Quantity MicroMetre; ///< 微米
        static const Quantity CentiMetre; ///< 厘米
        static const Quantity DeciMetre;  ///< 分米
        static const Quantity Metre;      ///< 米
        static const Quantity MilliMetre; ///< 毫米（基准长度单位）
        static const Quantity KiloMetre;  ///< 千米

        static const Quantity Liter;      ///< 升
        static const Quantity MilliLiter; ///< 毫升

        static const Quantity Hertz;     ///< 赫兹
        static const Quantity KiloHertz; ///< 千赫兹
        static const Quantity MegaHertz; ///< 兆赫兹
        static const Quantity GigaHertz; ///< 吉赫兹
        static const Quantity TeraHertz; ///< 太赫兹

        static const Quantity MicroGram; ///< 微克
        static const Quantity MilliGram; ///< 毫克
        static const Quantity Gram;      ///< 克
        static const Quantity KiloGram;  ///< 千克（基准质量单位）
        static const Quantity Ton;       ///< 吨

        static const Quantity Second; ///< 秒（基准时间单位）
        static const Quantity Minute; ///< 分钟
        static const Quantity Hour;   ///< 小时

        static const Quantity Ampere;      ///< 安培（基准电流单位）
        static const Quantity NanoAmpere;  ///< 纳安
        static const Quantity MicroAmpere; ///< 微安
        static const Quantity MilliAmpere; ///< 毫安
        static const Quantity KiloAmpere;  ///< 千安
        static const Quantity MegaAmpere;  ///< 兆安

        static const Quantity Kelvin;      ///< 开尔文（基准热力学温度单位）
        static const Quantity MilliKelvin; ///< 毫开尔文
        static const Quantity MicroKelvin; ///< 微开尔文

        static const Quantity NanoMole;  ///< 纳摩尔
        static const Quantity MicroMole; ///< 微摩尔
        static const Quantity MilliMole; ///< 毫摩尔
        static const Quantity Mole;      ///< 摩尔（基准物质的量单位）

        static const Quantity Candela; ///< 坎德拉（基准发光强度单位）

        static const Quantity Inch; ///< 英寸
        static const Quantity Foot; ///< 英尺
        static const Quantity Thou; ///< 密耳，千分之一英寸
        static const Quantity Yard; ///< 码
        static const Quantity Mile; ///< 英里

        static const Quantity MilePerHour; ///< 英里每小时

        static const Quantity Pound;          ///< 磅
        static const Quantity Ounce;          ///< 盎司
        static const Quantity Stone;          ///< 英石
        static const Quantity Hundredweights; ///< 英担

        static const Quantity SquareFoot; ///< 平方英尺
        static const Quantity CubicFoot;  ///< 立方英尺

        static const Quantity PoundForce; ///< 磅力

        static const Quantity Newton;      ///< 牛顿
        static const Quantity MilliNewton; ///< 毫牛
        static const Quantity KiloNewton;  ///< 千牛
        static const Quantity MegaNewton;  ///< 兆牛

        static const Quantity NewtonPerMeter;      ///< 牛每米
        static const Quantity MilliNewtonPerMeter; ///< 毫牛每米
        static const Quantity KiloNewtonPerMeter;  ///< 千牛每米
        static const Quantity MegaNewtonPerMeter;  ///< 兆牛每米

        static const Quantity Pascal;     ///< 帕斯卡
        static const Quantity KiloPascal; ///< 千帕
        static const Quantity MegaPascal; ///< 兆帕
        static const Quantity GigaPascal; ///< 吉帕

        static const Quantity Bar;      ///< 巴
        static const Quantity MilliBar; ///< 毫巴

        static const Quantity Torr;  ///< 托，1/760 标准大气压
        static const Quantity mTorr; ///< 毫托
        static const Quantity yTorr; ///< 微托（沿用量级的 y 前缀写法）

        static const Quantity PSI;  ///< 磅力每平方英寸
        static const Quantity KSI;  ///< 千磅力每平方英寸
        static const Quantity MPSI; ///< 兆磅力每平方英寸

        static const Quantity Watt;       ///< 瓦
        static const Quantity NanoWatt;   ///< 纳瓦
        static const Quantity MicroWatt;  ///< 微瓦
        static const Quantity MilliWatt;  ///< 毫瓦
        static const Quantity KiloWatt;   ///< 千瓦
        static const Quantity VoltAmpere; ///< 伏安

        static const Quantity Volt;      ///< 伏
        static const Quantity MilliVolt; ///< 毫伏
        static const Quantity KiloVolt;  ///< 千伏

        static const Quantity MegaSiemens;  ///< 兆西门子
        static const Quantity KiloSiemens;  ///< 千西门子
        static const Quantity Siemens;      ///< 西门子
        static const Quantity MilliSiemens; ///< 毫西门子
        static const Quantity MicroSiemens; ///< 微西门子

        static const Quantity Ohm;     ///< 欧姆
        static const Quantity KiloOhm; ///< 千欧
        static const Quantity MegaOhm; ///< 兆欧

        static const Quantity Coulomb; ///< 库仑

        static const Quantity Tesla;      ///< 特斯拉
        static const Quantity MilliTesla; ///< 毫特斯拉
        static const Quantity Gauss;      ///< 高斯

        static const Quantity Weber; ///< 韦伯

        static const Quantity Farad;      ///< 法拉
        static const Quantity MilliFarad; ///< 毫法
        static const Quantity MicroFarad; ///< 微法
        static const Quantity NanoFarad;  ///< 纳法
        static const Quantity PicoFarad;  ///< 皮法

        static const Quantity Henry;      ///< 亨利
        static const Quantity MilliHenry; ///< 毫亨
        static const Quantity MicroHenry; ///< 微亨
        static const Quantity NanoHenry;  ///< 纳亨

        static const Quantity Joule;            ///< 焦耳
        static const Quantity MilliJoule;       ///< 毫焦
        static const Quantity KiloJoule;        ///< 千焦
        static const Quantity NewtonMeter;      ///< 牛米
        static const Quantity VoltAmpereSecond; ///< 伏安秒
        static const Quantity WattSecond;       ///< 瓦秒
        static const Quantity KiloWattHour;     ///< 千瓦时
        static const Quantity ElectronVolt;     ///< 电子伏
        static const Quantity KiloElectronVolt; ///< 千电子伏
        static const Quantity MegaElectronVolt; ///< 兆电子伏
        static const Quantity Calorie;          ///< 卡
        static const Quantity KiloCalorie;      ///< 千卡

        static const Quantity KMH; ///< 千米每小时
        static const Quantity MPH; ///< 英里每小时

        static const Quantity Degree;      ///< 度
        static const Quantity Radian;      ///< 弧度
        static const Quantity Gon;         ///< 百分度，整圆为 400 度
        static const Quantity AngleMinute; ///< 角分
        static const Quantity AngleSecond; ///< 角秒
        //@}

    private:
        double         m_value;  ///< 数值，以基准量纲表示
        Unit           m_unit;   ///< 单位（量纲）
        QuantityFormat m_format; ///< 显示格式
    };
} // namespace ExpressionEngine::Units
