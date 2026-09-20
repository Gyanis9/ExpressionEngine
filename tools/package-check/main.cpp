// 安装包消费者检查：只用 find_package 拿到的公开头文件与导入目标，断言宿主会感觉到的行为。
// 库自己的用例跑在源码树内，看不见「导出头没随包发布」「安装规则漏了目录」这类接线缺陷；
// 中文注释的导出头还会让不带 /utf-8 的 MSVC 消费者直接编译失败。本目录就是那道门。

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Expression/Dictionary.h>
#include <ExpressionEngine/Expression/Expression.h>
#include <ExpressionEngine/Expression/ExpressionParser.h>
#include <ExpressionEngine/Expression/FunctionRegistry.h>
#include <ExpressionEngine/Expression/Value.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/QuantityParser.h>
#include <ExpressionEngine/Units/Unit.h>
#include <ExpressionEngine/Units/UnitsApi.h>
#include <ExpressionEngine/Units/UnitsSchema.h>
#include <ExpressionEngine/Units/UnitsSchemas.h>
#include <ExpressionEngine/Units/UnitsSchemasData.h>

namespace
{
    using ExpressionEngine::Base::NameError;
    using ExpressionEngine::Base::ValueError;
    using ExpressionEngine::Expression::CustomFunctionSpec;
    using ExpressionEngine::Expression::Dictionary;
    using ExpressionEngine::Expression::ExpressionParser;
    using ExpressionEngine::Expression::ExpressionPtr;
    using ExpressionEngine::Expression::FunctionCall;
    using ExpressionEngine::Expression::FunctionRegistry;
    using ExpressionEngine::Expression::IObjectResolver;
    using ExpressionEngine::Expression::Value;
    using ExpressionEngine::Expression::valuesEqual;
    using ExpressionEngine::Units::Quantity;
    using ExpressionEngine::Units::QuantityFormat;
    using ExpressionEngine::Units::QuantityParser;
    using ExpressionEngine::Units::Unit;
    using ExpressionEngine::Units::UnitsApi;
    using ExpressionEngine::Units::UnitsSchema;
    using ExpressionEngine::Units::UnitsSchemaSpecification;
    using ExpressionEngine::Units::UnitsSchemas;
    using ExpressionEngine::Units::UnitsSchemasDataPack;
    using ExpressionEngine::Units::UnitTranslationSpecification;

    int checks = 0;
    int failed = 0;

    /// 记一条断言：条件为假时打印标签，末尾按失败数决定退出码
    void check(const bool condition, const std::string &label)
    {
        ++checks;
        if (condition)
        {
            return;
        }

        ++failed;
        std::cout << "FAIL: " << label << '\n';
    }

    /// 解析并取值，任一环节失败就返回空值
    std::optional<Value> evaluate(const std::string &text, IObjectResolver *resolver = nullptr)
    {
        const auto parsed = ExpressionParser::tryParse(resolver, text);
        if (!parsed.has_value())
        {
            return {};
        }

        const auto value = (*parsed)->tryEvaluate();
        if (!value.has_value())
        {
            return {};
        }

        return *value;
    }
} // namespace

namespace
{
    void runExpressionChecks()
    {
        const auto sum   = evaluate("sum(list(1; 2))");
        const auto three = evaluate("3");
        check(sum.has_value() && three.has_value() && valuesEqual(*sum, *three), "sum(list(1; 2)) 等于 3");

        const auto lengths = evaluate("sum(split(<<1 mm; 2 mm>>; <<; >>))");
        const auto metres  = evaluate("3 mm");
        check(lengths.has_value() && metres.has_value() && valuesEqual(*lengths, *metres), "可解析成数量的文本参与运算");

        // 分量链随化简一起带走：取值不变，回写文本还能解析回同一取值
        const auto parsed = ExpressionParser::tryParse(nullptr, "split(<<a,b,c>>; <<,>>)[1]");
        check(parsed.has_value(), "解析带分量的表达式");
        if (parsed.has_value())
        {
            const ExpressionPtr simplified = (*parsed)->simplify();
            const auto          original   = (*parsed)->tryEvaluate();
            const auto          folded     = simplified->tryEvaluate();
            check(original.has_value() && folded.has_value() && valuesEqual(*original, *folded), "化简后取值不变");

            const std::string text     = simplified->toString(true);
            const auto        reparsed = evaluate(text);
            check(reparsed.has_value() && original.has_value() && valuesEqual(*original, *reparsed), "化简文本可往返取值");
        }

        check(!ExpressionParser::tryParse(nullptr, "1 +").has_value(), "坏文本走非异常通道报错");

        // 文本函数按 UTF-8 字符计，不做 Unicode 大小写映射（非 ASCII 字母原样留着）
        const auto folded = evaluate("upper(<<aB1>>)");
        check(folded.has_value() && std::holds_alternative<std::string>(*folded) && std::get<std::string>(*folded) == "AB1", "upper 只做 ASCII 大小写折叠");

        const auto characterCount = evaluate("len(<<a中b>>)");
        const auto countOfThree   = evaluate("3");
        check(characterCount.has_value() && countOfThree.has_value() && valuesEqual(*characterCount, *countOfThree), "len 按 UTF-8 字符计数");
    }

    void runHostInjectionChecks()
    {
        Dictionary dictionary;
        dictionary.define("Length", Value{Quantity(2.0, Unit::Length)});

        const auto readBack = evaluate("Length + 1 mm", &dictionary);
        check(readBack.has_value() && valuesEqual(*readBack, Quantity(3.0, Unit::Length)), "字典喂变量");

        check(dictionary.contains("Length"), "字典能查到已定义的名字");

        FunctionRegistry registry;
        CustomFunctionSpec spec;
        spec.name          = "fromHost";
        spec.usage         = "fromHost(x) 原样返回一段宿主文本";
        spec.minArguments  = 1;
        spec.maxArguments  = 1;
        spec.function      = [](const FunctionCall &call) -> Value
        {
            static_cast<void>(call.argumentValue(0));
            return Value{std::string{"from-host"}};
        };
        check(registry.registerFunction(std::move(spec)).has_value(), "宿主注册自定义函数");

        const auto parsed = ExpressionParser::tryParse(nullptr, "fromHost(4)", registry);
        check(parsed.has_value(), "按次传入宿主注册表");
        if (parsed.has_value())
        {
            const auto value = (*parsed)->tryEvaluate();
            check(value.has_value() && std::holds_alternative<std::string>(*value) && std::get<std::string>(*value) == "from-host", "自定义函数的取值来自宿主回调");
        }
    }

    void runUnitsChecks()
    {
        const auto parsedQuantity = QuantityParser::tryParse("2 m");
        check(parsedQuantity.has_value() && parsedQuantity->getValue() == 2000.0, "数量文本按基准单位毫米取值");

        UnitsApi::setSchema("Internal");
        UnitsApi::setDecimals(2);
        check(UnitsApi::schemaTranslate(Quantity(100.0, Unit::Length)) == "100.00 mm", "内置方案按 mm 排版");

        // 建筑/土木英制的角度走度分秒：负角按绝对值拆分再补符号
        UnitsApi::setSchema("ImperialCivil");
        check(UnitsApi::schemaTranslate(Quantity(-1.5, Unit::Angle)) == "-1°30′", "度分秒对负角取绝对值");

        UnitsApi::setSchema("Internal");
    }

    /**
     * @brief 宿主自带方案数据的通路：只用手工构造的数据包，不读内置表
     */
    void runCustomSchemaChecks()
    {
        const auto lengthSchema = [](const std::size_t number, const std::string &name, std::vector<UnitTranslationSpecification> rows)
        {
            UnitsSchemaSpecification specification;
            specification.number                             = number;
            specification.name                               = name;
            specification.basicLengthUnitString              = "mm";
            specification.description                        = "consumer schema";
            specification.translationSpecifications["Length"] = std::move(rows);
            return specification;
        };

        const UnitsSchemasDataPack pack{
                .specifications     = {lengthSchema(4, "Delta", {{0, "mm", 1.0}}), lengthSchema(2, "Beta", {{0, "cm", 10.0}})},
                .defaultDecimals    = 3,
                .defaultDenominator = 16,
        };

        UnitsSchemas schemas{pack};
        check(schemas.currentSchema()->getName() == "Delta", "没标记 isDefault 时取列表第一个");
        check(schemas.names() == (std::vector<std::string>{"Beta", "Delta"}), "名字表按编号排序");
        check(schemas.getDecimals() == 3U && schemas.defaultFractionDenominator() == 16U, "数据包默认精度与分母可读");

        schemas.select(2U);
        check(schemas.currentSchema()->getName() == "Beta", "按编号选中方案");

        bool rejectedWithNumber = false;
        try
        {
            static_cast<void>(schemas.specification(99U));
        }
        catch (const NameError &error)
        {
            rejectedWithNumber = error.message().find("99") != std::string::npos;
        }
        check(rejectedWithNumber, "查不到的编号报错带上编号");

        bool emptyPackRejected = false;
        try
        {
            static_cast<void>(UnitsSchemas{UnitsSchemasDataPack{{}, 2, 8}});
        }
        catch (const NameError &)
        {
            emptyPackRejected = true;
        }
        check(emptyPackRejected, "空数据包明确报错而不是回落到不存在的方案");

        // 条目自带回调接管排版：函数名未登记时不再排出一段空文本
        UnitTranslationSpecification callbackRow;
        callbackRow.threshold  = 0;
        callbackRow.unitString = "myFormat";
        callbackRow.factor     = 0.0;
        callbackRow.callback   = [](const double value) { return "<" + std::to_string(value) + ">"; };

        const UnitsSchema custom{lengthSchema(0, "Custom", {callbackRow})};
        check(custom.translate(Quantity(2.5, Unit::Length)) == "<" + std::to_string(2.5) + ">", "条目回调接管排版");

        bool unknownSpecialRejected = false;
        try
        {
            static_cast<void>(UnitsSchema{lengthSchema(0, "Custom", {{0, "toNope", 0.0}})}.translate(Quantity(1.0, Unit::Length)));
        }
        catch (const ValueError &error)
        {
            unknownSpecialRejected = error.message().find("toNope") != std::string::npos;
        }
        check(unknownSpecialRejected, "未登记的函数名报错带上名字");

        // 已登记的函数名走内置实现，回调不生效
        UnitTranslationSpecification fractionalRow;
        fractionalRow.threshold  = 0;
        fractionalRow.unitString = "toFractional";
        fractionalRow.factor     = 0.0;
        fractionalRow.callback   = [](const double) { return std::string{"callback"}; };

        Quantity       fractional{100.0, Unit::Length};
        QuantityFormat format;
        format.setPrecision(2);
        format.setDenominator(16);
        fractional.setFormat(format);
        check(UnitsSchema{lengthSchema(0, "Custom", {fractionalRow})}.translate(fractional) == "3\" + 15/16\"", "内置特殊函数优先于回调");

        // 宿主数据包整体装进门面：默认精度随之生效，之后换回内置包
        UnitsApi::applyPack(pack);
        UnitsApi::setDecimals(-1);
        UnitsApi::setDenominator(-1);
        check(UnitsApi::count() == 2U && UnitsApi::getDecimals() == 3 && UnitsApi::getDenominator() == 16, "宿主数据包能装进门面");
        check(UnitsApi::getNames() == (std::vector<std::string>{"Beta", "Delta"}), "门面的方案清单跟着数据包走");

        UnitsApi::applyPack(ExpressionEngine::Units::UnitsSchemasData::unitSchemasDataPack);
        check(UnitsApi::count() == 10U && UnitsApi::getDecimals() == 2, "换回内置数据包");
    }
} // namespace

int main()
{
    try
    {
        runExpressionChecks();
        runHostInjectionChecks();
        runUnitsChecks();
        runCustomSchemaChecks();
    }
    catch (const std::exception &error)
    {
        std::cout << "FAIL: 消费者检查抛出未预期异常: " << error.what() << '\n';
        ++failed;
    }

    std::cout << "checks=" << checks << " failed=" << failed << '\n';
    return failed == 0 ? 0 : 1;
}
