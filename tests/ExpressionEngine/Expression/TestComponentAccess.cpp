// 本文件覆盖分量取值与依赖收集：applyComponent/applyRangeComponent 的取值与拒绝面，
// 以及 Expression::collectReferences 的收集顺序与去重。

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Vector3D.h>
#include <ExpressionEngine/Expression/ComponentAccess.h>
#include <ExpressionEngine/Expression/Expression.h>
#include <ExpressionEngine/Expression/Value.h>
#include <ExpressionEngine/Units/Quantity.h>

namespace ExpressionEngine::Expression
{
    namespace
    {

        /// 造一个整数下标表达式；空值表示分量里没有这一段
        ExpressionPtr makeIndexExpression(std::optional<double> value)
        {
            if (!value.has_value())
            {
                return nullptr;
            }
            return std::make_unique<NumberExpression>(nullptr, Units::Quantity(*value));
        }

        /// 造一个 Index 分量
        Expression::Component makeIndexComponent(double index)
        {
            return Expression::Component::arrayIndex(makeIndexExpression(index));
        }

        /// 造一个区间分量；端点与步长都可缺省，缺省表示开放端或默认步长 1
        Expression::Component makeRangeComponent(std::optional<double> begin, std::optional<double> end, std::optional<double> step = std::nullopt)
        {
            return Expression::Component::rangeComponent(makeIndexExpression(begin), makeIndexExpression(end), makeIndexExpression(step));
        }

        /// 取出纯数；不是纯数时让用例失败并给出实际类型
        double doubleOf(const Value &value)
        {
            const auto *number = std::get_if<double>(&value);
            if (number == nullptr)
            {
                ADD_FAILURE() << "期望纯数，实际是 " << std::string(valueTypeName(value));
                return 0.0;
            }
            return *number;
        }

        /// 造一个变量引用节点；用例只关心结构，不接解析器
        ExpressionPtr makeVariableReference(std::string documentName, std::string objectName, std::string propertyName)
        {
            return std::make_unique<VariableExpression>(
                    nullptr, VariableExpression::Reference{.documentName = std::move(documentName), .objectName = std::move(objectName), .propertyName = std::move(propertyName)});
        }

    } // namespace

    /**
     * @brief 钉住：向量按分量下标取值，负下标从末尾计数
     */
    TEST(ComponentAccessTest, IndexSelectsVectorComponent)
    {
        const Value vector(Base::Vector3d(1.0, 2.0, 3.0));
        EXPECT_DOUBLE_EQ(doubleOf(applyComponent(vector, makeIndexComponent(0.0), "测试")), 1.0);
        EXPECT_DOUBLE_EQ(doubleOf(applyComponent(vector, makeIndexComponent(1.0), "测试")), 2.0);
        EXPECT_DOUBLE_EQ(doubleOf(applyComponent(vector, makeIndexComponent(2.0), "测试")), 3.0);
        EXPECT_DOUBLE_EQ(doubleOf(applyComponent(vector, makeIndexComponent(-1.0), "测试")), 3.0);
        EXPECT_DOUBLE_EQ(doubleOf(applyComponent(vector, makeIndexComponent(-3.0), "测试")), 1.0);
    }

    /**
     * @brief 钉住：文本下标按 UTF-8 字符切分，多字节字符不会被拆成半个
     */
    TEST(ComponentAccessTest, IndexSelectsTextCharacter)
    {
        const Value text(std::string("a中b"));
        EXPECT_EQ(std::get<std::string>(applyComponent(text, makeIndexComponent(0.0), "测试")), "a");
        EXPECT_EQ(std::get<std::string>(applyComponent(text, makeIndexComponent(1.0), "测试")), "中");
        EXPECT_EQ(std::get<std::string>(applyComponent(text, makeIndexComponent(2.0), "测试")), "b");
        EXPECT_EQ(std::get<std::string>(applyComponent(text, makeIndexComponent(-1.0), "测试")), "b");
    }

    /**
     * @brief 钉住：下标越界报 IndexError，非整数下标报 ValueError
     */
    TEST(ComponentAccessTest, IndexOutOfRangeIsRejected)
    {
        const Value vector(Base::Vector3d(1.0, 2.0, 3.0));
        EXPECT_THROW(static_cast<void>(applyComponent(vector, makeIndexComponent(3.0), "测试")), Base::IndexError);
        EXPECT_THROW(static_cast<void>(applyComponent(vector, makeIndexComponent(-4.0), "测试")), Base::IndexError);

        const Value text(std::string("ab"));
        EXPECT_THROW(static_cast<void>(applyComponent(text, makeIndexComponent(2.0), "测试")), Base::IndexError);
        EXPECT_THROW(static_cast<void>(applyComponent(text, makeIndexComponent(-3.0), "测试")), Base::IndexError);

        EXPECT_THROW(static_cast<void>(applyComponent(vector, makeIndexComponent(1.5), "测试")), Base::ValueError);
    }

    /**
     * @brief 钉住：数量、布尔等不支持下标的类型报 TypeError
     */
    TEST(ComponentAccessTest, IndexOnUnsupportedTypeIsRejected)
    {
        const Value quantity(Units::Quantity(2.0, Units::Unit::Length));
        EXPECT_THROW(static_cast<void>(applyComponent(quantity, makeIndexComponent(0.0), "测试")), Base::TypeError);
        EXPECT_THROW(static_cast<void>(applyComponent(Value(true), makeIndexComponent(0.0), "测试")), Base::TypeError);
        EXPECT_THROW(static_cast<void>(applyComponent(Value(2.5), makeIndexComponent(0.0), "测试")), Base::TypeError);
    }

    /**
     * @brief 钉住：映射键与名字分量在值层面报错，文案给出替代做法
     */
    TEST(ComponentAccessTest, MapKeyAndNameComponentsAreRejectedAtValueLevel)
    {
        const Value                 vector(Base::Vector3d(1.0, 2.0, 3.0));
        const Expression::Component key = Expression::Component::mapKey("k");
        EXPECT_THROW(static_cast<void>(applyComponent(vector, key, "测试")), Base::TypeError);
        EXPECT_THROW(static_cast<void>(applyComponent(Value(std::string("abc")), key, "测试")), Base::TypeError);
        try
        {
            static_cast<void>(applyComponent(vector, key, "测试"));
            FAIL() << "映射键分量在值层面应当报错";
        } catch (const Base::TypeError &error)
        {
            // 文案要点名值模型没有映射类型，指引调用方改用属性路径或下标
            EXPECT_NE(std::string(error.message()).find("映射类型"), std::string::npos);
        }

        const Expression::Component name("Rotation");
        EXPECT_THROW(static_cast<void>(applyComponent(vector, name, "测试")), Base::AttributeError);
        try
        {
            static_cast<void>(applyComponent(vector, name, "测试"));
            FAIL() << "名字分量在值层面应当报错";
        } catch (const Base::AttributeError &error)
        {
            // 文案要给出替代做法
            EXPECT_NE(std::string(error.message()).find("下标"), std::string::npos);
        }
    }

    /**
     * @brief 钉住：区间分量不能按单值取用，报错并提示只能作为聚合函数的实参
     */
    TEST(ComponentAccessTest, RangeComponentInSingleValuePathIsRejected)
    {
        const Value vector(Base::Vector3d(1.0, 2.0, 3.0));
        EXPECT_THROW(static_cast<void>(applyComponent(vector, makeRangeComponent(0.0, 1.0), "测试")), EvaluationError);
        try
        {
            static_cast<void>(applyComponent(vector, makeRangeComponent(0.0, 1.0), "测试"));
            FAIL() << "区间分量不应按单值取用";
        } catch (const EvaluationError &error)
        {
            EXPECT_NE(std::string(error.message()).find("聚合函数"), std::string::npos);
        }
    }

    /**
     * @brief 钉住：区间两端都算在结果里，并支持步长与负步长
     */
    TEST(ComponentAccessTest, RangeSelectsVectorComponents)
    {
        const Value vector(Base::Vector3d(1.0, 2.0, 3.0));

        const std::vector<Value> full = applyRangeComponent(vector, makeRangeComponent(0.0, 2.0), "测试");
        ASSERT_EQ(full.size(), std::size_t(3));
        EXPECT_DOUBLE_EQ(doubleOf(full[0]), 1.0);
        EXPECT_DOUBLE_EQ(doubleOf(full[1]), 2.0);
        EXPECT_DOUBLE_EQ(doubleOf(full[2]), 3.0);

        // [0:1] 含右端，取 x 与 y
        const std::vector<Value> pair = applyRangeComponent(vector, makeRangeComponent(0.0, 1.0), "测试");
        ASSERT_EQ(pair.size(), std::size_t(2));
        EXPECT_DOUBLE_EQ(doubleOf(pair[0]), 1.0);
        EXPECT_DOUBLE_EQ(doubleOf(pair[1]), 2.0);

        // 步长 2 隔一个取一个
        const std::vector<Value> spaced = applyRangeComponent(vector, makeRangeComponent(0.0, 2.0, 2.0), "测试");
        ASSERT_EQ(spaced.size(), std::size_t(2));
        EXPECT_DOUBLE_EQ(doubleOf(spaced[0]), 1.0);
        EXPECT_DOUBLE_EQ(doubleOf(spaced[1]), 3.0);

        // 负步长反向取值
        const std::vector<Value> reversed = applyRangeComponent(vector, makeRangeComponent(2.0, 0.0, -1.0), "测试");
        ASSERT_EQ(reversed.size(), std::size_t(3));
        EXPECT_DOUBLE_EQ(doubleOf(reversed[0]), 3.0);
        EXPECT_DOUBLE_EQ(doubleOf(reversed[1]), 2.0);
        EXPECT_DOUBLE_EQ(doubleOf(reversed[2]), 1.0);
    }

    /**
     * @brief 钉住：开放区间按边界含义取到开头或末尾
     */
    TEST(ComponentAccessTest, RangeOpenEndsUseBoundaries)
    {
        const Value vector(Base::Vector3d(1.0, 2.0, 3.0));

        // 起点缺省按 0 计
        const std::vector<Value> fromStart = applyRangeComponent(vector, makeRangeComponent(std::nullopt, 1.0), "测试");
        ASSERT_EQ(fromStart.size(), std::size_t(2));
        EXPECT_DOUBLE_EQ(doubleOf(fromStart[0]), 1.0);
        EXPECT_DOUBLE_EQ(doubleOf(fromStart[1]), 2.0);

        // 终点缺省按末位计
        const std::vector<Value> toEnd = applyRangeComponent(vector, makeRangeComponent(1.0, std::nullopt), "测试");
        ASSERT_EQ(toEnd.size(), std::size_t(2));
        EXPECT_DOUBLE_EQ(doubleOf(toEnd[0]), 2.0);
        EXPECT_DOUBLE_EQ(doubleOf(toEnd[1]), 3.0);
    }

    /**
     * @brief 钉住：区间越界与步长为 0 报 IndexError，非向量值报 TypeError
     */
    TEST(ComponentAccessTest, RangeRejections)
    {
        const Value vector(Base::Vector3d(1.0, 2.0, 3.0));
        EXPECT_THROW(static_cast<void>(applyRangeComponent(vector, makeRangeComponent(0.0, 3.0), "测试")), Base::IndexError);
        EXPECT_THROW(static_cast<void>(applyRangeComponent(vector, makeRangeComponent(3.0, 2.0), "测试")), Base::IndexError);
        EXPECT_THROW(static_cast<void>(applyRangeComponent(vector, makeRangeComponent(0.0, 2.0, 0.0), "测试")), Base::IndexError);

        EXPECT_THROW(static_cast<void>(applyRangeComponent(Value(std::string("abc")), makeRangeComponent(0.0, 1.0), "测试")), Base::TypeError);
        EXPECT_THROW(static_cast<void>(applyRangeComponent(Value(1.0), makeRangeComponent(0.0, 1.0), "测试")), Base::TypeError);
    }

    /**
     * @brief 钉住：Box.Length + 2 * Other.Width 按首次出现顺序收出两个引用
     */
    TEST(ComponentAccessTest, CollectReferencesKeepsFirstAppearanceOrder)
    {
        auto sum = std::make_unique<OperatorExpression>(nullptr, makeVariableReference("", "Box", "Length"), OperatorExpression::Operator::Add,
                                                        std::make_unique<OperatorExpression>(nullptr, std::make_unique<NumberExpression>(nullptr, Units::Quantity(2.0)),
                                                                                             OperatorExpression::Operator::Multiply, makeVariableReference("", "Other", "Width")));

        const std::vector<VariableReference> references = sum->collectReferences();
        ASSERT_EQ(references.size(), std::size_t(2));
        EXPECT_EQ(references[0].objectName, "Box");
        EXPECT_EQ(references[0].propertyName, "Length");
        EXPECT_EQ(references[1].objectName, "Other");
        EXPECT_EQ(references[1].propertyName, "Width");
    }

    /**
     * @brief 钉住：重复引用只保留首次出现，条件与函数子节点同样被递归收集
     */
    TEST(ComponentAccessTest, CollectReferencesDeduplicatesAndRecurses)
    {
        auto product = std::make_unique<OperatorExpression>(nullptr, makeVariableReference("", "Box", "Length"), OperatorExpression::Operator::Multiply,
                                                            makeVariableReference("", "Box", "Length"));
        const std::vector<VariableReference> references = product->collectReferences();
        ASSERT_EQ(references.size(), std::size_t(1));
        EXPECT_EQ(references[0].propertyName, "Length");

        // 同对象同属性但文档不同，是两条独立依赖
        auto qualified = std::make_unique<OperatorExpression>(nullptr, makeVariableReference("", "Box", "Length"), OperatorExpression::Operator::Add,
                                                              makeVariableReference("Doc", "Box", "Length"));
        EXPECT_EQ(qualified->collectReferences().size(), std::size_t(2));

        // 条件节点的条件与两个分支都会被收集
        auto conditional = std::make_unique<ConditionalExpression>(nullptr, makeVariableReference("", "Box", "UseTop"), makeVariableReference("", "Box", "Top"),
                                                                   makeVariableReference("", "Box", "Bottom"));
        EXPECT_EQ(conditional->collectReferences().size(), std::size_t(3));

        // 函数实参里的引用同样被递归收集
        std::vector<ExpressionPtr> arguments;
        arguments.push_back(makeVariableReference("", "Box", "Length"));
        auto                                 call = std::make_unique<FunctionExpression>(nullptr, FunctionExpression::Function::Sine, std::string("sin"), std::move(arguments));
        const std::vector<VariableReference> callReferences = call->collectReferences();
        ASSERT_EQ(callReferences.size(), std::size_t(1));
        EXPECT_EQ(callReferences[0].propertyName, "Length");

        // 常量表达式没有引用
        auto constant = std::make_unique<NumberExpression>(nullptr, Units::Quantity(3.0));
        EXPECT_TRUE(constant->collectReferences().empty());
    }

} // namespace ExpressionEngine::Expression
