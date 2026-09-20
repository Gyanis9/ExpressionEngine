// 本文件钉住内置字典宿主：不起宿主对象模型时，名字、两级引用与写回怎么走。

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <variant>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Expression/Dictionary.h>
#include <ExpressionEngine/Expression/ExpressionParser.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Expression
{
    namespace
    {

        /// 解析并求值，返回数量；异常与非数量结果都由用例报告，便于定位是哪一步走偏
        Units::Quantity quantityOf(Dictionary &dictionary, const std::string &text)
        {
            try
            {
                const ExpressionPtr expression    = ExpressionParser::parse(&dictionary, text);
                const Value         value         = expression->evaluate();
                const auto         *quantityValue = std::get_if<Units::Quantity>(&value);
                if (quantityValue == nullptr)
                {
                    ADD_FAILURE() << "表达式 " << text << " 的结果不是数量，而是" << valueTypeName(value);
                    return {};
                }
                return *quantityValue;
            } catch (const Base::Exception &error)
            {
                ADD_FAILURE() << "表达式 " << text << " 抛出 " << error.message();
                return {};
            }
        }

        /**
         * @brief 钉住：裸名字直接求值，不需要宿主实现任何接口
         */
        TEST(DictionaryTest, BareNameEvaluatesWithoutHostInterfaces)
        {
            Dictionary dictionary;
            dictionary.define("Length", Units::Quantity(3.0, Units::Unit::Length));

            EXPECT_DOUBLE_EQ(quantityOf(dictionary, "Length * 2").getValue(), 6.0);
            EXPECT_DOUBLE_EQ(quantityOf(dictionary, "Length + 1 mm").getValue(), 4.0);
            // 当前对象限定写法走同一条路径
            EXPECT_DOUBLE_EQ(quantityOf(dictionary, ".Length").getValue(), 3.0);
        }

        /**
         * @brief 钉住：挂子字典后两级引用也能解析，读到的值来自子字典
         */
        TEST(DictionaryTest, ChildDictionaryServesTwoSegmentReferences)
        {
            Dictionary  dictionary;
            Dictionary &box = dictionary.addObject("Box", std::make_unique<Dictionary>());
            box.define("Length", Units::Quantity(4.0, Units::Unit::Length));

            EXPECT_DOUBLE_EQ(quantityOf(dictionary, "Box.Length + 1 mm").getValue(), 5.0);
            EXPECT_EQ(box.name(), "Box");
            EXPECT_NE(dictionary.findObject("Box"), nullptr);
            EXPECT_EQ(dictionary.findObject("Sphere"), nullptr);
            // 对象名与属性名分别由解析器与字典负责，报错口径与宿主实现一致
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(&dictionary, "Sphere.Length")->evaluate()), Base::NameError);
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(&dictionary, "Box.Width")->evaluate()), Base::AttributeError);
        }

        /**
         * @brief 钉住：字典条目可写回，只读条目拒绝写入
         */
        TEST(DictionaryTest, EntriesAcceptAssignmentUntilMarkedReadOnly)
        {
            Dictionary dictionary;
            dictionary.define("Length", Units::Quantity(1.0, Units::Unit::Length));

            const ExpressionPtr target   = ExpressionParser::parse(&dictionary, "Length");
            const auto         *variable = dynamic_cast<const VariableExpression *>(target.get());
            ASSERT_NE(variable, nullptr);
            static_cast<void>(variable->assignValue(Value(Units::Quantity(7.0, Units::Unit::Length))));
            EXPECT_DOUBLE_EQ(quantityOf(dictionary, "Length").getValue(), 7.0);

            dictionary.find("Length")->setReadOnly(true);
            EXPECT_THROW(static_cast<void>(variable->assignValue(Value(Units::Quantity(9.0, Units::Unit::Length)))), Base::AttributeError);
            EXPECT_DOUBLE_EQ(quantityOf(dictionary, "Length").getValue(), 7.0);
        }

        /**
         * @brief 钉住：重新 define 只覆盖取值，不放开已设的只读标记
         */
        TEST(DictionaryTest, RedefineKeepsReadOnlyFlag)
        {
            Dictionary dictionary;
            dictionary.define("Length", Units::Quantity(1.0, Units::Unit::Length)).setReadOnly(true);
            dictionary.define("Length", Units::Quantity(2.0, Units::Unit::Length));

            EXPECT_DOUBLE_EQ(quantityOf(dictionary, "Length").getValue(), 2.0);
            const ExpressionPtr target   = ExpressionParser::parse(&dictionary, "Length");
            const auto         *variable = dynamic_cast<const VariableExpression *>(target.get());
            ASSERT_NE(variable, nullptr);
            EXPECT_THROW(static_cast<void>(variable->assignValue(Value(3.0))), Base::AttributeError);
        }

        /**
         * @brief 钉住：常量不需要单独机制，定义一个只读名字即可
         */
        TEST(DictionaryTest, NamedValuesServeAsConstants)
        {
            Dictionary dictionary;
            dictionary.define("wallThickness", Units::Quantity(3.0)).setReadOnly(true);

            EXPECT_DOUBLE_EQ(quantityOf(dictionary, "wallThickness * 2").getValue(), 6.0);
            // 代价是常量也会被当作引用收集，宿主建依赖时会看到它
            const std::vector<VariableReference> references = ExpressionParser::parse(&dictionary, "wallThickness * 2")->collectReferences();
            ASSERT_EQ(references.size(), 1U);
            EXPECT_EQ(references.front().propertyName, "wallThickness");
        }

        /**
         * @brief 钉住：引擎内置常量按词法识别，字典里的同名条目不能把它换掉
         */
        TEST(DictionaryTest, EngineConstantsCannotBeShadowed)
        {
            Dictionary dictionary;
            dictionary.define("pi", Units::Quantity(3.0));
            dictionary.define("True", false);

            // pi 与 True 在词法阶段就成常量节点，压根不会走到字典
            EXPECT_NEAR(quantityOf(dictionary, "pi").getValue(), 3.14159265358979, 1e-12);
            const Value trueValue = ExpressionParser::parse(&dictionary, "True")->evaluate();
            ASSERT_TRUE(std::holds_alternative<bool>(trueValue));
            EXPECT_TRUE(std::get<bool>(trueValue));
        }

        /**
         * @brief 钉住：文本与布尔条目按各自的取值类型参与求值
         */
        TEST(DictionaryTest, TextAndBooleanEntriesKeepTheirTypes)
        {
            Dictionary dictionary;
            dictionary.define("Greeting", std::string("hi"));
            dictionary.define("Flag", true);
            dictionary.define("Width", Units::Quantity(2.0, Units::Unit::Length));

            const Value greeting = ExpressionParser::parse(&dictionary, "Greeting")->evaluate();
            EXPECT_EQ(std::get<std::string>(greeting), "hi");
            EXPECT_DOUBLE_EQ(quantityOf(dictionary, "Flag ? Width : 10 mm").getValue(), 2.0);
            EXPECT_DOUBLE_EQ(quantityOf(dictionary, "not(Flag) ? Width : 10 mm").getValue(), 10.0);
        }

        /**
         * @brief 钉住：属性名与对象名列表按字典序给出，供报错提示使用
         */
        TEST(DictionaryTest, NameListingsFeedDiagnostics)
        {
            Dictionary dictionary;
            dictionary.define("Width", Units::Quantity(1.0));
            dictionary.define("Length", Units::Quantity(1.0));
            dictionary.addObject("Box", std::make_unique<Dictionary>());
            dictionary.addObject("Part", std::make_unique<Dictionary>());

            EXPECT_EQ(dictionary.propertyNames(), (std::vector<std::string>{"Length", "Width"}));
            EXPECT_EQ(dictionary.objectNames({}), (std::vector<std::string>{"Box", "Part"}));

            try
            {
                static_cast<void>(ExpressionParser::parse(&dictionary, "NoSuchName")->evaluate());
                FAIL() << "未定义的名字应当报错";
            } catch (const Base::AttributeError &error)
            {
                // 报错里列出可用属性，用户不必翻文档猜名字
                EXPECT_NE(std::string(error.message()).find("Length"), std::string::npos) << error.message();
                EXPECT_NE(std::string(error.message()).find("Width"), std::string::npos) << error.message();
            }
        }

        /**
         * @brief 钉住：条目的地址在字典存活期间稳定，宿主可长期持有引用
         */
        TEST(DictionaryTest, EntryReferencesStayValidAcrossDefinitions)
        {
            Dictionary         dictionary;
            Dictionary::Entry &first = dictionary.define("Length", Units::Quantity(1.0));
            dictionary.define("Width", Units::Quantity(2.0));
            dictionary.define("Height", Units::Quantity(3.0));
            dictionary.define("Length", Units::Quantity(5.0));

            EXPECT_EQ(&first, dictionary.find("Length"));
            EXPECT_DOUBLE_EQ(std::get<Units::Quantity>(*first.value()).getValue(), 5.0);
            EXPECT_EQ(dictionary.find("Missing"), nullptr);
            EXPECT_TRUE(dictionary.contains("Width"));
        }

        /**
         * @brief 钉住：删除与清空
         */
        TEST(DictionaryTest, EraseAndClearDropNames)
        {
            Dictionary dictionary;
            dictionary.define("Length", Units::Quantity(3.0, Units::Unit::Length));
            dictionary.define("Width", Units::Quantity(2.0));

            EXPECT_TRUE(dictionary.erase("Width"));
            EXPECT_FALSE(dictionary.erase("Width"));
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(&dictionary, "Width")->evaluate()), Base::AttributeError);

            dictionary.clear();
            EXPECT_TRUE(dictionary.propertyNames().empty());
            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(&dictionary, "Length")->evaluate()), Base::AttributeError);
        }

        /**
         * @brief 钉住：字典不区分文档，带文档名的引用交回宿主解析器处理
         */
        TEST(DictionaryTest, DocumentQualifiedReferencesAreNotResolved)
        {
            Dictionary dictionary;
            dictionary.define("Length", Units::Quantity(3.0, Units::Unit::Length));
            dictionary.addObject("Box", std::make_unique<Dictionary>());

            EXPECT_THROW(static_cast<void>(ExpressionParser::parse(&dictionary, "<<Doc>>.Box.Length")->evaluate()), Base::NameError);
            // 当前对象的约定：空文档名与空对象名解析到字典自身
            EXPECT_EQ(dictionary.resolve({}, {}), &dictionary);
            EXPECT_EQ(dictionary.resolve("Doc", {}), nullptr);
        }

    } // namespace
} // namespace ExpressionEngine::Expression
