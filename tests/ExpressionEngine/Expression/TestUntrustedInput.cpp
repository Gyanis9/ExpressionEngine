// 不受信输入的鲁棒性：把随机拼出的与刻意构造的坏文本喂进「不抛异常」那条通道，判三件事——
// 没有任何异常漏出库外、公开承诺的上限不会被越过、交给宿主的裸数与数量取值必定有限。
// 单条缺陷由 TestExpression*.cpp 里的手写用例钉；本文件要钉的是「这一类输入都不会把库打穿」。

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/ParseFailure.h>
#include <ExpressionEngine/Expression/Expression.h>
#include <ExpressionEngine/Expression/ExpressionParser.h>
#include <ExpressionEngine/Expression/Value.h>
#include <ExpressionEngine/Units/Quantity.h>

namespace ExpressionEngine::Expression
{
    namespace
    {
        /**
         * @brief 自带的小型确定性伪随机源（xorshift64*）
         * @details 刻意不用 std::uniform_int_distribution：它把随机数映射到区间的做法是实现定义的，
         *          换一家标准库就换一条序列，「本机复现的缺陷」就不再在别人机器上复现。这里只有
         *          位运算与取模（有轻微偏置，对造坏文本无所谓），任何实现给出同一条序列。
         */
        class RandomSource
        {
        public:
            /**
             * @brief 用种子建随机源
             * @param seed 种子；全零状态会让 xorshift 不动，故零按 1 处理
             */
            explicit RandomSource(const std::uint64_t seed) : m_state(seed != 0 ? seed : 1U)
            {
            }

            /**
             * @brief 取下一个 64 位随机量
             * @return 混合后的输出
             */
            [[nodiscard]] std::uint64_t next()
            {
                m_state ^= m_state >> 12;
                m_state ^= m_state << 25;
                m_state ^= m_state >> 27;
                return m_state * 0x2545F4914F6CDD1DU;
            }

            /**
             * @brief 取 [0, bound) 内的一个量
             * @param bound 上界，须大于零
             * @return 落界结果
             */
            [[nodiscard]] std::size_t below(const std::size_t bound)
            {
                return static_cast<std::size_t>(next() % static_cast<std::uint64_t>(bound));
            }

            /**
             * @brief 按「denominator 分之一」的概率决定要不要做某事
             * @param denominator 分母，例如 4 表示四分之一概率
             * @return 命中为 true
             */
            [[nodiscard]] bool chance(const std::size_t denominator)
            {
                return below(denominator) == 0;
            }

        private:
            std::uint64_t m_state; ///< 序列状态
        };

        /**
         * @brief 一条输入的判据结果
         * @details 只填用得上的槽：解析成功填 tree，失败填 failure，漏出的异常填 escapedWithMessage。
         */
        struct Probe
        {
            ExpressionPtr      tree;               ///< 解析成功时给出的树
            Base::ParseFailure failure{};          ///< 解析失败时给出的失败数据
            std::string        escapedWithMessage; ///< 非空表示有异常漏出了 tryParse 这条不抛异常的通道
        };

        /**
         * @brief 走「不抛异常」通道解析一条文本，并把任何漏出的异常记进结果
         * @param text 待解析文本
         * @return 判据结果
         */
        Probe probeParse(const std::string_view text)
        {
            Probe probe;
            try
            {
                auto parsed = ExpressionParser::tryParse(nullptr, text);
                if (parsed.has_value())
                {
                    probe.tree = std::move(*parsed);
                } else
                {
                    probe.failure = parsed.error();
                }
            } catch (const Base::Exception &error)
            {
                probe.escapedWithMessage = std::string("漏出库内异常: ") + error.message();
            } catch (const std::exception &error)
            {
                probe.escapedWithMessage = std::string("标准异常: ") + error.what();
            } catch (...)
            {
                probe.escapedWithMessage = "未知类型异常";
            }

            return probe;
        }

        /// 判据用到的取值种类：只有裸数与带单位的量承诺「必有限」，其余取值按契约原样交给宿主
        bool mustBeFinite(const Value &value)
        {
            return std::holds_alternative<double>(value) || std::holds_alternative<Units::Quantity>(value);
        }

        /**
         * @brief 对一条输入跑全部判据，返回它是否解析成功（供调用方统计，防止用例空转）
         * @param text 待判文本
         * @return 解析成功为 true
         */
        bool checkInput(const std::string &text)
        {
            SCOPED_TRACE(text);

            const Probe probe = probeParse(text);
            if (!probe.escapedWithMessage.empty())
            {
                ADD_FAILURE() << "tryParse 漏出异常: " << probe.escapedWithMessage;
                return false;
            }

            if (probe.tree == nullptr)
            {
                // 失败支的判据：文案不能是空的，位置给出时得落在文本范围内
                EXPECT_FALSE(probe.failure.message.empty()) << "解析失败却没给出原因";
                if (probe.failure.column.has_value())
                {
                    EXPECT_GT(probe.failure.column.value(), 0);
                    EXPECT_LE(static_cast<std::size_t>(probe.failure.column.value()), text.size() + 1);
                }

                return false;
            }

            EXPECT_LE(probe.tree->astDepth(), Expression::maxAstDepth) << "树深越过了公开承诺的上限";

            // 文本化、化简、深拷贝与求值都只走库内通道：宿主缺席（resolver 为 nullptr）时
            // 任何漏出的异常都是库的缺陷，不是宿主的错。
            std::string printed;
            try
            {
                printed = probe.tree->toString(true);
            } catch (const std::exception &error)
            {
                ADD_FAILURE() << "toString 漏出异常: " << error.what();
            }
            EXPECT_FALSE(printed.empty());

            try
            {
                const ExpressionPtr simplified = probe.tree->simplify();
                if (simplified != nullptr)
                {
                    EXPECT_LE(simplified->astDepth(), Expression::maxAstDepth);
                }
            } catch (const Base::Exception &)
            {
                // 化简途中撞上限是允许的：那是库内异常，走的是调用方能 catch 的那条通道
            } catch (const std::exception &error)
            {
                ADD_FAILURE() << "simplify 漏出非库内异常: " << error.what();
            }

            try
            {
                const ExpressionPtr copied = probe.tree->copy();
                EXPECT_NE(copied, nullptr);
            } catch (const std::exception &error)
            {
                ADD_FAILURE() << "copy 漏出异常: " << error.what();
            }

            try
            {
                const auto evaluated = probe.tree->tryEvaluate();
                if (evaluated.has_value())
                {
                    if (mustBeFinite(*evaluated))
                    {
                        const double number = std::holds_alternative<double>(*evaluated) ? std::get<double>(*evaluated) : std::get<Units::Quantity>(*evaluated).getValue();
                        EXPECT_TRUE(std::isfinite(number)) << "非有限的裸数或数量取值被交给了宿主";
                    }
                } else
                {
                    EXPECT_FALSE(evaluated.error().message.empty());
                }
            } catch (const std::exception &error)
            {
                ADD_FAILURE() << "tryEvaluate 漏出异常: " << error.what();
            }

            // 自己写出去的文本再喂回自己：必须还是那两条通道之一，不能因为「来自库内」就例外
            const Probe reparsed = probeParse(printed);
            EXPECT_TRUE(reparsed.escapedWithMessage.empty()) << "重新解析库内文本漏出异常: " << reparsed.escapedWithMessage;

            // 持久文本是宿主存盘的内容：必须解析得回来，且引用路径逐格相同、文本再写一次还是
            // 同一串、树形要比得过——`Box.<<a.b>>` 打成 `Box.a.b` 也解析得动，读回来却是另一条路径；
            // 非有限常量曾打成 `inf`，读回来变成一个变量引用（现在折回 `1e400` 写法）。
            if (reparsed.tree == nullptr)
            {
                ADD_FAILURE() << "持久文本 [" << printed << "] 解析不回来";
                return false;
            }
            EXPECT_EQ(reparsed.tree->collectReferences(), probe.tree->collectReferences()) << "持久文本 [" << printed << "] 重解析后引用路径变了";
            EXPECT_EQ(reparsed.tree->toString(true), printed) << "持久文本 [" << printed << "] 再写一次不稳定";
            EXPECT_TRUE(probe.tree->isSame(*reparsed.tree)) << "持久文本 [" << printed << "] 重解析后成了另一棵树";

            return true;
        }

        /// 拼一条输入用的零件：数字与写法边界、运算符、单位、引用、函数、区间、文本与噪声。
        /// 每项都显式写成 string_view 并用 CTAD 定长度，条目增减时不必再改一个手数的容量数字。
        constexpr std::array inputPieces{
                std::string_view{"1"},
                std::string_view{"2"},
                std::string_view{"0"},
                std::string_view{"-1"},
                std::string_view{"1.5"},
                std::string_view{".5"},
                std::string_view{"1e400"},
                std::string_view{"99999999999999999999"},
                std::string_view{"0.000000000001"},
                std::string_view{"+"},
                std::string_view{"-"},
                std::string_view{"*"},
                std::string_view{"/"},
                std::string_view{"^"},
                std::string_view{"("},
                std::string_view{")"},
                std::string_view{"["},
                std::string_view{"]"},
                std::string_view{":"},
                std::string_view{";"},
                std::string_view{","},
                std::string_view{"?"},
                std::string_view{"="},
                std::string_view{"<"},
                std::string_view{">>"},
                std::string_view{"!"},
                std::string_view{"&"},
                std::string_view{"%"},
                std::string_view{"mm"},
                std::string_view{"m"},
                std::string_view{"deg"},
                std::string_view{"kg"},
                std::string_view{"zzz"},
                std::string_view{"µm"},
                std::string_view{"5'"},
                std::string_view{"6\""},
                std::string_view{"Box"},
                std::string_view{"Length"},
                std::string_view{"Box.Length"},
                std::string_view{"<<Sheet#A1>>"},
                std::string_view{"<<x>>"},
                std::string_view{"<<文本>>"},
                std::string_view{"\"abc\""},
                std::string_view{"sqrt("},
                std::string_view{"log("},
                std::string_view{"sin("},
                std::string_view{"sum("},
                std::string_view{"max("},
                std::string_view{"list("},
                std::string_view{"bogus("},
                std::string_view{" "},
                std::string_view{"\t"},
                std::string_view{"\n"},
                std::string_view{"A1"},
                std::string_view{"B2"},
                std::string_view{"true"},
                std::string_view{"中文"},
                std::string_view{"\\"},
                std::string_view{"x\0y", 3},
        };

        /**
         * @brief 用随机零件拼一条坏文本
         * @param random 随机源
         * @return 长度不定的输入
         */
        std::string makeRandomInput(RandomSource &random)
        {
            const std::size_t pieces = 1 + random.below(16);
            std::string       text;
            for (std::size_t index = 0; index < pieces; ++index)
            {
                text += inputPieces[random.below(inputPieces.size())];
                if (random.chance(4))
                {
                    text += ' ';
                }
            }

            return text;
        }

        /**
         * @brief 拼一条同类运算的长链（`1+1+1+…`）
         * @details 左结合同类运算把树叠成又深又窄的形状，是深度记账的主要压力来源。随机零件里运算符
         *          太少，拼出来的式子深不到哪儿去——把上限判据撤掉也不会红（本轮实测如此），
         *          所以这一形状必须专门造，「树深不越上限」才真的有人守着。
         * @param random 随机源
         * @return 一条长链文本
         */
        std::string makeRandomChain(RandomSource &random)
        {
            constexpr std::array<char, 4> operators{'+', '-', '*', '/'};

            std::string       text{"1"};
            const std::size_t terms = 2 + random.below(200);
            for (std::size_t index = 1; index < terms; ++index)
            {
                text += operators[random.below(operators.size())];
                text += '1';
            }

            return text;
        }

        /// 供「随机 + 构造」两支共用的锚定语料：已知的边界写法与曾经真出过事的形状
        constexpr std::array<std::string_view, 12> seedCorpus{
                "1 + 2 * 3", "sqrt(-1)", "log(0)", "10^999", "1e308 * 1e308", "0 / 0", "sum(A1:ZZ16384)", "((((1))))", "Box.Length + 2 mm", "<<Sheet#A1>>.Box.Length", "list(1; 2 mm)[0:3]", "5' 6\"",
        };

        /**
         * @brief 在锚定语料上做单点突变（插入、删除、替换、截断、重复）
         * @param random 随机源
         * @return 一条与已知形状相邻的输入
         */
        std::string makeMutatedInput(RandomSource &random)
        {
            /// 替换用的字符集：都是语法零件，故意不含 NUL——把 NUL 塞进中间的位置由极端输入那一支负责
            constexpr std::array<char, 10> mutationChars{'(', ')', '+', '*', '/', '[', ']', ':', ';', ' '};

            std::string text(seedCorpus[random.below(seedCorpus.size())]);
            if (text.empty())
            {
                return text;
            }

            const std::size_t position = random.below(text.size());
            switch (random.below(5))
            {
                case 0:
                    text.insert(position, inputPieces[random.below(inputPieces.size())]);
                    break;
                case 1:
                    text.erase(position);
                    break;
                case 2:
                    text[position] = mutationChars[random.below(mutationChars.size())];
                    break;
                case 3:
                    text = text.substr(0, position);
                    break;
                default:
                    text += text;
                    break;
            }

            return text;
        }
    } // namespace

    /**
     * @brief 钉住：随机拼出的坏文本不会把库打穿——没有异常漏出不抛异常的通道，树深不越上限，
     *        交给宿主的裸数与数量取值必有限
     * @details 两轮各一万条：一轮在零件表上随机拼（四分之一换成同类运算的长链，深树只有这一支压得出来），
     *          一轮在锚定语料上做单点突变（随机拼出来的多半是语法就错的短串，突变支才够得着
     *          「差一点就合法」的形状）。
     *          末尾断言两支都真的产出了成功与失败两种结果：如果生成器哪天只会造出必然失败的输入，
     *          前面那些判据就全是空判，这条自检负责让空转变红。
     */
    TEST(UntrustedInputTest, RandomInputsStayWithinTheDocumentedChannels)
    {
        RandomSource random(0x9E3779B97F4A7C15ULL);

        std::size_t composedParsed = 0;
        std::size_t composedFailed = 0;
        std::size_t mutatedParsed  = 0;
        std::size_t mutatedFailed  = 0;

        for (std::size_t iteration = 0; iteration < 10000; ++iteration)
        {
            // 四分之一走长链支：随机零件拼不出足够深的树，深度上限那条判据只能靠造的长链压
            const std::string text = random.chance(4) ? makeRandomChain(random) : makeRandomInput(random);
            checkInput(text) ? ++composedParsed : ++composedFailed;
        }

        for (std::size_t iteration = 0; iteration < 10000; ++iteration)
        {
            const std::string text = makeMutatedInput(random);
            checkInput(text) ? ++mutatedParsed : ++mutatedFailed;
        }

        EXPECT_GT(composedParsed, 0U);
        EXPECT_GT(composedFailed, 0U);
        EXPECT_GT(mutatedParsed, 0U);
        EXPECT_GT(mutatedFailed, 0U);
    }

    /**
     * @brief 钉住：刻意构造的极端输入（深括号、长链、超长文本、内嵌 NUL、纯空白）都不把库打穿
     * @details 这些形状的资源上限各自有专门的用例钉「拒绝」，本条钉的是「拒绝也走正常通道」：
     *          既不崩溃也不漏异常，而且真解析出来的树仍在上限之内。类别断言不在这里重复。
     */
    TEST(UntrustedInputTest, ConstructedExtremeInputsAreRefusedThroughTheNormalChannel)
    {
        const std::string deepParens = std::string(5000, '(') + "1" + std::string(5000, ')');
        // 真·左结合长链（3000 项同类运算）。原先写成「1 后面跟 20000 个 +」，那是 20000 层一元正号，
        // 被括号/一元那套限深先拦掉了，深度上限那条判据在这条输入上根本走不到——撤掉上限也不变红。
        std::string longChain{"1"};
        for (std::size_t term = 1; term < 3000; ++term)
        {
            longChain += " + 1";
        }
        const std::string hugeNumber = std::string(1000000, '9');
        const std::string onlyBlanks = std::string(200000, ' ');
        std::string       withNul    = "1 + ";
        withNul += '\0';
        withNul += " 2";

        const std::array<std::string_view, 6> hostiles{
                std::string_view(deepParens), std::string_view(longChain), std::string_view(hugeNumber), std::string_view(onlyBlanks), std::string_view(withNul), "sum(A1:ZZ16384)",
        };

        for (const std::string_view hostile: hostiles)
        {
            const Probe probe = probeParse(hostile);
            if (!probe.escapedWithMessage.empty())
            {
                ADD_FAILURE() << "极端输入漏出异常（长度 " << hostile.size() << "）: " << probe.escapedWithMessage;
                continue;
            }

            if (probe.tree != nullptr)
            {
                EXPECT_LE(probe.tree->astDepth(), Expression::maxAstDepth);
                const auto evaluated = probe.tree->tryEvaluate();
                if (evaluated.has_value() && mustBeFinite(*evaluated))
                {
                    const double number = std::holds_alternative<double>(*evaluated) ? std::get<double>(*evaluated) : std::get<Units::Quantity>(*evaluated).getValue();
                    EXPECT_TRUE(std::isfinite(number));
                }
            } else
            {
                EXPECT_FALSE(probe.failure.message.empty());
            }
        }
    }
} // namespace ExpressionEngine::Expression
