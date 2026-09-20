/**
 * @file PropertyModel.h
 * @brief 表达式引擎与宿主对象模型之间的抽象接口
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ExpressionEngine/Expression/Value.h>

namespace ExpressionEngine::Expression
{

    /**
     * @brief 宿主侧的单个属性
     * @details 表达式引擎只通过本接口读写值，因此宿主可以用任意实现（文档对象、配置项、
     *          数据库记录）承载属性，而不必引入库自己的对象模型。
     */
    class IProperty
    {
    public:
        virtual ~IProperty() = default;

        /// 属性名，如 "Length"
        [[nodiscard]] virtual std::string_view name() const = 0;

        /// 属性类型名，用于把引用解析到正确的分量，如 "Length"、"Vector"、"Map"
        [[nodiscard]] virtual std::string_view typeName() const = 0;

        /**
         * @brief 读属性值
         * @return 当前值；属性尚未赋值时返回 std::nullopt
         */
        [[nodiscard]] virtual std::optional<Value> value() const = 0;

        /**
         * @brief 写属性值
         * @param newValue 新值
         * @return true 写入成功
         * @return false 属性只读或值不被接受
         */
        [[nodiscard]] virtual bool setValue(const Value &newValue) = 0;

        /// 属性是否只读；只读属性参与求值但拒绝写入
        [[nodiscard]] virtual bool isReadOnly() const
        {
            return false;
        }
    };

    /**
     * @brief 可容纳属性的容器
     * @details 宿主对象与对象的子分量（如数组元素所属的子对象）都实现本接口，
     *          使 `Box.Length`、`List[0].Size` 这类路径能用同一套解析逻辑走到底。
     */
    class IPropertyContainer
    {
    public:
        virtual ~IPropertyContainer() = default;

        /// 容器名，如对象名
        [[nodiscard]] virtual std::string_view name() const = 0;

        /// 容器标签；为空表示没有独立标签
        [[nodiscard]] virtual std::string_view label() const = 0;

        /**
         * @brief 按名取属性
         * @param propertyName 属性名
         * @return 属性对象；不存在时返回 nullptr
         */
        [[nodiscard]] virtual IProperty *findProperty(std::string_view propertyName) = 0;

        /// 取全部属性名，用于错误提示与依赖枚举
        [[nodiscard]] virtual std::vector<std::string> propertyNames() const = 0;
    };

    /**
     * @brief 宿主对象
     * @details 对象归属于某个文档；库不关心文档如何组织，只按「文档名 + 对象名」请求解析。
     */
    class IObject : public IPropertyContainer
    {
    public:
        ~IObject() override = default;

        /// 所属文档名；空表示当前文档（无需限定名即可引用）
        [[nodiscard]] virtual std::string_view documentName() const = 0;
    };

    /**
     * @brief 对象解析器
     * @details 宿主实现本接口后，库就能把 `Box.Length`、`<<Part>>.Box.Length` 这类引用
     *          解析成实际的属性读写；库自身不持有对象树的任何所有权。
     */
    class IObjectResolver
    {
    public:
        virtual ~IObjectResolver() = default;

        /**
         * @brief 按文档名与对象名解析对象
         * @details 库把引用文本切段后原样传进来，不做拆分也不归一大小写：对象名可以含点
         *          （写成 <<Doc>>.<<a.b>>.Length），宿主按整段名字查即可。空 objectName 表示
         *          「表达式所属的当前对象」，区间聚合读单元格也按这条约定取容器。
         * @param documentName 文档名；为空表示不限文档（取宿主认为合适的那一个）
         * @param objectName 对象名；为空表示当前对象
         * @return 对象；找不到时返回 nullptr，由调用方给出中文报错
         */
        [[nodiscard]] virtual IObject *resolve(std::string_view documentName, std::string_view objectName) = 0;

        /**
         * @brief 列出文档内的对象名
         * @param documentName 文档名；为空表示当前文档
         * @return 对象名列表，供错误提示与补全使用
         */
        [[nodiscard]] virtual std::vector<std::string> objectNames(std::string_view documentName) const = 0;
    };

} // namespace ExpressionEngine::Expression
