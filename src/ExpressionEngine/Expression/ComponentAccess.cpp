#include <ExpressionEngine/Expression/ComponentAccess.h>

#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Vector3D.h>
#include <ExpressionEngine/Units/Quantity.h>

namespace ExpressionEngine::Expression
{
    namespace
    {

        /// 一个 UTF-8 字符在字节序列里的位置
        struct CharacterSpan
        {
            std::size_t offset; ///< 起始字节偏移
            std::size_t length; ///< 字节长度
        };

        /**
         * @brief 定位文本里的第 index 个 UTF-8 字符
         * @details 按前导字节推断字符长度，非法或截断的尾字节按单字节处理：文本里混入非
         *          UTF-8 字节时也不会越界读，只是把坏字节当成一个字符。index 越界时返回
         *          零长度，由调用方统一报越界错。
         * @param text 文本
         * @param index 字符下标
         * @return 该字符的起始偏移与字节长度
         */
        [[nodiscard]] CharacterSpan locateCharacter(std::string_view text, std::size_t index)
        {
            std::size_t characterIndex = 0;
            std::size_t offset         = 0;
            while (offset < text.size())
            {
                const unsigned char leadByte = static_cast<unsigned char>(text[offset]);
                std::size_t         length   = 1;
                if ((leadByte & 0xE0U) == 0xC0U)
                {
                    length = 2;
                } else if ((leadByte & 0xF0U) == 0xE0U)
                {
                    length = 3;
                } else if ((leadByte & 0xF8U) == 0xF0U)
                {
                    length = 4;
                }
                if (length > text.size() - offset)
                {
                    // 尾部被截断：按剩余字节数取，保证 substr 不越界
                    length = text.size() - offset;
                }
                if (characterIndex == index)
                {
                    return CharacterSpan{offset, length};
                }
                offset += length;
                ++characterIndex;
            }
            return CharacterSpan{text.size(), 0};
        }

        /**
         * @brief 数文本里的 UTF-8 字符个数
         * @param text 文本
         * @return 字符个数；续字节（10xxxxxx）不计数
         */
        [[nodiscard]] std::size_t characterCount(std::string_view text)
        {
            std::size_t count = 0;
            for (const char byte: text)
            {
                if ((static_cast<unsigned char>(byte) & 0xC0U) != 0x80U)
                {
                    ++count;
                }
            }
            return count;
        }

        /**
         * @brief 求分量里常量下标表达式的整数值
         * @details 分量下标由解析器保证是常量表达式；这里直接求值再取整，非整数或带量纲
         *          说明调用方手工拼了非法分量，明确报错而不是截断取值。
         * @param indexExpression 下标表达式；空表示分量不完整
         * @param description 下标的名字，用于报错文案（如「区间步长」）
         * @param context 报错场景描述
         * @return 下标整数值
         * @throws Base::ParserError 下标表达式缺失
         * @throws Base::TypeError 下标带量纲
         * @throws Base::ValueError 下标不是整数
         * @throws Base::OverflowError 下标超出 long 范围
         */
        [[nodiscard]] long constantIndex(const ExpressionPtr &indexExpression, std::string_view description, std::string_view context)
        {
            if (indexExpression == nullptr)
            {
                throw Base::ParserError(std::format("{}：分量缺少{}表达式；请重新解析表达式，或补齐该分量的{}", context, description, description));
            }
            const Units::Quantity quantity = toQuantity(indexExpression->evaluate(), description);
            if (!quantity.isDimensionless())
            {
                throw Base::TypeError(std::format("{}：{}不能带量纲（当前为 {}）；请去掉单位，只写整数", context, description, toString(Value(quantity))));
            }
            const double number = quantity.getValue();
            if (number != std::floor(number))
            {
                throw Base::ValueError(std::format("{}：{}必须是整数，当前为 {}；请改成整数后再求值", context, description, number));
            }
            constexpr double longMinimum = static_cast<double>(std::numeric_limits<long>::min());
            constexpr double longMaximum = static_cast<double>(std::numeric_limits<long>::max());
            if (number < longMinimum || number > longMaximum)
            {
                throw Base::OverflowError(std::format("{}：{}超出可处理范围（当前为 {}）；请缩小下标", context, description, number));
            }
            return static_cast<long>(number);
        }

        /**
         * @brief 按整数下标取向量的分量
         * @param vector 向量
         * @param index 分量下标；负数从末尾计数，如 -1 表示 z
         * @param context 报错场景描述
         * @return 分量值
         * @throws Base::IndexError 下标越界
         */
        [[nodiscard]] Value vectorComponent(const Base::Vector3d &vector, long index, std::string_view context)
        {
            constexpr long componentCount = 3;
            const long     offset         = index < 0 ? index + componentCount : index;
            if (offset < 0 || offset >= componentCount)
            {
                throw Base::IndexError(std::format("{}：向量下标 {} 越界；向量分量下标只能是 0（x）、1（y）、2（z），"
                                                   "也支持 -1 到 -3 从末尾计数",
                                                   context, index));
            }
            return Value(vector[static_cast<unsigned short>(offset)]);
        }

        /**
         * @brief 按 Index 分量取子值
         * @param value 基值
         * @param component 下标分量
         * @param context 报错场景描述
         * @return 子值
         */
        [[nodiscard]] Value applyIndexComponent(const Value &value, const Expression::Component &component, std::string_view context)
        {
            const long index = constantIndex(component.index, "下标", context);
            if (const auto *vector = std::get_if<Base::Vector3d>(&value))
            {
                return vectorComponent(*vector, index, context);
            }
            if (const auto *text = std::get_if<std::string>(&value))
            {
                const long characters = static_cast<long>(characterCount(*text));
                const long offset     = index < 0 ? index + characters : index;
                if (offset < 0 || offset >= characters)
                {
                    throw Base::IndexError(std::format("{}：文本下标 {} 越界；文本共 {} 个字符，"
                                                       "也支持负下标从末尾计数",
                                                       context, index, characters));
                }
                const CharacterSpan span = locateCharacter(*text, static_cast<std::size_t>(offset));
                return Value(text->substr(span.offset, span.length));
            }
            throw Base::TypeError(std::format("{}：{}不支持下标分量；下标只支持向量（如 v[0] 取 x 分量）"
                                              "与文本（如 'abc'[0] 取首字符）",
                                              context, valueTypeName(value)));
        }

    } // namespace

    Value applyComponent(const Value &value, const Expression::Component &component, std::string_view context)
    {
        switch (component.kind)
        {
            case Expression::ComponentKind::Index:
                return applyIndexComponent(value, component, context);
            case Expression::ComponentKind::MapKey:
                // 值模型里没有映射类型：键是宿主自定义属性的定位方式，不是值的一部分
                throw Base::TypeError(std::format("{}：映射键分量 '{}' 在值层面无法使用——当前值模型没有映射类型，"
                                                  "键分量只能用于宿主自定义属性；请改用属性路径或下标分量",
                                                  context, component.name));
            case Expression::ComponentKind::Name:
                // 名字分量指向对象的子属性，而值是纯数据、没有属性表可查
                throw Base::AttributeError(std::format("{}：名字分量 '{}' 在值层面无法解析——它指向对象的子属性，"
                                                       "不是值的一部分；请在引用路径里补全该段（如 Box.{}.Length），"
                                                       "或改用 [下标] 分量",
                                                       context, component.name, component.name));
            case Expression::ComponentKind::Range:
                throw EvaluationError(std::format("{}：区间分量不能按单个子值取用，"
                                                  "区间分量只能作为聚合函数的实参（如 sum(v[0:2])）",
                                                  context));
        }
        // 分量种类只有上面四种，走到这里说明枚举被写坏；不做静默兜底
        throw EvaluationError(std::format("{}：未知的分量种类，无法取值", context));
    }

    std::vector<Value> applyRangeComponent(const Value &value, const Expression::Component &component, std::string_view context)
    {
        if (component.kind != Expression::ComponentKind::Range)
        {
            // 用法错误：这里只处理区间分量，单值路径请走 applyComponent
            throw std::invalid_argument("applyRangeComponent 只接受区间分量；单个下标或名字请用 applyComponent");
        }
        const auto *vector = std::get_if<Base::Vector3d>(&value);
        if (vector == nullptr)
        {
            throw Base::TypeError(std::format("{}：{}不支持区间分量；区间分量目前只支持向量（如 v[0:2]），"
                                              "其它类型请改用聚合函数或单个下标",
                                              context, valueTypeName(value)));
        }

        constexpr long componentCount = 3;
        const long     step           = component.step != nullptr ? constantIndex(component.step, "区间步长", context) : 1;
        if (step == 0)
        {
            throw Base::IndexError(std::format("{}：区间步长不能为 0；请给出非零步长，或省略步长按默认的 1 取值", context));
        }
        // 开放端取到边界：正步长从 0 数到末位，负步长方向相反
        const long defaultBegin = step > 0 ? 0 : componentCount - 1;
        const long defaultEnd   = step > 0 ? componentCount - 1 : 0;
        long       begin        = component.index != nullptr ? constantIndex(component.index, "区间起点", context) : defaultBegin;
        long       end          = component.endIndex != nullptr ? constantIndex(component.endIndex, "区间终点", context) : defaultEnd;
        if (begin < 0)
        {
            begin += componentCount;
        }
        if (end < 0)
        {
            end += componentCount;
        }
        if (begin < 0 || begin >= componentCount)
        {
            throw Base::IndexError(std::format("{}：区间起点越界（换算后为 {}）；向量分量下标只能是 "
                                               "0（x）、1（y）、2（z）",
                                               context, begin));
        }
        if (end < 0 || end >= componentCount)
        {
            throw Base::IndexError(std::format("{}：区间终点越界（换算后为 {}）；向量分量下标只能是 "
                                               "0（x）、1（y）、2（z）",
                                               context, end));
        }

        // 两端都算在取值范围内；起点越过终点时结果为空，由聚合函数决定空集的处理方式
        std::vector<Value> values;
        if (step > 0)
        {
            for (long index = begin; index <= end; index += step)
            {
                values.push_back(Value((*vector)[static_cast<unsigned short>(index)]));
            }
        } else
        {
            for (long index = begin; index >= end; index += step)
            {
                values.push_back(Value((*vector)[static_cast<unsigned short>(index)]));
            }
        }
        return values;
    }

} // namespace ExpressionEngine::Expression
