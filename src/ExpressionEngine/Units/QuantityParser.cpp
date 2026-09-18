#include <ExpressionEngine/Units/QuantityParser.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <numbers>
#include <string_view>

#include <ExpressionEngine/Base/Exception.h>

namespace ExpressionEngine::Units
{
    namespace
    {
        /// 判断是否为 ASCII 十进制数字
        constexpr bool isDecimalDigit(const char character)
        {
            return character >= '0' && character <= '9';
        }

        /// 判断是否为数量文本里的空白字符
        constexpr bool isInsignificantWhitespace(const char character)
        {
            return character == ' ' || character == '\t' || character == '\n' || character == '\r';
        }

        /// 热路径上的字符类别位图：把逐字节谓词在编译期铺成 256 项查表，扫描不再走判定函数链
        struct CharacterClassBitmaps
        {
            std::array<std::uint8_t, 256> decimalDigit{}; ///< 十进制数字
            std::array<std::uint8_t, 256> whitespace{};   ///< 空白：空格、制表符、换行、回车
        };

        /// 在编译期把字符类别谓词铺成位图，下标是字节值
        constexpr CharacterClassBitmaps buildCharacterClassBitmaps()
        {
            CharacterClassBitmaps bitmaps;
            for (std::size_t index = 0; index < bitmaps.decimalDigit.size(); ++index)
            {
                const char character        = static_cast<char>(static_cast<unsigned char>(index));
                bitmaps.decimalDigit[index] = isDecimalDigit(character) ? 1 : 0;
                bitmaps.whitespace[index]   = isInsignificantWhitespace(character) ? 1 : 0;
            }
            return bitmaps;
        }

        /// 编译期建好的字符类别位图；静态存储期且无运行时初始化
        constexpr CharacterClassBitmaps characterClasses = buildCharacterClassBitmaps();

        /// 记号类别
        enum class TokenKind
        {
            Number,     ///< 十进制数值
            Unit,       ///< 单位符号
            Function,   ///< 标量函数名
            Plus,       ///< '+'
            Minus,      ///< '-' 或 Unicode 减号
            Star,       ///< '*'
            Slash,      ///< '/'
            Caret,      ///< '^'
            LeftParen,  ///< '('
            RightParen, ///< ')'
            End         ///< 输入结束
        };

        /// 支持的标量函数；取值名即数学函数的通用写法
        enum class FunctionId
        {
            Acos,  ///< acos：反余弦
            Asin,  ///< asin：反正弦
            Atan,  ///< atan：反正切
            Cos,   ///< cos：余弦
            Exp,   ///< exp：自然指数
            Abs,   ///< abs：绝对值
            Log,   ///< log：自然对数
            Log10, ///< log10：常用对数
            Sin,   ///< sin：正弦
            Sinh,  ///< sinh：双曲正弦
            Tan,   ///< tan：正切
            Tanh,  ///< tanh：双曲正切
            Sqrt   ///< sqrt：平方根
        };

        /// 一个记号；数值与单位分开存放——数值直接带 double，单位只带指向静态预定义量的指针，
        /// 免得每个记号都构造一份完整 Quantity（含 Unit 与 QuantityFormat）。
        /// 需要完整量的地方由消费方现造，记号本身是可平凡拷贝的小对象。
        struct Token
        {
            TokenKind       kind{TokenKind::End};  ///< 记号类别
            FunctionId      function{};            ///< 函数记号对应的函数
            std::size_t     offset{0};             ///< 记号在输入中的字节偏移，用于报错定位
            double          numberValue{0.0};      ///< 数值记号的数值
            const Quantity *unitQuantity{nullptr}; ///< 单位记号对应的预定义量；静态存储期，不持所有权
        };

        /// 单位符号到预定义量的对照
        struct UnitTokenSpec
        {
            std::string_view symbol;   ///< 单位符号原文，如 "mm"、"µm"
            const Quantity  *quantity; ///< 该符号对应的预定义量（静态存储期，不持所有权）
        };

        /// 单位符号对照表；等长符号之间的先后决定匹配优先级，由 longestMatch 取最长匹配
        // clang-format off
constexpr std::array unitTokenSpecs {
    UnitTokenSpec { "nm"  , &Quantity::NanoMetre    }, UnitTokenSpec { "um"   , &Quantity::MicroMetre   },
    UnitTokenSpec { "µm"  , &Quantity::MicroMetre   }, UnitTokenSpec { "mm"   , &Quantity::MilliMetre   },
    UnitTokenSpec { "cm"  , &Quantity::CentiMetre   }, UnitTokenSpec { "dm"   , &Quantity::DeciMetre    },
    UnitTokenSpec { "m"   , &Quantity::Metre        }, UnitTokenSpec { "km"   , &Quantity::KiloMetre    },

    UnitTokenSpec { "l"   , &Quantity::Liter        }, UnitTokenSpec { "ml"   , &Quantity::MilliLiter   },

    UnitTokenSpec { "Hz"  , &Quantity::Hertz        }, UnitTokenSpec { "kHz"  , &Quantity::KiloHertz    },
    UnitTokenSpec { "MHz" , &Quantity::MegaHertz    }, UnitTokenSpec { "GHz"  , &Quantity::GigaHertz    },
    UnitTokenSpec { "THz" , &Quantity::TeraHertz    },

    UnitTokenSpec { "ug"  , &Quantity::MicroGram    }, UnitTokenSpec { "µg"   , &Quantity::MicroGram    },
    UnitTokenSpec { "mg"  , &Quantity::MilliGram    }, UnitTokenSpec { "g"    , &Quantity::Gram         },
    UnitTokenSpec { "kg"  , &Quantity::KiloGram     }, UnitTokenSpec { "t"    , &Quantity::Ton          },

    UnitTokenSpec { "s"   , &Quantity::Second       }, UnitTokenSpec { "min"  , &Quantity::Minute       },
    UnitTokenSpec { "h"   , &Quantity::Hour         },

    UnitTokenSpec { "A"   , &Quantity::Ampere       }, UnitTokenSpec { "nA"   , &Quantity::NanoAmpere   },
    UnitTokenSpec { "uA"  , &Quantity::MicroAmpere  }, UnitTokenSpec { "µA"   , &Quantity::MicroAmpere  },
    UnitTokenSpec { "mA"  , &Quantity::MilliAmpere  }, UnitTokenSpec { "kA"   , &Quantity::KiloAmpere   },
    UnitTokenSpec { "MA"  , &Quantity::MegaAmpere   },

    UnitTokenSpec { "K"   , &Quantity::Kelvin       }, UnitTokenSpec { "mK"   , &Quantity::MilliKelvin  },
    UnitTokenSpec { "uK"  , &Quantity::MicroKelvin  }, UnitTokenSpec { "µK"   , &Quantity::MicroKelvin  },

    UnitTokenSpec { "mol" , &Quantity::Mole         }, UnitTokenSpec { "nmol" , &Quantity::NanoMole     },
    UnitTokenSpec { "umol", &Quantity::MicroMole    }, UnitTokenSpec { "µmol" , &Quantity::MicroMole    },
    UnitTokenSpec { "mmol", &Quantity::MilliMole    },

    UnitTokenSpec { "cd"  , &Quantity::Candela      },

    UnitTokenSpec { "in"  , &Quantity::Inch         }, UnitTokenSpec { "\"",    &Quantity::Inch        },
    UnitTokenSpec { "ft"  , &Quantity::Foot         }, UnitTokenSpec { "'",     &Quantity::Foot        },
    UnitTokenSpec { "thou", &Quantity::Thou         }, UnitTokenSpec { "mil"  , &Quantity::Thou        },
    UnitTokenSpec { "yd"  , &Quantity::Yard         }, UnitTokenSpec { "mi"   , &Quantity::Mile        },

    UnitTokenSpec { "mph" , &Quantity::MilePerHour  }, UnitTokenSpec { "sqft" , &Quantity::SquareFoot  },
    UnitTokenSpec { "cft" , &Quantity::CubicFoot    },

    UnitTokenSpec { "lb"  , &Quantity::Pound        }, UnitTokenSpec { "lbm"  , &Quantity::Pound       },
    UnitTokenSpec { "oz"  , &Quantity::Ounce        }, UnitTokenSpec { "st"   , &Quantity::Stone       },
    UnitTokenSpec { "cwt" , &Quantity::Hundredweights },

    UnitTokenSpec { "lbf" , &Quantity::PoundForce   },

    UnitTokenSpec { "N"   , &Quantity::Newton       }, UnitTokenSpec { "mN"   , &Quantity::MilliNewton },
    UnitTokenSpec { "kN"  , &Quantity::KiloNewton   }, UnitTokenSpec { "MN"   , &Quantity::MegaNewton  },

    UnitTokenSpec { "Pa"  , &Quantity::Pascal       }, UnitTokenSpec { "kPa"  , &Quantity::KiloPascal  },
    UnitTokenSpec { "MPa" , &Quantity::MegaPascal   }, UnitTokenSpec { "GPa"  , &Quantity::GigaPascal  },

    UnitTokenSpec { "bar" , &Quantity::Bar          }, UnitTokenSpec { "mbar" , &Quantity::MilliBar    },

    UnitTokenSpec { "Torr", &Quantity::Torr         }, UnitTokenSpec { "mTorr", &Quantity::mTorr       },
    UnitTokenSpec { "uTorr", &Quantity::yTorr       }, UnitTokenSpec { "µTorr", &Quantity::yTorr       },

    UnitTokenSpec { "psi" , &Quantity::PSI          }, UnitTokenSpec { "ksi"  , &Quantity::KSI         },
    UnitTokenSpec { "Mpsi", &Quantity::MPSI         },

    UnitTokenSpec { "W"   , &Quantity::Watt         }, UnitTokenSpec { "nW"   , &Quantity::NanoWatt    },
    UnitTokenSpec { "uW"  , &Quantity::MicroWatt    }, UnitTokenSpec { "µW"   , &Quantity::MicroWatt   },
    UnitTokenSpec { "mW"  , &Quantity::MilliWatt    }, UnitTokenSpec { "kW"   , &Quantity::KiloWatt    },
    UnitTokenSpec { "VA"  , &Quantity::VoltAmpere   },

    UnitTokenSpec { "V"   , &Quantity::Volt         }, UnitTokenSpec { "kV"   , &Quantity::KiloVolt    },
    UnitTokenSpec { "mV"  , &Quantity::MilliVolt    },

    UnitTokenSpec { "MS"  , &Quantity::MegaSiemens  }, UnitTokenSpec { "kS"   , &Quantity::KiloSiemens },
    UnitTokenSpec { "S"   , &Quantity::Siemens      }, UnitTokenSpec { "mS"   , &Quantity::MilliSiemens },
    UnitTokenSpec { "uS"  , &Quantity::MicroSiemens }, UnitTokenSpec { "µS"   , &Quantity::MicroSiemens },

    UnitTokenSpec { "Ohm" , &Quantity::Ohm          }, UnitTokenSpec { "kOhm" , &Quantity::KiloOhm     },
    UnitTokenSpec { "MOhm", &Quantity::MegaOhm      },

    UnitTokenSpec { "C"   , &Quantity::Coulomb      },

    UnitTokenSpec { "T"   , &Quantity::Tesla        }, UnitTokenSpec { "mT"   , &Quantity::MilliTesla  },
    UnitTokenSpec { "G"   , &Quantity::Gauss        },

    UnitTokenSpec { "Wb"  , &Quantity::Weber        },

    UnitTokenSpec { "F"   , &Quantity::Farad        }, UnitTokenSpec { "mF"   , &Quantity::MilliFarad  },
    UnitTokenSpec { "uF"  , &Quantity::MicroFarad   }, UnitTokenSpec { "µF"   , &Quantity::MicroFarad  },
    UnitTokenSpec { "nF"  , &Quantity::NanoFarad    }, UnitTokenSpec { "pF"   , &Quantity::PicoFarad   },

    UnitTokenSpec { "H"   , &Quantity::Henry        }, UnitTokenSpec { "mH"   , &Quantity::MilliHenry  },
    UnitTokenSpec { "uH"  , &Quantity::MicroHenry   }, UnitTokenSpec { "µH"   , &Quantity::MicroHenry  },
    UnitTokenSpec { "nH"  , &Quantity::NanoHenry    },

    UnitTokenSpec { "J"   , &Quantity::Joule        }, UnitTokenSpec { "mJ"   , &Quantity::MilliJoule  },
    UnitTokenSpec { "kJ"  , &Quantity::KiloJoule    }, UnitTokenSpec { "Nm"   , &Quantity::NewtonMeter },
    UnitTokenSpec { "VAs" , &Quantity::VoltAmpereSecond }, UnitTokenSpec { "CV" , &Quantity::WattSecond },
    UnitTokenSpec { "Ws"  , &Quantity::WattSecond   }, UnitTokenSpec { "kWh"  , &Quantity::KiloWattHour },
    UnitTokenSpec { "eV"  , &Quantity::ElectronVolt }, UnitTokenSpec { "keV"  , &Quantity::KiloElectronVolt },
    UnitTokenSpec { "MeV" , &Quantity::MegaElectronVolt }, UnitTokenSpec { "cal", &Quantity::Calorie    },
    UnitTokenSpec { "kcal", &Quantity::KiloCalorie  },

    UnitTokenSpec { "°"   , &Quantity::Degree       }, UnitTokenSpec { "deg"  , &Quantity::Degree      },
    UnitTokenSpec { "rad" , &Quantity::Radian       }, UnitTokenSpec { "gon"  , &Quantity::Gon         },
    UnitTokenSpec { "M"   , &Quantity::AngMinute    }, UnitTokenSpec { "′"    , &Quantity::AngMinute   },
    UnitTokenSpec { "AS"  , &Quantity::AngSecond    }, UnitTokenSpec { "″"    , &Quantity::AngSecond   },
};
        // clang-format on

        /// 函数名到函数标识的对照
        struct FunctionTokenSpec
        {
            std::string_view name;
            FunctionId       function;
        };

        /// 标量函数的名字对照表：走最长匹配，使 log10 胜过 log
        constexpr std::array functionTokenSpecs{
                FunctionTokenSpec{"acos", FunctionId::Acos}, FunctionTokenSpec{"asin", FunctionId::Asin},   FunctionTokenSpec{"atan", FunctionId::Atan},
                FunctionTokenSpec{"cos", FunctionId::Cos},   FunctionTokenSpec{"exp", FunctionId::Exp},     FunctionTokenSpec{"abs", FunctionId::Abs},
                FunctionTokenSpec{"log", FunctionId::Log},   FunctionTokenSpec{"log10", FunctionId::Log10}, FunctionTokenSpec{"sin", FunctionId::Sin},
                FunctionTokenSpec{"sinh", FunctionId::Sinh}, FunctionTokenSpec{"tan", FunctionId::Tan},     FunctionTokenSpec{"tanh", FunctionId::Tanh},
                FunctionTokenSpec{"sqrt", FunctionId::Sqrt},
        };

        /// 首字节相同的候选在分组数组里的区间；组内保持总表顺序，等长匹配时靠前者胜出
        struct FirstByteBucket
        {
            std::uint16_t begin{0}; ///< 区间在分组数组里的起始下标
            std::uint16_t count{0}; ///< 区间里的候选条数
        };

        /// 首字节分派表：256 项索引加一张按首字节分好组的候选指针数组
        template<typename Spec, std::size_t Count>
        struct FirstByteDispatch
        {
            std::array<const Spec *, Count>  entries{}; ///< 按首字节分组的候选指针
            std::array<FirstByteBucket, 256> buckets{}; ///< 每个首字节对应的候选区间
        };

        /// 取候选原文的首字节值，用作分派表的索引
        constexpr std::size_t firstByteOf(const std::string_view text)
        {
            return static_cast<unsigned char>(text.front());
        }

        /// 编译期按首字节做计数排序，把候选指针按组铺进分派表；Member 给出候选的符号原文
        template<typename Spec, std::size_t Count, std::string_view Spec::*Member>
        constexpr FirstByteDispatch<Spec, Count> buildFirstByteDispatch(const std::array<Spec, Count> &specs)
        {
            FirstByteDispatch<Spec, Count> dispatch;
            std::array<std::size_t, 256>   counts{};
            for (const Spec &spec: specs)
            {
                ++counts[firstByteOf(spec.*Member)];
            }

            std::size_t begin = 0;
            for (std::size_t byte = 0; byte < counts.size(); ++byte)
            {
                dispatch.buckets[byte] = {static_cast<std::uint16_t>(begin), static_cast<std::uint16_t>(counts[byte])};
                begin += counts[byte];
            }

            // 游标从各分组起点向后推进，于是同一分组的候选保持它们在总表里的先后
            std::array<std::size_t, 256> cursors{};
            for (std::size_t byte = 0; byte < cursors.size(); ++byte)
            {
                cursors[byte] = dispatch.buckets[byte].begin;
            }
            for (const Spec &spec: specs)
            {
                dispatch.entries[cursors[firstByteOf(spec.*Member)]++] = &spec;
            }
            return dispatch;
        }

        /// 编译期建好的单位符号分派表：查找时先按首字节把候选缩到一组；静态存储期，无运行时初始化与堆分配
        constexpr FirstByteDispatch<UnitTokenSpec, unitTokenSpecs.size()> unitSpecDispatch =
                buildFirstByteDispatch<UnitTokenSpec, unitTokenSpecs.size(), &UnitTokenSpec::symbol>(unitTokenSpecs);

        /// 编译期建好的标量函数名分派表
        constexpr FirstByteDispatch<FunctionTokenSpec, functionTokenSpecs.size()> functionSpecDispatch =
                buildFirstByteDispatch<FunctionTokenSpec, functionTokenSpecs.size(), &FunctionTokenSpec::name>(functionTokenSpecs);

        /// 单位符号的最长匹配结果
        struct UnitMatch
        {
            const UnitTokenSpec *spec{nullptr}; ///< 命中的符号表条目；nullptr 表示没有命中
            std::size_t          length{0};     ///< 匹配到的字节数
        };

        /// 在 text 开头对单位符号做最长匹配：先按首字节把候选缩到一组，再在组内挑最长；
        /// 只在严格更长时替换，因此等长时保留表里靠前的符号
        [[nodiscard]] UnitMatch matchUnitSymbol(const std::string_view text)
        {
            const FirstByteBucket bucket = unitSpecDispatch.buckets[firstByteOf(text)];
            UnitMatch             best;
            for (std::size_t index = 0; index < bucket.count; ++index)
            {
                const UnitTokenSpec *spec = unitSpecDispatch.entries[bucket.begin + index];
                if (spec->symbol.size() > best.length && text.starts_with(spec->symbol))
                {
                    best = {spec, spec->symbol.size()};
                }
            }
            return best;
        }

        /// 函数名的最长匹配结果
        struct FunctionMatch
        {
            const FunctionTokenSpec *spec{nullptr}; ///< 命中的函数表条目；nullptr 表示没有命中
            std::size_t              length{0};     ///< 匹配到的字节数
        };

        /// 在 text 开头对函数名做最长匹配；分组与胜出规则同 matchUnitSymbol
        [[nodiscard]] FunctionMatch matchFunctionName(const std::string_view text)
        {
            const FirstByteBucket bucket = functionSpecDispatch.buckets[firstByteOf(text)];
            FunctionMatch         best;
            for (std::size_t index = 0; index < bucket.count; ++index)
            {
                const FunctionTokenSpec *spec = functionSpecDispatch.entries[bucket.begin + index];
                if (spec->name.size() > best.length && text.starts_with(spec->name))
                {
                    best = {spec, spec->name.size()};
                }
            }
            return best;
        }

        /// 求标量函数值；入参必须是纯数值，单位由调用方保证已被剥掉
        [[nodiscard]] double applyFunction(const FunctionId function, const double argument)
        {
            switch (function)
            {
                case FunctionId::Acos:
                    return std::acos(argument);
                case FunctionId::Asin:
                    return std::asin(argument);
                case FunctionId::Atan:
                    return std::atan(argument);
                case FunctionId::Cos:
                    return std::cos(argument);
                case FunctionId::Exp:
                    return std::exp(argument);
                case FunctionId::Abs:
                    return std::fabs(argument);
                case FunctionId::Log:
                    return std::log(argument);
                case FunctionId::Log10:
                    return std::log10(argument);
                case FunctionId::Sin:
                    return std::sin(argument);
                case FunctionId::Sinh:
                    return std::sinh(argument);
                case FunctionId::Tan:
                    return std::tan(argument);
                case FunctionId::Tanh:
                    return std::tanh(argument);
                case FunctionId::Sqrt:
                    return std::sqrt(argument);
            }

            // 枚举取值来自词法表，走到这里说明表与实现脱节
            throw Base::ExpressionError("数量解析器的函数表与实现不一致，请检查函数登记表");
        }

        /// 匹配一串十进制数字，返回匹配到的字节数
        [[nodiscard]] std::size_t matchDigits(const std::string_view text, const std::size_t position)
        {
            std::size_t count = 0;
            while (position + count < text.size() && characterClasses.decimalDigit[static_cast<unsigned char>(text[position + count])] != 0)
            {
                ++count;
            }
            return count;
        }

        /// 匹配指数部分 [eE][-+]?[0-9]+，返回匹配到的字节数
        [[nodiscard]] std::size_t matchExponent(const std::string_view text, const std::size_t position)
        {
            if (position >= text.size() || (text[position] != 'e' && text[position] != 'E'))
            {
                return 0;
            }

            std::size_t cursor = position + 1;
            if (cursor < text.size() && (text[cursor] == '+' || text[cursor] == '-'))
            {
                ++cursor;
            }

            const auto digits = matchDigits(text, cursor);
            return digits == 0 ? 0 : cursor + digits - position;
        }

        /// 纯整数可以逐位精确累加的最大位数：10^15 小于 2^53，乘 10 加一位都不会丢精度
        constexpr std::size_t exactIntegerDigitLimit = 15;

        /// 数字扫描结果：一次扫描给出匹配长度、所用小数点字符；纯整数时还直接带上数值
        struct NumberScan
        {
            std::size_t length{0};             ///< 匹配到的字节数；0 表示这里不是数字
            char        decimalSeparator{'.'}; ///< 小数点字符，'.' 或 ','
            double      integerValue{0.0};     ///< 纯整数时的数值，可由累加过程直接得到
            bool        isPureInteger{false};  ///< 数值已由 integerValue 给出，不必再从文本转换
        };

        /**
         * @brief 一遍扫描数字
         * @details 等价于 flex 里四条数字规则（带/不带整数部分 × '.'/',' 两种小数点）取最长匹配：
         *          先扫整数部分并顺手累加数值，再按出现的小数点字符扫小数部分，最后扫指数。
         *          最多只会有一个小数点、一个指数，最长匹配的胜出者与逐条试配时一致。
         */
        [[nodiscard]] NumberScan scanNumber(const std::string_view text, const std::size_t position)
        {
            const std::size_t size   = text.size();
            std::size_t       cursor = position;

            double      integerValue = 0.0;
            std::size_t digitCount   = 0;
            while (cursor < size && characterClasses.decimalDigit[static_cast<unsigned char>(text[cursor])] != 0)
            {
                integerValue = integerValue * 10.0 + static_cast<double>(text[cursor] - '0');
                ++digitCount;
                ++cursor;
            }

            NumberScan scan;
            if (digitCount == 0)
            {
                // 整数部分可省：接受以 '.' 或 ',' 开头的小数，如 ".5"、",5"；小数点后至少要有一位数字
                if (cursor >= size || (text[cursor] != '.' && text[cursor] != ','))
                {
                    return scan;
                }
                if (cursor + 1 >= size || characterClasses.decimalDigit[static_cast<unsigned char>(text[cursor + 1])] == 0)
                {
                    return scan;
                }
                scan.decimalSeparator = text[cursor];
                cursor += 1 + matchDigits(text, cursor + 1);
            } else if (cursor < size && (text[cursor] == '.' || text[cursor] == ','))
            {
                scan.decimalSeparator = text[cursor];
                cursor += 1 + matchDigits(text, cursor + 1);
            } else if (digitCount <= exactIntegerDigitLimit)
            {
                // 位数不超上限时逐位累加是精确的，数值现成可用，省掉一次文本转换
                scan.integerValue  = integerValue;
                scan.isPureInteger = true;
            }

            const auto exponentLength = matchExponent(text, cursor);
            if (exponentLength != 0)
            {
                scan.isPureInteger = false;
                cursor += exponentLength;
            }

            scan.length = cursor - position;
            return scan;
        }

        /// 从整段文本解析双精度值；失败时按既有文案报错，display 是写进文案的数字写法
        [[nodiscard]] double parseNumberText(const char *begin, const std::size_t size, const std::string_view display)
        {
            double     value  = 0.0;
            const auto result = std::from_chars(begin, begin + size, value);
            if (result.ec == std::errc::result_out_of_range)
            {
                throw Base::ParserError(std::format("数量文本里的数字 {} 超出双精度可表示范围，请改用量级更小的写法", display));
            }
            if (result.ec != std::errc{} || result.ptr != begin + size)
            {
                throw Base::ParserError(std::format("数量文本里的数字 {} 无法解析，请检查写法", display));
            }

            return value;
        }

        /// 把匹配到的数字文本换算成双精度值；分隔符本就是 '.' 又不需要补尾零时原地解析，不拼临时串
        [[nodiscard]] double convertNumberText(const std::string_view text, const char decimalSeparator)
        {
            // 快路径：原文就是 from_chars 能接受的写法，直接解析；对另一种分隔符仍做一次防御性检查，
            // 免得日后改动扫描规则时这里悄悄失效
            if (decimalSeparator == '.' && text.back() != '.' && text.find(',') == std::string_view::npos)
            {
                return parseNumberText(text.data(), text.size(), text);
            }

            // 慢路径：把小数点统一成 '.'、去掉分组分隔符，并给 "1." 这类写法补一个 0
            std::string canonical;
            canonical.reserve(text.size() + 1);

            const char groupSeparator = decimalSeparator == '.' ? ',' : '.';
            for (const char character: text)
            {
                if (character == groupSeparator)
                {
                    continue;
                }
                canonical += character == decimalSeparator ? '.' : character;
            }

            if (!canonical.empty() && canonical.back() == '.')
            {
                canonical += '0';
            }

            return parseNumberText(canonical.data(), canonical.size(), canonical);
        }

        /// 词法分析器：跳过空白与方括号注释，按下标推进
        class QuantityLexer
        {
        public:
            explicit QuantityLexer(const std::string_view text) : m_text(text)
            {
            }

            [[nodiscard]] Token next();

        private:
            /// 跳过空白与 [注释]；注释未闭合时报错而不是静默吞掉剩余输入
            void skipInsignificant();

            std::string_view m_text;        ///< 待扫描文本
            std::size_t      m_position{0}; ///< 当前字节偏移
        };

        void QuantityLexer::skipInsignificant()
        {
            while (m_position < m_text.size())
            {
                const char character = m_text[m_position];
                if (characterClasses.whitespace[static_cast<unsigned char>(character)] != 0)
                {
                    ++m_position;
                    continue;
                }

                if (character == '[')
                {
                    const auto closing = m_text.find(']', m_position + 1);
                    if (closing == std::string_view::npos)
                    {
                        throw Base::ParserError(std::format("数量文本第 {} 个字符处的方括号注释没有闭合，请补上 ']'", m_position + 1));
                    }
                    m_position = closing + 1;
                    continue;
                }

                break;
            }
        }

        Token QuantityLexer::next()
        {
            skipInsignificant();

            Token token;
            token.offset = m_position;

            if (m_position >= m_text.size())
            {
                token.kind = TokenKind::End;
                return token;
            }

            const auto remaining = m_text.substr(m_position);

            // 单字符运算符优先判定：它们不会与字母开头的记号竞争长度
            const auto singleCharacter = [&token, this](const TokenKind kind, const std::size_t length)
            {
                token.kind = kind;
                m_position += length;
                return token;
            };

            switch (remaining.front())
            {
                case '+':
                    return singleCharacter(TokenKind::Plus, 1);
                case '-':
                    return singleCharacter(TokenKind::Minus, 1);
                case '*':
                    return singleCharacter(TokenKind::Star, 1);
                case '/':
                    return singleCharacter(TokenKind::Slash, 1);
                case '^':
                    return singleCharacter(TokenKind::Caret, 1);
                case '(':
                    return singleCharacter(TokenKind::LeftParen, 1);
                case ')':
                    return singleCharacter(TokenKind::RightParen, 1);
                default:
                    break;
            }

            // Unicode 减号与 ASCII 减号同义
            if (remaining.starts_with("−"))
            {
                return singleCharacter(TokenKind::Minus, std::string_view{"−"}.size());
            }

            // 数值：一遍扫描取最长匹配，纯整数连文本转换都省了
            const NumberScan numberScan = scanNumber(remaining, 0);
            if (numberScan.length != 0)
            {
                token.kind        = TokenKind::Number;
                token.numberValue = numberScan.isPureInteger ? numberScan.integerValue : convertNumberText(remaining.substr(0, numberScan.length), numberScan.decimalSeparator);
                m_position += numberScan.length;
                return token;
            }

            // 单位、函数名与常量都在字母区，必须按最长匹配竞争而不是按类别先后判定：
            // 否则 "sin(" 会被拆成单位 "s" + 单位 "in"，"tan(" 会被拆成吨 + 埃。
            const UnitMatch     unitMatch     = matchUnitSymbol(remaining);
            const FunctionMatch functionMatch = matchFunctionName(remaining);

            std::size_t matchedConstantLength = 0;
            double      matchedConstantValue  = 0.0;
            if (remaining.starts_with("pi"))
            {
                matchedConstantLength = 2;
                matchedConstantValue  = std::numbers::pi;
            } else if (remaining.front() == 'e')
            {
                matchedConstantLength = 1;
                matchedConstantValue  = std::numbers::e;
            }

            const auto bestLength = std::max({unitMatch.length, functionMatch.length, matchedConstantLength});
            if (bestLength != 0)
            {
                // 长度相同时按「单位 → 函数 → 常量」定序，与需求里记号类别的优先级一致
                if (unitMatch.length == bestLength)
                {
                    token.kind         = TokenKind::Unit;
                    token.unitQuantity = unitMatch.spec->quantity;
                } else if (functionMatch.length == bestLength)
                {
                    token.kind     = TokenKind::Function;
                    token.function = functionMatch.spec->function;
                } else
                {
                    token.kind        = TokenKind::Number;
                    token.numberValue = matchedConstantValue;
                }
                m_position += bestLength;
                return token;
            }

            throw Base::ParserError(std::format("数量文本第 {} 个字符处出现无法识别的字符 '{}'，请检查是否多写了符号", m_position + 1, remaining.front()));
        }

        /**
         * @brief 数量语法分析器
         * @details 递归下降实现，优先级自低到高为：加减、乘除、一元正负、乘方、括号与函数，
         *          单位表达式单独一棵子树，单位与数值的结合按「相邻即相乘」处理。
         */
        class QuantityParserImplementation
        {
        public:
            explicit QuantityParserImplementation(const std::string_view text) : m_lexer(text)
            {
                advance();
                advance();
            }

            [[nodiscard]] Quantity parseInput();

        private:
            void advance();

            [[nodiscard]] bool startsNumber() const;

            [[nodiscard]] Quantity parseItem(bool requireUnit);

            [[nodiscard]] Quantity parseAdditive();

            [[nodiscard]] Quantity parseMultiplicative();

            [[nodiscard]] Quantity parseUnary();

            [[nodiscard]] Quantity parsePower();

            [[nodiscard]] Quantity parseNumberAtom();

            [[nodiscard]] Quantity parseUnitExpression();

            [[nodiscard]] Quantity parseUnitProduct();

            [[nodiscard]] Quantity parseUnitPower();

            [[nodiscard]] Quantity parseUnitAtom();

            void expect(const TokenKind kind, const std::string_view description);

            void expectEnd();

            QuantityLexer m_lexer;     ///< 词法分析器
            Token         m_current;   ///< 当前记号
            Token         m_lookahead; ///< 下一记号，用于区分 "1/mm" 与 "1/2"
        };

        void QuantityParserImplementation::advance()
        {
            m_current   = m_lookahead;
            m_lookahead = m_lexer.next();
        }

        bool QuantityParserImplementation::startsNumber() const
        {
            switch (m_lookahead.kind)
            {
                case TokenKind::Number:
                case TokenKind::Function:
                case TokenKind::LeftParen:
                case TokenKind::Plus:
                case TokenKind::Minus:
                    return true;
                default:
                    return false;
            }
        }

        void QuantityParserImplementation::expect(const TokenKind kind, const std::string_view description)
        {
            if (m_current.kind != kind)
            {
                throw Base::ParserError(std::format("数量文本第 {} 个字符处缺少{}，请补上后重试", m_current.offset + 1, description));
            }
            advance();
        }

        void QuantityParserImplementation::expectEnd()
        {
            if (m_current.kind != TokenKind::End)
            {
                throw Base::ParserError(std::format("数量文本第 {} 个字符处出现多余内容，请检查表达式是否完整", m_current.offset + 1));
            }
        }

        Quantity QuantityParserImplementation::parseInput()
        {
            // 空输入的语义沿用原文法：返回最小正数，表示「没有有效内容」
            if (m_current.kind == TokenKind::End)
            {
                return Quantity(std::numeric_limits<double>::min());
            }

            Quantity result = parseItem(false);

            // 原文法只允许至多三段相邻数量求和（如 3' 4" 与 5' 6" 的写法）
            int extraItemCount = 0;
            while (m_current.kind != TokenKind::End)
            {
                if (++extraItemCount > 2)
                {
                    throw Base::ParserError(std::format("数量文本第 {} 个字符处开始的分段过多，相邻数量最多三段（如 5' "
                                                        "6\"），请把其余内容并入前面的表达式",
                                                        m_current.offset + 1));
                }
                result += parseItem(true);
            }

            return result;
        }

        Quantity QuantityParserImplementation::parseItem(const bool requireUnit)
        {
            // 整段就是单位：此时等价于「1 个该单位」
            if (m_current.kind == TokenKind::Unit)
            {
                return parseUnitExpression();
            }

            Quantity value = parseAdditive();

            // num unit 与 num '/' unit 两种形态：前者相邻即相乘，后者只允许除号后直接跟单位
            if (m_current.kind == TokenKind::Unit)
            {
                value = value * parseUnitExpression();
            } else if (m_current.kind == TokenKind::Slash && m_lookahead.kind == TokenKind::Unit)
            {
                advance();
                value = value / parseUnitExpression();
            } else if (requireUnit)
            {
                throw Base::ParserError(std::format("数量文本第 {} 个字符处缺少单位，相邻分段（如 5' 6\"）要求每一段都带单位", m_current.offset + 1));
            }

            return value;
        }

        Quantity QuantityParserImplementation::parseAdditive()
        {
            Quantity value = parseMultiplicative();

            while (m_current.kind == TokenKind::Plus || m_current.kind == TokenKind::Minus)
            {
                const bool isAddition = m_current.kind == TokenKind::Plus;
                advance();
                const Quantity right = parseMultiplicative();
                value                = isAddition ? value + right : value - right;
            }

            return value;
        }

        Quantity QuantityParserImplementation::parseMultiplicative()
        {
            Quantity value = parseUnary();

            while (m_current.kind == TokenKind::Star || m_current.kind == TokenKind::Slash)
            {
                // 除号后紧跟单位时（如 "1/mm"），这一层不消费，交给数量层的 num '/' unit 规则
                if (!startsNumber())
                {
                    break;
                }

                const bool isMultiplication = m_current.kind == TokenKind::Star;
                advance();
                const Quantity right = parseUnary();
                value                = isMultiplication ? value * right : value / right;
            }

            return value;
        }

        Quantity QuantityParserImplementation::parseUnary()
        {
            if (m_current.kind == TokenKind::Plus)
            {
                advance();
                return parseUnary();
            }

            if (m_current.kind == TokenKind::Minus)
            {
                advance();
                return -parseUnary();
            }

            return parsePower();
        }

        Quantity QuantityParserImplementation::parsePower()
        {
            Quantity value = parseNumberAtom();

            // '^' 右结合：指数递归走一元层，因此 2^-3 与 2^3^2 都能正确结合
            if (m_current.kind == TokenKind::Caret)
            {
                advance();
                const Quantity exponent = parseUnary();
                value                   = value.pow(exponent);
            }

            return value;
        }

        Quantity QuantityParserImplementation::parseNumberAtom()
        {
            switch (m_current.kind)
            {
                case TokenKind::Number:
                {
                    // 完整量到这里才构造：记号本身只带数值
                    const Quantity value = Quantity(m_current.numberValue);
                    advance();
                    return value;
                }
                case TokenKind::LeftParen:
                {
                    advance();
                    const Quantity value = parseAdditive();
                    expect(TokenKind::RightParen, "右括号 ')'");
                    return value;
                }
                case TokenKind::Function:
                {
                    const FunctionId function = m_current.function;
                    advance();
                    expect(TokenKind::LeftParen, "函数名后的左括号 '('");
                    const Quantity argument = parseAdditive();
                    expect(TokenKind::RightParen, "函数实参后的右括号 ')'");
                    // 标量函数只作用于数值，实参本身必须是纯数字
                    return Quantity(applyFunction(function, argument.getValue()));
                }
                default:
                    throw Base::ParserError(std::format("数量文本第 {} 个字符处需要数字、函数或左括号，请检查写法", m_current.offset + 1));
            }
        }

        Quantity QuantityParserImplementation::parseUnitExpression()
        {
            return parseUnitProduct();
        }

        Quantity QuantityParserImplementation::parseUnitProduct()
        {
            Quantity value = parseUnitPower();

            while (m_current.kind == TokenKind::Star || m_current.kind == TokenKind::Slash)
            {
                const bool isMultiplication = m_current.kind == TokenKind::Star;
                advance();
                const Quantity right = parseUnitPower();
                value                = isMultiplication ? value * right : value / right;
            }

            return value;
        }

        Quantity QuantityParserImplementation::parseUnitPower()
        {
            Quantity value = parseUnitAtom();

            if (m_current.kind == TokenKind::Caret)
            {
                advance();
                const Quantity exponent = parseUnary();
                if (!exponent.isDimensionless())
                {
                    throw Base::ParserError(std::format("单位幂次必须是无量纲的纯数字，当前写法在偏移 {} 处带了单位", m_current.offset));
                }
                // 幂次不是整数时由 Unit::pow() 报错，这里不做静默取整
                value = value.pow(exponent);
            }

            return value;
        }

        Quantity QuantityParserImplementation::parseUnitAtom()
        {
            switch (m_current.kind)
            {
                case TokenKind::Unit:
                {
                    // 静态预定义量在这里拷一份，记号里只存指针
                    const Quantity value = *m_current.unitQuantity;
                    advance();
                    return value;
                }
                case TokenKind::LeftParen:
                {
                    advance();
                    const Quantity value = parseUnitProduct();
                    expect(TokenKind::RightParen, "右括号 ')'");
                    return value;
                }
                default:
                    throw Base::ParserError(std::format("数量文本第 {} 个字符处需要单位符号或左括号，请检查写法", m_current.offset + 1));
            }
        }
    } // namespace

    Quantity QuantityParser::parse(const std::string_view text)
    {
        QuantityParserImplementation parser{text};
        return parser.parseInput();
    }

    const Quantity *findPredefinedUnit(const std::string_view symbol)
    {
        // 符号表按最长匹配语义使用，这里只需精确相等；先按首字节把候选缩到一组再比原文
        if (symbol.empty())
        {
            return nullptr;
        }
        const FirstByteBucket bucket = unitSpecDispatch.buckets[firstByteOf(symbol)];
        for (std::size_t index = 0; index < bucket.count; ++index)
        {
            const UnitTokenSpec *spec = unitSpecDispatch.entries[bucket.begin + index];
            if (spec->symbol == symbol)
            {
                return spec->quantity;
            }
        }
        return nullptr;
    }
} // namespace ExpressionEngine::Units
