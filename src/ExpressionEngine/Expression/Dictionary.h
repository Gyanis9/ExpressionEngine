/**
 * @file Dictionary.h
 * @brief 内置的名字到取值字典，替宿主实现属性容器与解析器接口
 * @author Gyanis
 * @date 2026-09-20
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ExpressionEngine/Expression/PropertyModel.h>
#include <ExpressionEngine/Expression/Value.h>

namespace ExpressionEngine::Expression
{

    /**
     * @brief 内置的名字到取值字典
     * @details 宿主只要把名字与取值 define() 进来，就能直接求值 `x * 2`、`Box.Length + 1 mm`
     *          这类表达式，不必自己实现 IProperty / IObject / IObjectResolver。字典同时是
     *          解析器与「当前对象」，再用 addObject() 挂子字典，两级引用也走同一条路径。
     *          常量不需要单独的机制：定义一个只读名字即得常量。注意 pi、e、True、False 由
     *          词法阶段识别成引擎常量，字典里的同名条目不会生效。
     *          引用未命中时按 PropertyModel.h 的约定报错：对象名交给解析器，属性名交给本字典。
     */
    class Dictionary : public IObjectResolver, public IObject
    {
    public:
        /**
         * @brief 字典里的一个名字
         * @details 值存在这里，因此引用读写的两端都落在同一个条目上；地址在字典存活期间稳定。
         */
        class Entry : public IProperty
        {
        public:
            /**
             * @brief 构造一个条目
             * @param name 名字，如 "Length"
             * @param value 当前取值
             */
            Entry(std::string name, Value value);

            /**
             * @brief 名字
             * @details 重写 IProperty::name()：返回构造时登记的键。
             * @return 名字
             */
            [[nodiscard]] std::string_view name() const override;

            /**
             * @brief 类型名
             * @details 重写 IProperty::typeName()：按当前取值的类别给出（如 "数量"、"文本"），
             *          供报错文案与宿主的分量判定使用。
             * @return 取值的类型名
             */
            [[nodiscard]] std::string_view typeName() const override;

            /**
             * @brief 读取值
             * @details 重写 IProperty::value()：条目总在 define() 之后才有值，因此这里恒有值。
             * @return 当前取值
             */
            [[nodiscard]] std::optional<Value> value() const override;

            /**
             * @brief 写取值
             * @details 重写 IProperty::setValue()：字典条目默认可写，赋值表达式因此能落在这里；
             *          需要只读时用 setReadOnly() 关掉。
             * @param newValue 新值
             * @return 恒为 true（只读时返回 false）
             */
            [[nodiscard]] bool setValue(const Value &newValue) override;

            /**
             * @brief 是否只读
             * @details 重写 IProperty::isReadOnly()：返回 setReadOnly() 设定的标记。
             * @return 只读时为 true
             */
            [[nodiscard]] bool isReadOnly() const override;

            /**
             * @brief 设定只读标记
             * @param readOnly true 时拒绝写入，赋值表达式会报属性错
             */
            void setReadOnly(bool readOnly);

        private:
            friend class Dictionary; ///< 字典需要直接换掉取值（define 覆盖时保留只读标记）

            std::string m_name;     ///< 名字
            Value       m_value;    ///< 当前取值
            bool        m_readOnly; ///< 只读标记
        };

        Dictionary() = default;

        Dictionary(const Dictionary &) = delete;

        Dictionary &operator=(const Dictionary &) = delete;

        Dictionary(Dictionary &&) = delete;

        Dictionary &operator=(Dictionary &&) = delete;

        /**
         * @brief 定义或覆盖一个名字
         * @details 返回条目只是省去再 find() 一次；只在乎登记效果时可以丢弃返回值。
         * @param name 名字；已存在时连同取值一起覆盖
         * @param value 取值
         * @return 条目引用，地址在字典存活期间稳定
         */
        Entry &define(std::string name, Value value);

        /**
         * @brief 取条目
         * @param name 名字
         * @return 条目；未定义时返回 nullptr
         */
        [[nodiscard]] Entry *find(std::string_view name);

        /**
         * @brief 是否定义过该名字
         * @param name 名字
         * @return 已定义时为 true
         */
        [[nodiscard]] bool contains(std::string_view name) const;

        /**
         * @brief 删除一个名字
         * @param name 名字
         * @return 确有该条目并删除时为 true
         */
        [[nodiscard]] bool erase(std::string_view name);

        /**
         * @brief 挂一个子字典作为同级对象
         * @details 使 `Box.Length` 这类两级引用不必由宿主实现接口；子字典由本字典持有。
         * @param name 对象名，如 "Box"
         * @param child 子字典，所有权交给本字典
         * @return 挂进来的子字典引用；只需挂载效果时可以丢弃
         */
        Dictionary &addObject(std::string name, std::unique_ptr<Dictionary> child);

        /**
         * @brief 取子字典
         * @param name 对象名
         * @return 子字典；没有挂过则返回 nullptr
         */
        [[nodiscard]] Dictionary *findObject(std::string_view name);

        /**
         * @brief 清空名字与子字典
         * @details 只影响本字典，已交出去的对象引用与条目引用随之失效。
         */
        void clear();

        /**
         * @brief 对象名
         * @details 重写 IObject::name()：根字典是「当前对象」，名字为空；子字典返回挂载时
         *          给定的名字。
         * @return 对象名
         */
        [[nodiscard]] std::string_view name() const override;

        /**
         * @brief 对象标签
         * @details 重写 IObject::label()：字典没有独立标签，返回空。
         * @return 空视图
         */
        [[nodiscard]] std::string_view label() const override;

        /**
         * @brief 按名取属性
         * @details 重写 IObject::findProperty()：查本字典定义的名字。
         * @param propertyName 属性名
         * @return 属性；未定义时返回 nullptr，由引用节点报属性错
         */
        [[nodiscard]] IProperty *findProperty(std::string_view propertyName) override;

        /**
         * @brief 取全部属性名
         * @details 重写 IObject::propertyNames()：供报错提示与依赖枚举使用。
         * @return 按字典序排列的名字列表
         */
        [[nodiscard]] std::vector<std::string> propertyNames() const override;

        /**
         * @brief 所属文档名
         * @details 重写 IObject::documentName()：字典不区分文档，恒为空，因此无需限定名即可引用。
         * @return 空视图
         */
        [[nodiscard]] std::string_view documentName() const override;

        /**
         * @brief 按文档名与对象名解析对象
         * @details 重写 IObjectResolver::resolve()：对象名为空时给本字典（当前对象），
         *          否则查挂过的子字典；带文档名的引用本字典一概解析不到，由引用节点报错。
         * @param documentName 文档名；非空时解析失败
         * @param objectName 对象名
         * @return 对象；解析不到时返回 nullptr
         */
        [[nodiscard]] IObject *resolve(std::string_view documentName, std::string_view objectName) override;

        /**
         * @brief 列出文档内的对象名
         * @details 重写 IObjectResolver::objectNames()：字典没有文档概念，任何文档名都返回
         *          同一份子对象列表。
         * @param documentName 文档名，未使用
         * @return 子对象名列表
         */
        [[nodiscard]] std::vector<std::string> objectNames(std::string_view documentName) const override;

    private:
        std::string                                        m_name;    ///< 作为子字典时的对象名；根字典为空
        std::map<std::string, Entry>                       m_entries; ///< 名字到条目，有序存放以保证地址稳定
        std::map<std::string, std::unique_ptr<Dictionary>> m_objects; ///< 子字典，由本字典持有
    };

} // namespace ExpressionEngine::Expression
