// 本文件钉住宿主可扩展函数集：注册表的校验面、自定义函数节点的求值/文本/依赖与隔离性。

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Vector3D.h>
#include <ExpressionEngine/Expression/ExpressionLexer.h>
#include <ExpressionEngine/Expression/ExpressionParser.h>
#include <ExpressionEngine/Expression/FunctionRegistry.h>
#include <ExpressionEngine/Units/Quantity.h>

namespace ExpressionEngine::Expression
{
    namespace
    {

        /// 只有一个对象、一个属性的最小宿主，够钉住引用依赖与解析器透传
        class FakeProperty : public IProperty
        {
        public:
            FakeProperty(std::string propertyName, Value currentValue) : m_name(std::move(propertyName)), m_value(std::move(currentValue))
            {
            }

            std::string_view name() const override
            {
                return m_name;
            }

            std::string_view typeName() const override
            {
                return "Length";
            }

            std::optional<Value> value() const override
            {
                return m_value;
            }

            bool setValue(const Value &newValue) override
            {
                m_value = newValue;
                return true;
            }

        private:
            std::string m_name;  ///< 属性名
            Value       m_value; ///< 属性值
        };

        /// 承载 FakeProperty 的对象
        class FakeObject : public IObject
        {
        public:
            FakeObject(std::string objectName, std::vector<std::unique_ptr<FakeProperty>> properties) : m_name(std::move(objectName)), m_properties(std::move(properties))
            {
            }

            std::string_view name() const override
            {
                return m_name;
            }

            std::string_view label() const override
            {
                return m_name;
            }

            IProperty *findProperty(const std::string_view propertyName) override
            {
                for (const auto &property: m_properties)
                {
                    if (property->name() == propertyName)
                    {
                        return property.get();
                    }
                }
                return nullptr;
            }

            std::vector<std::string> propertyNames() const override
            {
                std::vector<std::string> names;
                names.reserve(m_properties.size());
                for (const auto &property: m_properties)
                {
                    names.push_back(std::string{property->name()});
                }
                return names;
            }

            std::string_view documentName() const override
            {
                return {};
            }

        private:
            std::string                                m_name;       ///< 对象名
            std::vector<std::unique_ptr<FakeProperty>> m_properties; ///< 属性，按对象名解析后由本对象持有
        };

        /// 只认得 Box 这个对象的解析器
        class FakeResolver : public IObjectResolver
        {
        public:
            FakeResolver()
            {
                std::vector<std::unique_ptr<FakeProperty>> properties;
                properties.push_back(std::make_unique<FakeProperty>("Length", Units::Quantity(3.0)));
                m_objects.push_back(std::make_unique<FakeObject>("Box", std::move(properties)));
            }

            IObject *resolve(const std::string_view documentName, const std::string_view objectName) override
            {
                if (!documentName.empty())
                {
                    return nullptr;
                }
                for (const auto &object: m_objects)
                {
                    if (object->name() == objectName)
                    {
                        return object.get();
                    }
                }
                return nullptr;
            }

            std::vector<std::string> objectNames(const std::string_view) const override
            {
                return {"Box"};
            }

        private:
            std::vector<std::unique_ptr<FakeObject>> m_objects; ///< 宿主对象树
        };

        /// 造一条数量实参的自定义函数：function 直接给出实现
        [[nodiscard]] CustomFunctionSpec makeSpec(std::string name, CustomFunction function, std::size_t minArguments = 0, std::optional<std::size_t> maxArguments = {})
        {
            return CustomFunctionSpec{
                    .name         = std::move(name),
                    .function     = std::move(function),
                    .minArguments = minArguments,
                    .maxArguments = maxArguments,
                    .usage        = "用例函数",
            };
        }

        /// 取求值结果里的数量数值；非数量由用例报告，便于定位是哪个表达式走偏
        [[nodiscard]] double quantityValueOf(const Expression &expression)
        {
            const Value value         = expression.evaluate();
            const auto *quantityValue = std::get_if<Units::Quantity>(&value);
            if (quantityValue == nullptr)
            {
                ADD_FAILURE() << "求值结果不是数量，而是" << valueTypeName(value);
                return 0.0;
            }
            return quantityValue->getValue();
        }

        /**
         * @brief 钉住：登记好的自定义函数可以像内建函数一样调用，且单位随实参传递
         */
        TEST(FunctionRegistryTest, RegisteredFunctionIsCallableFromExpression)
        {
            FunctionRegistry registry;
            ASSERT_TRUE(registry.registerFunction(makeSpec("triple", [](const FunctionCall &call) { return Units::Quantity(3.0) * toQuantity(call.argumentValue(0), "triple 的实参"); }, 1, 1)).has_value());

            const ExpressionPtr expression = ExpressionParser::parse(nullptr, "triple(2 mm)", registry);
            EXPECT_EQ(expression->nodeName(), "CustomFunction");
            EXPECT_DOUBLE_EQ(quantityValueOf(*expression), 6.0);

            // 名字带空白也能识别，与内建函数同一套词法
            EXPECT_DOUBLE_EQ(quantityValueOf(*ExpressionParser::parse(nullptr, "triple ( 2 mm )", registry)), 6.0);
        }

        /**
         * @brief 钉住：注册表的校验面——空回调、非法名字、内置同名、区间颠倒、重复登记都要被拒
         */
        TEST(FunctionRegistryTest, RejectsInvalidRegistrations)
        {
            FunctionRegistry registry;

            // 回调为空：节点无处求值
            EXPECT_FALSE(registry.registerFunction(makeSpec("nop", nullptr, 0)).has_value());

            // 内置函数名不可覆盖，也不可被遮蔽
            EXPECT_FALSE(registry.registerFunction(makeSpec("sqrt", [](const FunctionCall &) { return Value{1.0}; }, 1)).has_value());

            // 参数个数区间自相矛盾
            EXPECT_FALSE(registry.registerFunction(makeSpec("odd", [](const FunctionCall &) { return Value{1.0}; }, 3, 2)).has_value());
            EXPECT_EQ(registry.size(), 0U);

            for (const std::string &badName: {"", "1abc", "_private", "a b", "a@b", "a-b", "a+b", "a\u2212b"})
            {
                SCOPED_TRACE(badName);
                const auto failed = registry.registerFunction(makeSpec(badName, [](const FunctionCall &) { return Value{1.0}; }, 0));
                ASSERT_FALSE(failed.has_value());
                EXPECT_FALSE(failed.error().empty()) << "拒绝理由必须给出可操作文案";
            }
            EXPECT_TRUE(registry.names().empty());
        }

        /**
         * @brief 钉住：函数名校验与词法器的函数记号规则同源
         */
        TEST(FunctionRegistryTest, NameValidationMirrorsLexer)
        {
            // 下划线开头只会识别成标识符，因此不能作为函数名
            EXPECT_FALSE(isFunctionNameText(""));
            EXPECT_FALSE(isFunctionNameText("_x"));
            EXPECT_FALSE(isFunctionNameText("9x"));
            EXPECT_TRUE(isFunctionNameText("x"));
            EXPECT_TRUE(isFunctionNameText("my_func2"));
            EXPECT_TRUE(isFunctionNameText("面积"));

            FunctionRegistry registry;
            EXPECT_FALSE(registry.registerFunction(makeSpec("_neverCallable", [](const FunctionCall &) { return Value{1.0}; }, 0)).has_value());
        }

        /**
         * @brief 钉住：注销后同名可换实现，已解析出的节点仍用旧实现
         */
        TEST(FunctionRegistryTest, UnregisterAllowsReplacementAndKeptOldNodes)
        {
            FunctionRegistry registry;
            ASSERT_TRUE(registry.registerFunction(makeSpec("tag", [](const FunctionCall &) { return Units::Quantity(1.0); }, 0)).has_value());

            // 重复登记被拒：同名两套实现会让表达式文本的含义取决于登记顺序
            EXPECT_FALSE(registry.registerFunction(makeSpec("tag", [](const FunctionCall &) { return Units::Quantity(2.0); }, 0)).has_value());

            const ExpressionPtr before = ExpressionParser::parse(nullptr, "tag()", registry);
            EXPECT_DOUBLE_EQ(quantityValueOf(*before), 1.0);

            // 注销与再登记只影响之后解析的表达式，节点自持实现
            ASSERT_TRUE(registry.unregisterFunction("tag"));
            EXPECT_FALSE(registry.unregisterFunction("tag"));
            ASSERT_TRUE(registry.registerFunction(makeSpec("tag", [](const FunctionCall &) { return Units::Quantity(9.0); }, 0)).has_value());
            EXPECT_DOUBLE_EQ(quantityValueOf(*before), 1.0);
            EXPECT_DOUBLE_EQ(quantityValueOf(*ExpressionParser::parse(nullptr, "tag()", registry)), 9.0);
        }

        /**
         * @brief 钉住：实参个数在建节点时校验，异常通道与非异常通道给出同一份文案
         */
        TEST(FunctionRegistryTest, ArgumentCountIsCheckedWhileBuildingTree)
        {
            FunctionRegistry registry;
            ASSERT_TRUE(registry.registerFunction(makeSpec("pair", [](const FunctionCall &call) { return toQuantity(call.argumentValue(0), "a") + toQuantity(call.argumentValue(1), "b"); }, 2, 2)).has_value());

            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "pair(1)", registry)), EvaluationError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "pair(1; 2; 3)", registry)), EvaluationError);

            const auto failed = ExpressionParser::tryParse(nullptr, "pair(1)", registry);
            ASSERT_FALSE(failed.has_value());
            EXPECT_NE(failed.error().message.find("需要恰好 2 个参数"), std::string::npos) << failed.error().message;
        }

        /**
         * @brief 钉住：不限个数与「至少 N 个」两种登记要求
         */
        TEST(FunctionRegistryTest, VariadicAndMinimumArgumentCounts)
        {
            FunctionRegistry registry;
            ASSERT_TRUE(registry.registerFunction(makeSpec("sumAll",
                                                           [](const FunctionCall &call)
                                                           {
                                                               Units::Quantity total;
                                                               for (const Value &value: call.argumentValues())
                                                               {
                                                                   total = total + toQuantity(value, "sumAll 的实参");
                                                               }
                                                               return total;
                                                           }))
                                .has_value());
            EXPECT_DOUBLE_EQ(quantityValueOf(*ExpressionParser::parse(nullptr, "sumAll(1; 2; 3; 4)", registry)), 10.0);
            EXPECT_DOUBLE_EQ(quantityValueOf(*ExpressionParser::parse(nullptr, "sumAll()", registry)), 0.0);

            FunctionRegistry atLeastOne;
            ASSERT_TRUE(atLeastOne.registerFunction(makeSpec("first", [](const FunctionCall &call) { return call.argumentValue(0); }, 1)).has_value());
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "first()", atLeastOne)), EvaluationError);
            EXPECT_DOUBLE_EQ(quantityValueOf(*ExpressionParser::parse(nullptr, "first(7)", atLeastOne)), 7.0);
        }

        /**
         * @brief 钉住：未登记且非内置的函数名照旧报出可操作文案
         */
        TEST(FunctionRegistryTest, UnknownFunctionStillRejected)
        {
            FunctionRegistry registry;
            const auto       failed = ExpressionParser::tryParse(nullptr, "nope(1)", registry);
            ASSERT_FALSE(failed.has_value());
            EXPECT_NE(failed.error().message.find("nope"), std::string::npos) << failed.error().message;

            // 内置函数不走注册表，两个入口都能调到
            EXPECT_DOUBLE_EQ(quantityValueOf(*ExpressionParser::parse(nullptr, "abs(-3)", registry)), 3.0);
        }

        /**
         * @brief 钉住：注册表彼此隔离，默认入口走进程级注册表
         */
        TEST(FunctionRegistryTest, RegistriesAreIsolatedAndGlobalIsDefault)
        {
            FunctionRegistry pluginA;
            FunctionRegistry pluginB;
            ASSERT_TRUE(pluginA.registerFunction(makeSpec("onlyA", [](const FunctionCall &) { return Units::Quantity(1.0); }, 0)).has_value());
            ASSERT_TRUE(pluginB.registerFunction(makeSpec("onlyB", [](const FunctionCall &) { return Units::Quantity(2.0); }, 0)).has_value());

            EXPECT_TRUE(pluginA.contains("onlyA"));
            EXPECT_FALSE(pluginA.contains("onlyB"));
            EXPECT_NE(pluginA.find("onlyA"), nullptr);
            EXPECT_EQ(pluginA.find("onlyB"), nullptr);

            // 不带注册表参数的入口查默认注册表，因此这里必须查不到
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "onlyA()")), Base::ParserError);

            FunctionRegistry &globalRegistry   = FunctionRegistry::global();
            const std::size_t registeredBefore = globalRegistry.size();
            ASSERT_TRUE(globalRegistry.registerFunction(makeSpec("qoderCaseTriple", [](const FunctionCall &call) { return Units::Quantity(3.0) * toQuantity(call.argumentValue(0), "x"); }, 1, 1)).has_value());

            // 默认入口与显式传默认注册表等价
            EXPECT_DOUBLE_EQ(quantityValueOf(*ExpressionParser::parse(nullptr, "qoderCaseTriple(5)")), 15.0);
            EXPECT_DOUBLE_EQ(quantityValueOf(*ExpressionParser::parse(nullptr, "qoderCaseTriple(5)", globalRegistry)), 15.0);
            // 另一条线看不到它
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "qoderCaseTriple(5)", pluginA)), Base::ParserError);

            EXPECT_TRUE(globalRegistry.unregisterFunction("qoderCaseTriple"));
            EXPECT_EQ(globalRegistry.size(), registeredBefore);
        }

        /**
         * @brief 钉住：回调拿到的是表达式，可自选分支求值，未被选中的分支不产生副作用
         */
        TEST(FunctionRegistryTest, CallbackCanEvaluateArgumentsLazily)
        {
            FunctionRegistry registry;
            int              boomCalls = 0;

            ASSERT_TRUE(registry.registerFunction(makeSpec(
                                                          "boom",
                                                          [&boomCalls](const FunctionCall &) -> Value
                                                          {
                                                              ++boomCalls;
                                                              throw Base::ValueError("boom() 被求值了");
                                                          },
                                                          0))
                                .has_value());
            ASSERT_TRUE(registry.registerFunction(makeSpec(
                                                          "pick",
                                                          [](const FunctionCall &call)
                                                          {
                                                              // 第一个实参当真值用，只算选中的那一支
                                                              const std::size_t index = toBool(call.argumentValue(0), "pick 的条件") ? 1 : 2;
                                                              return call.argumentValue(index);
                                                          },
                                                          3, 3))
                                .has_value());

            EXPECT_DOUBLE_EQ(quantityValueOf(*ExpressionParser::parse(nullptr, "pick(1 > 0; 5; boom())", registry)), 5.0);
            EXPECT_EQ(boomCalls, 0);

            // 选中那一支才会触发副作用
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "pick(1 < 0; 5; boom())", registry)->evaluate()), Base::ValueError);
            EXPECT_EQ(boomCalls, 1);
        }

        /**
         * @brief 钉住：实参下标越界的两种处置——取表达式给空指针，取值报索引错
         */
        TEST(FunctionRegistryTest, ArgumentAccessBeyondEnd)
        {
            FunctionRegistry registry;
            ASSERT_TRUE(registry.registerFunction(makeSpec(
                                                          "probe",
                                                          [](const FunctionCall &call)
                                                          {
                                                              // 越界的表达式视图返回空，取值则必须明确报错而不是静默给 0
                                                              EXPECT_EQ(call.argument(5), nullptr);
                                                              EXPECT_EQ(call.argumentCount(), 2U);
                                                              EXPECT_EQ(call.name(), "probe");
                                                              return call.argumentValue(5);
                                                          },
                                                          2, 2))
                                .has_value());

            const ExpressionPtr expression = ExpressionParser::parse(nullptr, "probe(1; 2)", registry);
            EXPECT_THROW(static_cast<void>(expression->evaluate()), Base::IndexError);
        }

        /**
         * @brief 钉住：自定义函数可访问宿主解析器，引用依赖也照收
         */
        TEST(FunctionRegistryTest, HostResolverReachableAndReferencesCollected)
        {
            FunctionRegistry registry;
            ASSERT_TRUE(registry.registerFunction(makeSpec(
                                                          "doubleOf",
                                                          [](const FunctionCall &call)
                                                          {
                                                              // 直接复用宿主解析器读别的属性，证明解析器透传到位
                                                              EXPECT_NE(call.resolver(), nullptr);
                                                              return Units::Quantity(2.0) * toQuantity(call.argumentValue(0), "doubleOf 的实参");
                                                          },
                                                          1, 1))
                                .has_value());

            FakeResolver        resolver;
            const ExpressionPtr expression = ExpressionParser::parse(&resolver, "doubleOf(Box.Length) + Box.Length", registry);

            const std::vector<VariableReference> references = expression->collectReferences();
            ASSERT_EQ(references.size(), 1U);
            EXPECT_EQ(references.front().objectName, "Box");
            EXPECT_EQ(references.front().propertyName, "Length");

            // Box.Length 是 3，doubleOf 给出 6，再加 3 得 9
            EXPECT_DOUBLE_EQ(quantityValueOf(*expression), 9.0);
        }

        /**
         * @brief 钉住：回调抛出的引擎异常原样向上传播，文案不被包装
         */
        TEST(FunctionRegistryTest, CallbackErrorsPropagateUnwrapped)
        {
            FunctionRegistry registry;
            ASSERT_TRUE(registry.registerFunction(makeSpec("needsNumber", [](const FunctionCall &call) { return Value{toDouble(call.argumentValue(0), "needsNumber 的实参")}; }, 1, 1)).has_value());

            // 文本实参进不了纯数值通道
            const ExpressionPtr expression = ExpressionParser::parse(nullptr, "needsNumber(<<abc>>)", registry);
            EXPECT_THROW(static_cast<void>(expression->evaluate()), Base::TypeError);
        }

        /**
         * @brief 钉住：自定义函数结果能按分量取子值
         */
        TEST(FunctionRegistryTest, ResultAcceptsComponentAccess)
        {
            FunctionRegistry registry;
            ASSERT_TRUE(registry
                                .registerFunction(makeSpec(
                                        "makeVector", [](const FunctionCall &call) { return Value{Base::Vector3d(toDouble(call.argumentValue(0), "x"), toDouble(call.argumentValue(1), "y"), toDouble(call.argumentValue(2), "z"))}; }, 3, 3))
                                .has_value());

            const ExpressionPtr expression = ExpressionParser::parse(nullptr, "makeVector(1; 2; 3)[1]", registry);
            const Value         value      = expression->evaluate();
            const auto         *number     = std::get_if<double>(&value);
            ASSERT_NE(number, nullptr) << "向量下标应取到纯数分量，实际是" << valueTypeName(value);
            EXPECT_DOUBLE_EQ(*number, 2.0);

            // 不存在的分量按值语义明确报错
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(nullptr, "makeVector(1; 2; 3).x", registry)->evaluate()), Base::Exception);
        }

        /**
         * @brief 钉住：文本往返、拷贝与结构相等
         */
        TEST(FunctionRegistryTest, TextRoundTripCopyAndEquality)
        {
            FunctionRegistry registry;
            ASSERT_TRUE(registry.registerFunction(makeSpec("mix", [](const FunctionCall &call) { return call.argumentValue(0); }, 2, 2)).has_value());

            const ExpressionPtr first   = ExpressionParser::parse(nullptr, "mix(1 + 2; 3 mm)", registry);
            const std::string   printed = first->toString(true, true);
            EXPECT_NE(printed.find("mix("), std::string::npos) << printed;

            const ExpressionPtr reparsed = ExpressionParser::parse(nullptr, printed, registry);
            EXPECT_TRUE(first->isSame(*reparsed));

            const ExpressionPtr copied = first->copy();
            EXPECT_TRUE(first->isSame(*copied));
            EXPECT_NE(copied.get(), first.get());
            EXPECT_DOUBLE_EQ(quantityValueOf(*copied), quantityValueOf(*first));

            // 同名不同实现仍是同一棵树：结构相等按文本判定，实现替换由注销/再登记表达
            FunctionRegistry other;
            ASSERT_TRUE(other.registerFunction(makeSpec("mix", [](const FunctionCall &call) { return call.argumentValue(1); }, 2, 2)).has_value());
            const ExpressionPtr fromOther = ExpressionParser::parse(nullptr, printed, other);
            EXPECT_TRUE(first->isSame(*fromOther));
            EXPECT_DOUBLE_EQ(quantityValueOf(*fromOther), 3.0);
        }

        /**
         * @brief 钉住：实参全常量时化简折叠成常量节点，含宿主引用时保留结构
         */
        TEST(FunctionRegistryTest, SimplifyFoldsOnlyConstantArguments)
        {
            FunctionRegistry registry;
            ASSERT_TRUE(registry.registerFunction(makeSpec("triple", [](const FunctionCall &call) { return Units::Quantity(3.0) * toQuantity(call.argumentValue(0), "x"); }, 1, 1)).has_value());

            const ExpressionPtr folded = ExpressionParser::parse(nullptr, "triple(1 + 1)", registry)->simplify();
            EXPECT_NE(folded->nodeName(), "CustomFunction");
            EXPECT_DOUBLE_EQ(quantityValueOf(*folded), 6.0);

            FakeResolver        resolver;
            const ExpressionPtr kept = ExpressionParser::parse(&resolver, "triple(Box.Length)", registry)->simplify();
            EXPECT_EQ(kept->nodeName(), "CustomFunction");
            EXPECT_DOUBLE_EQ(quantityValueOf(*kept), 9.0);
        }

        /**
         * @brief 钉住：登记与查询可并发调用，结果与单线程一致
         */
        TEST(FunctionRegistryTest, RegistrationAndLookupAreThreadSafe)
        {
            FunctionRegistry         registry;
            constexpr int            workerCount = 4;
            std::vector<std::thread> workers;
            workers.reserve(static_cast<std::size_t>(workerCount) * 2);

            for (int workerIndex = 0; workerIndex < workerCount; ++workerIndex)
            {
                workers.emplace_back(
                        [&registry, workerIndex]
                        {
                            for (int round = 0; round < 25; ++round)
                            {
                                static_cast<void>(registry.registerFunction(makeSpec(std::format("fn_{}_{}", workerIndex, round), [](const FunctionCall &) { return Value{1.0}; }, 0)));
                            }
                        });
                workers.emplace_back(
                        [&registry, workerIndex]
                        {
                            // 只读侧：与写侧交错执行，验证共享锁不下到坏数据
                            for (int round = 0; round < 25; ++round)
                            {
                                static_cast<void>(registry.contains(std::format("fn_{}_{}", workerIndex, round)));
                                static_cast<void>(registry.names());
                            }
                        });
            }
            for (std::thread &worker: workers)
            {
                worker.join();
            }

            EXPECT_EQ(registry.size(), static_cast<std::size_t>(workerCount * 25));
            EXPECT_EQ(registry.names().size(), static_cast<std::size_t>(workerCount * 25));
        }

    } // namespace
} // namespace ExpressionEngine::Expression
