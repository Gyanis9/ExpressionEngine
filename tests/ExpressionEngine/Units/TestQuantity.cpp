// 本文件覆盖 Quantity 的运算契约、格式排版、单位不匹配拒绝面，以及预定义常量表的换算关系。

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/NumericFormatting.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/UnitsApi.h>
#include <ExpressionEngine/Units/UnitsSchema.h>

namespace ExpressionEngine::Units
{
    namespace
    {

        /**
         * @brief 钉住：数值以基准量纲表示，同一物理量在不同单位下的数值不同
         */
        TEST(QuantityTest, PredefinedQuantitiesUseBaseUnits)
        {
            EXPECT_DOUBLE_EQ(Quantity::MilliMetre.getValue(), 1.0);
            EXPECT_DOUBLE_EQ(Quantity::CentiMetre.getValue(), 10.0);
            EXPECT_DOUBLE_EQ(Quantity::Metre.getValue(), 1000.0);
            EXPECT_DOUBLE_EQ(Quantity::Inch.getValue(), 25.4);
            EXPECT_DOUBLE_EQ(Quantity::Foot.getValue(), 304.8);
            EXPECT_EQ(Quantity::Metre.getUnit(), Unit::Length);
        }

        /**
         * @brief 比对两个量：量纲指数必须相同，数值按相对容差判等
         * @details 用恒等式而不是抄数值来查常量表，不同乘法路径上的末位差异不该算失败。
         *          量纲按指数向量比，不比类型名——Work 与 Moment 同量纲不同名，乘出来的单位
         *          没有显式名字，反查到的类型名与命名常量不同是设计如此。
         */
        void expectEquivalent(const Quantity &actual, const Quantity &expected, const std::string &label)
        {
            EXPECT_TRUE(actual.getUnit() == expected.getUnit()) << label;
            EXPECT_NEAR(actual.getValue(), expected.getValue(), std::abs(expected.getValue()) * 1e-12) << label;
        }

        /**
         * @brief 钉住：导出量等于各基本量按物理定义的组合
         * @details 预定义量有 119 项，宿主按名字直接取用，表里一个指数写错就是静默错答；
         *          这里按定义复核，不重复抄一遍实现里的数值。
         */
        TEST(QuantityTest, DerivedConstantsMatchTheirDefinitions)
        {
            expectEquivalent(Quantity::Newton, Quantity::KiloGram * Quantity::Metre / (Quantity::Second * Quantity::Second), "牛顿");
            expectEquivalent(Quantity::Pascal, Quantity::Newton / (Quantity::Metre * Quantity::Metre), "帕斯卡");
            expectEquivalent(Quantity::Joule, Quantity::Newton * Quantity::Metre, "焦耳");
            expectEquivalent(Quantity::Watt, Quantity::Joule / Quantity::Second, "瓦特");
            expectEquivalent(Quantity::Coulomb, Quantity::Ampere * Quantity::Second, "库仑");
            expectEquivalent(Quantity::Volt, Quantity::Watt / Quantity::Ampere, "伏特");
            expectEquivalent(Quantity::Ohm, Quantity::Volt / Quantity::Ampere, "欧姆");
            expectEquivalent(Quantity::Siemens, Quantity::Ampere / Quantity::Volt, "西门子");
            expectEquivalent(Quantity::Farad, Quantity::Coulomb / Quantity::Volt, "法拉");
            expectEquivalent(Quantity::Henry, Quantity::Volt * Quantity::Second / Quantity::Ampere, "亨利");
            expectEquivalent(Quantity::Weber, Quantity::Volt * Quantity::Second, "韦伯");
            expectEquivalent(Quantity::Tesla, Quantity::Weber / (Quantity::Metre * Quantity::Metre), "特斯拉");
            expectEquivalent(Quantity::Gauss, Quantity::Tesla / 1e4, "高斯");

            // 牛·米作为力矩与作为能量量纲不同，数值相同
            EXPECT_EQ(Quantity::NewtonMeter.getUnit().getTypeString(), "Moment");
            EXPECT_DOUBLE_EQ(Quantity::NewtonMeter.getValue(), Quantity::Joule.getValue());

            // 国际码磅与标准重力加速度
            expectEquivalent(Quantity::Inch, Quantity::MilliMetre * 25.4, "英寸");
            expectEquivalent(Quantity::Foot, Quantity::Inch * 12.0, "英尺");
            expectEquivalent(Quantity::Yard, Quantity::Foot * 3.0, "码");
            expectEquivalent(Quantity::Mile, Quantity::Yard * 1760.0, "英里");
            expectEquivalent(Quantity::Thou, Quantity::Inch / 1000.0, "mil");
            expectEquivalent(Quantity::SquareFoot, Quantity::Foot * Quantity::Foot, "平方英尺");
            expectEquivalent(Quantity::CubicFoot, Quantity::Foot * Quantity::Foot * Quantity::Foot, "立方英尺");
            expectEquivalent(Quantity::Pound, Quantity::KiloGram * 0.45359237, "磅");
            expectEquivalent(Quantity::Ounce, Quantity::Pound / 16.0, "盎司");
            expectEquivalent(Quantity::Stone, Quantity::Pound * 14.0, "斯通");
            expectEquivalent(Quantity::Hundredweights, Quantity::Pound * 112.0, "长百磅");
            expectEquivalent(Quantity::PoundForce, Quantity::Pound * (Quantity::Metre * 9.80665 / (Quantity::Second * Quantity::Second)), "磅力");
            expectEquivalent(Quantity::PSI, Quantity::PoundForce / (Quantity::Inch * Quantity::Inch), "磅力每平方英寸");

            expectEquivalent(Quantity::Bar, Quantity::Pascal * 1e5, "巴");
            expectEquivalent(Quantity::Torr, Quantity::Bar * (1.01325 / 760.0), "托");
            expectEquivalent(Quantity::KiloWattHour, Quantity::Joule * 3.6e6, "千瓦时");
            expectEquivalent(Quantity::ElectronVolt, Quantity::Joule * 1.602176634e-19, "电子伏");
            expectEquivalent(Quantity::Calorie, Quantity::Joule * 4.1868, "热化学卡");
            expectEquivalent(Quantity::KiloCalorie, Quantity::Joule * 4186.8, "千卡");

            expectEquivalent(Quantity::KMH, Quantity::KiloMetre / Quantity::Hour, "公里每小时");
            expectEquivalent(Quantity::MPH, Quantity::Mile / Quantity::Hour, "英里每小时");
            expectEquivalent(Quantity::MilePerHour, Quantity::MPH, "英里每小时别名");

            // 角度以度为基准
            expectEquivalent(Quantity::AngleMinute, Quantity::Degree / 60.0, "角分");
            expectEquivalent(Quantity::AngleSecond, Quantity::Degree / 3600.0, "角秒");
            expectEquivalent(Quantity::Radian, Quantity::Degree * (180.0 / std::numbers::pi), "弧度");
            expectEquivalent(Quantity::Gon, Quantity::Degree * 0.9, "百分度");
        }

        /**
         * @brief 钉住：同一量纲的前缀族按 10 的幂递推
         */
        TEST(QuantityTest, PrefixedConstantsFollowSIPrefixPowers)
        {
            struct Row
            {
                std::string    name;      ///< 报错时指出是哪一项
                const Quantity *prefixed; ///< 带前缀的量
                const Quantity *base;     ///< 同量纲的基准量
                double          ratio;    ///< 前缀倍数
            };

            const std::vector<Row> rows{
                    {"NanoMetre", &Quantity::NanoMetre, &Quantity::Metre, 1e-9},
                    {"MicroMetre", &Quantity::MicroMetre, &Quantity::Metre, 1e-6},
                    {"MilliMetre", &Quantity::MilliMetre, &Quantity::Metre, 1e-3},
                    {"CentiMetre", &Quantity::CentiMetre, &Quantity::Metre, 1e-2},
                    {"DeciMetre", &Quantity::DeciMetre, &Quantity::Metre, 1e-1},
                    {"KiloMetre", &Quantity::KiloMetre, &Quantity::Metre, 1e3},
                    {"Liter", &Quantity::Liter, &Quantity::MilliLiter, 1e3},
                    {"MicroGram", &Quantity::MicroGram, &Quantity::Gram, 1e-6},
                    {"MilliGram", &Quantity::MilliGram, &Quantity::Gram, 1e-3},
                    {"Gram", &Quantity::Gram, &Quantity::KiloGram, 1e-3},
                    {"Ton", &Quantity::Ton, &Quantity::KiloGram, 1e3},
                    {"NanoAmpere", &Quantity::NanoAmpere, &Quantity::Ampere, 1e-9},
                    {"MicroAmpere", &Quantity::MicroAmpere, &Quantity::Ampere, 1e-6},
                    {"MilliAmpere", &Quantity::MilliAmpere, &Quantity::Ampere, 1e-3},
                    {"KiloAmpere", &Quantity::KiloAmpere, &Quantity::Ampere, 1e3},
                    {"MegaAmpere", &Quantity::MegaAmpere, &Quantity::Ampere, 1e6},
                    {"MilliKelvin", &Quantity::MilliKelvin, &Quantity::Kelvin, 1e-3},
                    {"MicroKelvin", &Quantity::MicroKelvin, &Quantity::Kelvin, 1e-6},
                    {"NanoMole", &Quantity::NanoMole, &Quantity::Mole, 1e-9},
                    {"MicroMole", &Quantity::MicroMole, &Quantity::Mole, 1e-6},
                    {"MilliMole", &Quantity::MilliMole, &Quantity::Mole, 1e-3},
                    {"KiloHertz", &Quantity::KiloHertz, &Quantity::Hertz, 1e3},
                    {"MegaHertz", &Quantity::MegaHertz, &Quantity::Hertz, 1e6},
                    {"GigaHertz", &Quantity::GigaHertz, &Quantity::Hertz, 1e9},
                    {"TeraHertz", &Quantity::TeraHertz, &Quantity::Hertz, 1e12},
                    {"MilliNewton", &Quantity::MilliNewton, &Quantity::Newton, 1e-3},
                    {"KiloNewton", &Quantity::KiloNewton, &Quantity::Newton, 1e3},
                    {"MegaNewton", &Quantity::MegaNewton, &Quantity::Newton, 1e6},
                    {"MilliNewtonPerMeter", &Quantity::MilliNewtonPerMeter, &Quantity::NewtonPerMeter, 1e-3},
                    {"KiloNewtonPerMeter", &Quantity::KiloNewtonPerMeter, &Quantity::NewtonPerMeter, 1e3},
                    {"MegaNewtonPerMeter", &Quantity::MegaNewtonPerMeter, &Quantity::NewtonPerMeter, 1e6},
                    {"KiloPascal", &Quantity::KiloPascal, &Quantity::Pascal, 1e3},
                    {"MegaPascal", &Quantity::MegaPascal, &Quantity::Pascal, 1e6},
                    {"GigaPascal", &Quantity::GigaPascal, &Quantity::Pascal, 1e9},
                    {"MilliBar", &Quantity::MilliBar, &Quantity::Bar, 1e-3},
                    {"mTorr", &Quantity::mTorr, &Quantity::Torr, 1e-3},
                    {"uTorr", &Quantity::yTorr, &Quantity::Torr, 1e-6},
                    {"KSI", &Quantity::KSI, &Quantity::PSI, 1e3},
                    {"MPSI", &Quantity::MPSI, &Quantity::PSI, 1e6},
                    {"NanoWatt", &Quantity::NanoWatt, &Quantity::Watt, 1e-9},
                    {"MicroWatt", &Quantity::MicroWatt, &Quantity::Watt, 1e-6},
                    {"MilliWatt", &Quantity::MilliWatt, &Quantity::Watt, 1e-3},
                    {"KiloWatt", &Quantity::KiloWatt, &Quantity::Watt, 1e3},
                    {"VoltAmpere", &Quantity::VoltAmpere, &Quantity::Watt, 1.0},
                    {"MilliVolt", &Quantity::MilliVolt, &Quantity::Volt, 1e-3},
                    {"KiloVolt", &Quantity::KiloVolt, &Quantity::Volt, 1e3},
                    {"MilliSiemens", &Quantity::MilliSiemens, &Quantity::Siemens, 1e-3},
                    {"MicroSiemens", &Quantity::MicroSiemens, &Quantity::Siemens, 1e-6},
                    {"KiloSiemens", &Quantity::KiloSiemens, &Quantity::Siemens, 1e3},
                    {"MegaSiemens", &Quantity::MegaSiemens, &Quantity::Siemens, 1e6},
                    {"KiloOhm", &Quantity::KiloOhm, &Quantity::Ohm, 1e3},
                    {"MegaOhm", &Quantity::MegaOhm, &Quantity::Ohm, 1e6},
                    {"PicoFarad", &Quantity::PicoFarad, &Quantity::Farad, 1e-12},
                    {"NanoFarad", &Quantity::NanoFarad, &Quantity::Farad, 1e-9},
                    {"MicroFarad", &Quantity::MicroFarad, &Quantity::Farad, 1e-6},
                    {"MilliFarad", &Quantity::MilliFarad, &Quantity::Farad, 1e-3},
                    {"NanoHenry", &Quantity::NanoHenry, &Quantity::Henry, 1e-9},
                    {"MicroHenry", &Quantity::MicroHenry, &Quantity::Henry, 1e-6},
                    {"MilliHenry", &Quantity::MilliHenry, &Quantity::Henry, 1e-3},
                    {"MilliJoule", &Quantity::MilliJoule, &Quantity::Joule, 1e-3},
                    {"KiloJoule", &Quantity::KiloJoule, &Quantity::Joule, 1e3},
                    {"KiloElectronVolt", &Quantity::KiloElectronVolt, &Quantity::ElectronVolt, 1e3},
                    {"MegaElectronVolt", &Quantity::MegaElectronVolt, &Quantity::ElectronVolt, 1e6},
                    {"MilliTesla", &Quantity::MilliTesla, &Quantity::Tesla, 1e-3},
            };

            for (const auto &row: rows)
            {
                expectEquivalent(*row.prefixed, (*row.base) * row.ratio, row.name);
            }
        }

        /**
         * @brief 钉住：加减与比较要求单位一致，不一致时报错而不是按数值硬算
         */
        TEST(QuantityTest, AdditionRequiresMatchingUnits)
        {
            const Quantity oneMetre      = Quantity::Metre;
            const Quantity oneMillimetre = Quantity::MilliMetre;
            EXPECT_DOUBLE_EQ((oneMetre + oneMillimetre).getValue(), 1001.0);

            const Quantity oneSecond = Quantity::Second;
            EXPECT_THROW(static_cast<void>(oneMetre + oneSecond), Base::UnitsMismatchError);
            EXPECT_THROW(static_cast<void>(oneMetre - oneSecond), Base::UnitsMismatchError);
            EXPECT_THROW(static_cast<void>(oneMetre < oneSecond), Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：乘除只合并量纲，幂次同时作用于数值与量纲
         */
        TEST(QuantityTest, MultiplicationDividesUnitsAndPowers)
        {
            const Quantity area = Quantity::Metre * Quantity::Metre;
            EXPECT_EQ(area.getUnit(), Unit::Area);
            EXPECT_DOUBLE_EQ(area.getValue(), 1.0e6);

            const Quantity squared = Quantity(2.0, Unit::Length).pow(2.0);
            EXPECT_EQ(squared.getUnit(), Unit::Area);
            EXPECT_DOUBLE_EQ(squared.getValue(), 4.0);

            // 幂次带单位属于用法错误
            EXPECT_THROW(static_cast<void>(Quantity(2.0).pow(Quantity(1.0, Unit::Length))), Base::UnitsMismatchError);
        }

        /**
         * @brief 钉住：无效值用 NaN 表示，可被 isValid() 识别
         */
        TEST(QuantityTest, InvalidValueRoundTrip)
        {
            Quantity value{1.0};
            EXPECT_TRUE(value.isValid());

            value.setInvalid();
            EXPECT_FALSE(value.isValid());
            EXPECT_TRUE(std::isnan(value.getValue()));
        }

        /**
         * @brief 钉住：toString 带引号便于回填，toNumber 只给数值
         */
        TEST(QuantityTest, FormattingKeepsUnitAndQuotes)
        {
            const Quantity value{25.4, Unit::Length};
            EXPECT_EQ(value.toNumber(QuantityFormat(QuantityFormat::NumberFormat::Fixed, 1)), "25.4");
            EXPECT_EQ(value.toString(QuantityFormat(QuantityFormat::NumberFormat::Fixed, 1)), "'25.4 mm'");
        }

        /**
         * @brief 钉住：单位文本构造走解析器，无法解析时退化为无量纲且数值归零
         */
        TEST(QuantityTest, ConstructFromUnitText)
        {
            const Quantity value{2.0, std::string("in")};
            EXPECT_DOUBLE_EQ(value.getValue(), 50.8);
            EXPECT_EQ(value.getUnit(), Unit::Length);

            // 非法单位文本不抛异常，但会退化成无量纲的零值
            const Quantity broken{2.0, std::string("not-a-unit")};
            EXPECT_DOUBLE_EQ(broken.getValue(), 0.0);
            EXPECT_EQ(broken.getUnit(), Unit::One);
        }

        /**
         * @brief 钉住：无量纲判定只看量纲是否为单位一
         */
        TEST(QuantityTest, DimensionlessDetection)
        {
            EXPECT_TRUE(Quantity(2.0).isDimensionless());
            EXPECT_FALSE(Quantity(2.0, Unit::Length).isDimensionless());
            EXPECT_TRUE(Quantity(2.0, Unit::Length).isDimensionlessOrUnit(Unit::Length));
        }

        /**
         * @brief 钉住：getUserString 按当前单位方案换算，输出参数给出实际用的因子与单位串
         */
        TEST(QuantityTest, UserStringFollowsCurrentSchema)
        {
            UnitsApi::setSchema("Internal");
            UnitsApi::setDecimals(2);

            double      factor = 0.0;
            std::string unitString;
            EXPECT_EQ(Quantity::Metre.getUserString(factor, unitString), "1000.00 mm");
            EXPECT_DOUBLE_EQ(factor, 1.0);
            EXPECT_EQ(unitString, "mm");

            // 换方案只改排版与目标单位，量本身的数值不动；Imperial 把 1000 mm 折算成码
            UnitsApi::setSchema("Imperial");
            EXPECT_EQ(Quantity::Metre.getUserString(factor, unitString), "1.09 yd");
            EXPECT_DOUBLE_EQ(factor, 914.4);
            EXPECT_EQ(unitString, "yd");
            EXPECT_DOUBLE_EQ(Quantity::Metre.getValue(), 1000.0);

            // 指定方案的重载不读全局状态，宿主可以并行渲染多种排版
            const std::unique_ptr<UnitsSchema> internal = UnitsApi::createSchema(0);
            EXPECT_EQ(Quantity::Metre.getUserString(internal.get(), factor, unitString), "1000.00 mm");
            EXPECT_EQ(unitString, "mm");
        }

        /**
         * @brief 钉住：getSafeUserString 的回落面与英寸符号转义
         */
        TEST(QuantityTest, SafeUserStringFallsBackWhenTextWouldLoseValue)
        {
            UnitsApi::setSchema("Internal");
            UnitsApi::setDecimals(2);

            // 方案会自己挑合适的前缀，100 微米不会被四舍五入成 0，也就无需回落
            const Quantity tiny{0.0001, Unit::Length};
            EXPECT_EQ(tiny.getUserString(), "100.00 nm");
            EXPECT_EQ(tiny.getSafeUserString(), "100.00 nm");

            // 小到方案排版不出非零值的量会被写成 0.00，此时必须回落到原值写法，
            // 否则回填表达式就把值整个丢了
            const Quantity femto{1.0e-12, Unit::Length};
            EXPECT_EQ(femto.getUserString(), "0.00 mm");
            EXPECT_EQ(femto.getSafeUserString(), "1e-12 mm");

            // 真值本来就是 0，不需要回落，照常按方案排版
            const Quantity zero{0.0, Unit::Length};
            EXPECT_EQ(zero.getUserString(), "0.00 mm");
            EXPECT_EQ(zero.getSafeUserString(), "0.00 mm");

            // 英寸符号要转义，回填到带引号的表达式文本里才不会截断
            UnitsApi::setSchema("Imperial");
            EXPECT_EQ(Quantity::Inch.getSafeUserString(), "1.00\\\"");
        }

        /**
         * @brief 钉住：getValueAs 是同量纲之间的换算比，量纲不符报错、参照为零给无穷大
         */
        TEST(QuantityTest, ValueAsRatio)
        {
            EXPECT_DOUBLE_EQ(Quantity::Metre.getValueAs(Quantity::MilliMetre), 1000.0);
            EXPECT_DOUBLE_EQ(Quantity::Inch.getValueAs(Quantity::MilliMetre), 25.4);
            EXPECT_DOUBLE_EQ(Quantity::Metre.getValueAs(Quantity::Inch), 1000.0 / 25.4);

            // 量纲不同就没有「相对于」可言，报错而不是给个像模像样的数
            EXPECT_THROW(static_cast<void>(Quantity::Metre.getValueAs(Quantity(1.0, Unit::Mass))), Base::UnitsMismatchError);
            // 除零与 operator/ 同口径：交给 IEEE 的无穷大
            EXPECT_TRUE(std::isinf(Quantity::Metre.getValueAs(Quantity(0.0, Unit::Length))));
        }

        /**
         * @brief 钉住：格式设置的读写、全局默认回落与 printf 风格字符互转
         */
        TEST(QuantityTest, FormatSettingsRoundTrip)
        {
            UnitsApi::setDecimals(3);
            Quantity value{25.4, Unit::Length};

            // 默认构造就是定点记数法，只有精度与分母在未显式设置时跟全局默认
            EXPECT_EQ(value.getFormat().getPrecision(), 3);
            EXPECT_EQ(value.getFormat().getDenominator(), UnitsApi::getDenominator());
            EXPECT_EQ(value.getFormat().toFormat(), 'f');
            EXPECT_EQ(QuantityFormat(QuantityFormat::NumberFormat::Default).toFormat(), 'g');

            const QuantityFormat fixed(QuantityFormat::NumberFormat::Fixed, 5);
            value.setFormat(fixed);
            EXPECT_EQ(value.getFormat().getPrecision(), 5);
            EXPECT_EQ(value.toNumber(value.getFormat()), "25.40000");

            bool notationIsValid = false;
            EXPECT_EQ(QuantityFormat::toFormat('e', &notationIsValid), QuantityFormat::NumberFormat::Scientific);
            EXPECT_TRUE(notationIsValid);
            EXPECT_EQ(QuantityFormat::toFormat('x', &notationIsValid), QuantityFormat::NumberFormat::Default);
            EXPECT_FALSE(notationIsValid);
            // 传 nullptr 也要能用，调用方不关心字符是否被识别时不必自备输出位
            EXPECT_NO_THROW(static_cast<void>(QuantityFormat::toFormat('x', nullptr)));
        }

        /**
         * @brief 钉住：parseUserInput 先按区域分隔符规范化，再交给 parse()
         */
        TEST(QuantityTest, ParseUserInputHonoursLocale)
        {
            const auto german = Base::createNumericLocaleContext("de_DE");
            ASSERT_TRUE(german.has_value());
            // 德语用逗号作小数点
            EXPECT_DOUBLE_EQ(Quantity::parseUserInput("1,5 mm", *german).getValue(), 1.5);

            const auto english = Base::createNumericLocaleContext("en_US");
            ASSERT_TRUE(english.has_value());
            EXPECT_DOUBLE_EQ(Quantity::parseUserInput("1.5 mm", *english).getValue(), 1.5);
            // 英语里逗号是分组分隔符，"1,5" 不成组就是非法输入
            EXPECT_THROW(static_cast<void>(Quantity::parseUserInput("1,5 mm", *english)), Base::ParserError);
            EXPECT_DOUBLE_EQ(Quantity::parseUserInput("1,000 mm", *english).getValue(), 1000.0);
        }

    } // namespace
}     // namespace ExpressionEngine::Units
