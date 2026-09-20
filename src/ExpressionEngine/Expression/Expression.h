/**
 * @file Expression.h
 * @brief 表达式抽象语法树与求值
 * @author Gyanis
 * @date 2026-09-19
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Expression/PropertyModel.h>
#include <ExpressionEngine/Expression/Range.h>
#include <ExpressionEngine/Expression/Value.h>
#include <ExpressionEngine/Units/Quantity.h>
#include <ExpressionEngine/Units/Unit.h>

namespace ExpressionEngine::Expression
{

    class Expression;
    class OperatorExpression;
    class RangeExpression;

    /**
     * @brief 表达式节点的所有权句柄
     * @details 子节点由父节点独占持有；节点不共享，因此不需要引用计数。
     */
    using ExpressionPtr = std::unique_ptr<Expression>;

    /**
     * @brief 变量引用路径的临时表示
     * @details 与 VariableExpression::Reference 是同一类型（后者是它的别名）；提升到命名
     *          空间作用域，是因为 Expression::collectReferences 声明时 VariableExpression
     *          尚未定义，无法在签名里写 VariableExpression::Reference。完整的标识符模型留给
     *          后续 Identifier 模块替换。
     */
    struct VariableReference
    {
        std::string documentName; ///< 文档名；空表示不限定文档
        std::string objectName;   ///< 对象名；空表示表达式所属的当前对象
        std::string propertyName; ///< 属性名，必填
    };

    /**
     * @brief 表达式求值失败
     * @details 表达式在语法上成立、但按当前取值无法求值时报出：函数参数个数不符、函数未知、
     *          运算符收到不参与该运算的类型、区间被当成标量取值等。取值自身的类型错与量纲错
     *          仍按 Base::TypeError、Base::UnitsMismatchError 抛出，调用方可分类处置。
     */
    class EvaluationError : public Base::ExpressionError
    {
    public:
        using Base::ExpressionError::ExpressionError;
    };

    /**
     * @brief 表达式节点基类
     * @details 每个节点自己完成求值、化简、文本化与深拷贝；对象引用通过宿主注入的
     *          IObjectResolver 解析，节点不持有对象树的所有权。分量由各节点自行解释：
     *          引用节点在解析到宿主属性后按分量逐段取子值，其余节点遇到分量时明确报错，
     *          不会静默忽略。
     */
    class Expression
    {
    public:
        /**
         * @brief 分量种类
         */
        enum class ComponentKind
        {
            Name,  ///< 属性路径上的一段名字，如 .Rotation
            Index, ///< 数组下标，如 [0]
            Range, ///< 区间，如 [1:3]
            MapKey ///< 映射键，如 [<<Length>>]
        };

        /**
         * @brief 引用路径上的一段分量
         */
        struct Component
        {
            ComponentKind kind{ComponentKind::Name}; ///< 分量种类
            std::string   name;                      ///< Name 与 MapKey 分量的文本
            ExpressionPtr index;                     ///< Index 与 Range 分量的起始下标
            ExpressionPtr endIndex;                  ///< Range 分量的结束下标；空表示开放区间
            ExpressionPtr step;                      ///< Range 分量的步长；空表示步长 1

            Component() = default;

            /**
             * @brief 造一个名字分量，如 .Rotation
             * @param componentName 分量名字
             */
            explicit Component(std::string componentName);

            /**
             * @brief 拷贝构造，子节点深拷贝
             * @param other 被拷贝的分量
             */
            Component(const Component &other);

            /**
             * @brief 移动构造
             * @param other 被移动的分量，移动后处于有效但未指定状态
             */
            Component(Component &&other) noexcept;

            ~Component();

            /**
             * @brief 拷贝赋值，子节点深拷贝
             * @param other 被赋值的分量
             * @return 自身引用
             */
            Component &operator=(const Component &other);

            /**
             * @brief 移动赋值
             * @param other 被移动的分量，移动后处于有效但未指定状态
             * @return 自身引用
             */
            Component &operator=(Component &&other) noexcept;

            /**
             * @brief 造一个数组下标分量，如 [0]
             * @param indexExpression 下标表达式
             * @return 下标分量
             */
            static Component arrayIndex(ExpressionPtr indexExpression);

            /**
             * @brief 造一个映射键分量，如 [<<Length>>]
             * @param key 映射键文本
             * @return 映射键分量
             */
            static Component mapKey(std::string key);

            /**
             * @brief 造一个区间分量，如 [1:3]
             * @param begin 起始下标表达式
             * @param end 结束下标表达式；空表示开放区间
             * @param stepExpression 步长表达式；空表示步长 1
             * @return 区间分量
             */
            static Component rangeComponent(ExpressionPtr begin, ExpressionPtr end, ExpressionPtr stepExpression = nullptr);

            /**
             * @brief 两段分量是否结构相同（名字相同、下标表达式结构相同）
             * @param other 另一段分量
             * @return 结构相同时为 true
             */
            [[nodiscard]] bool isSame(const Component &other) const;

            /**
             * @brief 追加分量的文本写法
             * @param text 输出：在末尾追加分量文本
             * @param persistent true 时生成可回填、可持久化的文本
             */
            void appendText(std::string &text, bool persistent) const;
        };

        /**
         * @brief 引用路径的分量列表
         * @details 按路径顺序排列，如 Box.Placement.Base 对应 [Name(Box), Name(Placement), Name(Base)]。
         */
        using ComponentList = std::vector<Component>;

        /**
         * @brief 默认优先级
         * @details 优先级数值更小的节点在合成文本时需要括号；不在运算中的节点取本值。
         */
        static constexpr int s_defaultPriority = 20;

        /**
         * @brief 构造表达式节点
         * @param resolver 对象解析器，可为空；为空时引用型节点求值会明确报错
         */
        explicit Expression(IObjectResolver *resolver = nullptr);

        virtual ~Expression();

        Expression(const Expression &) = delete;

        Expression &operator=(const Expression &) = delete;

        /**
         * @brief 取对象解析器
         * @return 对象解析器；未绑定时为空
         */
        [[nodiscard]] IObjectResolver *resolver() const noexcept;

        /**
         * @brief 求值
         * @return 求值结果
         * @throws Base::Exception 各类求值失败；调用方可用 Base::Exception 统一兜住
         */
        [[nodiscard]] Value evaluate() const;

        /**
         * @brief 求值并把结果包装成常量节点
         * @return 常量节点：数值为 NumberExpression，文本为 StringExpression，
         *         布尔为 ConstantExpression，几何值为 ValueExpression
         * @throws Base::Exception 同 evaluate()
         */
        [[nodiscard]] ExpressionPtr evaluateToConstantNode() const;

        /**
         * @brief 化简
         * @details 与求值的区别：只有全部子节点都是常量时才折叠成常量节点，否则重建同构节点。
         * @return 化简后的表达式
         */
        [[nodiscard]] virtual ExpressionPtr simplify() const = 0;

        /**
         * @brief 转成文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param checkPriority true 时按优先级补括号，使文本能原样解析回同一棵树
         * @param indent 缩进层级，预留给多行排版
         * @return 表达式文本
         */
        [[nodiscard]] std::string toString(bool persistent = false, bool checkPriority = false, int indent = 0) const;

        /**
         * @brief 取运算优先级
         * @return 优先级；默认 20，数值越小优先级越低
         */
        [[nodiscard]] virtual int priority() const;

        /**
         * @brief 深拷贝，连同分量与注释
         * @return 本节点的新副本
         */
        [[nodiscard]] ExpressionPtr copy() const;

        /**
         * @brief 判断两棵树是否相同
         * @param other 另一棵树
         * @param checkComment true 时注释也要相同
         * @return 节点种类、持久化文本与注释都相同时为 true
         */
        [[nodiscard]] bool isSame(const Expression &other, bool checkComment = true) const;

        /**
         * @brief 是否带分量
         * @return 带分量时为 true
         */
        [[nodiscard]] bool hasComponent() const noexcept;

        /**
         * @brief 取分量列表
         * @return 分量列表
         */
        [[nodiscard]] const ComponentList &components() const noexcept;

        /**
         * @brief 追加一段分量
         * @param component 分量；名字相同的分量按路径顺序依次追加
         */
        void addComponent(Component component);

        /**
         * @brief 收集表达式里出现的全部变量引用
         * @details 宿主据此建立依赖关系（被引用的属性变化时重新求值）。返回值按首次出现顺序去重。
         * @return 变量引用列表；没有引用时为空
         */
        [[nodiscard]] std::vector<VariableReference> collectReferences() const;

        /**
         * @brief 取注释
         * @return 注释文本
         */
        [[nodiscard]] const std::string &comment() const noexcept;

        /**
         * @brief 设置注释
         * @param text 注释文本
         */
        void setComment(std::string text);

        /**
         * @brief 节点种类名，用于相等判定与诊断
         * @return 本节点的种类名
         */
        [[nodiscard]] virtual std::string_view nodeName() const = 0;

        /**
         * @brief 本节点是区间表达式时返回自身，否则返回 nullptr；供聚合函数识别区间参数
         * @return 本节点自身；非区间节点为空
         */
        [[nodiscard]] virtual const RangeExpression *asRangeExpression() const noexcept;

        /**
         * @brief 本节点是运算符节点时返回自身，否则返回 nullptr；供文本化判断结合性
         * @return 本节点自身；非运算符节点为空
         */
        [[nodiscard]] virtual const OperatorExpression *asOperatorExpression() const noexcept;

        /**
         * @brief 本节点是否已是常量数值（数值节点或命名常量节点）；供常量折叠判断
         * @return 已是常量数值时为 true
         */
        [[nodiscard]] virtual bool isConstantNumeric() const noexcept;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 由基类 evaluate() 调用，返回的取值随后才接受分量访问与其它节点语义；
         *          子类只实现本方法，不必重复处理分量。
         * @return 本节点的取值
         * @throws Base::Exception 求值失败（类型不符、量纲不匹配、引用解析不到等）
         */
        [[nodiscard]] virtual Value evaluateNode() const = 0;

        /**
         * @brief 追加本节点的文本
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        virtual void appendText(std::string &text, bool persistent, int indent) const = 0;

        /**
         * @brief 生成同类型的空壳副本
         * @details 分量与注释由基类 copy() 统一补齐，子类只需复制自身节点数据。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] virtual ExpressionPtr copyNode() const = 0;

        /**
         * @brief 本节点后面能否直接跟分量
         * @details 默认 false；只有引用节点与文本节点允许，其值可按下标取子值。
         * @return 允许跟随分量时为 true
         */
        [[nodiscard]] virtual bool isIndexable() const;

        /**
         * @brief 本节点能否自行把分量作用到求值结果上
         * @details 只有引用节点在绑定了解析器时才能（分量作用在解析到的属性值上）；其余
         *          节点遇到分量时由 evaluate() 明确报错，避免静默取到整体值。
         * @return 能按分量取值时为 true；默认 false
         */
        [[nodiscard]] virtual bool supportsComponentAccess() const noexcept;

        /**
         * @brief 收集本节点子树里的变量引用
         * @details 基类不知道子节点结构，默认什么都不收集；复合节点覆写后递归子表达式，
         *          引用节点覆写后把自身追加进列表。去重由 collectReferences() 统一完成。
         * @param collectedReferences 输出：按首次出现顺序追加引用
         */
        virtual void collectReferencesInto(std::vector<VariableReference> &collectedReferences) const;

        /**
         * @brief 供复合节点的递归钩子把子表达式的引用追加到同一列表
         * @details 输出列表由调用方复用，去重只在 collectReferences() 做一次；空指针表示
         *          该分支不存在，直接跳过。
         * @param expression 子表达式；可为空
         * @param collectedReferences 输出：追加收集到的引用
         */
        static void collectReferencesFrom(const Expression *expression, std::vector<VariableReference> &collectedReferences);

    private:
        IObjectResolver *m_resolver;   ///< 对象解析器，不持所有权，可为空
        ComponentList    m_components; ///< 分量列表，由各节点按 supportsComponentAccess() 的约定解释
        std::string      m_comment;    ///< 注释
    };

    /**
     * @brief 把值包装成常量节点
     * @param resolver 对象解析器，可为空
     * @param value 待包装的值
     * @return 数值与文本各有专用节点，几何值用 ValueExpression 承载
     */
    [[nodiscard]] ExpressionPtr makeValueExpression(IObjectResolver *resolver, const Value &value);

    /**
     * @brief 带单位的数值节点
     * @details 同时是数值、运算符、函数与变量节点的基类：这些节点的"取值"都是
     *          一个带单位的量，因此单位节点承担这部分公共状态。
     */
    class UnitExpression : public Expression
    {
    public:
        /**
         * @brief 构造单位节点
         * @param resolver 对象解析器，可为空
         * @param quantity 数量
         * @param unitText 单位原文，用于回写表达式；空表示按数值排版
         */
        explicit UnitExpression(IObjectResolver *resolver = nullptr, const Units::Quantity &quantity = Units::Quantity(), std::string unitText = std::string());

        ~UnitExpression() override;

        /**
         * @brief 设置数量
         * @param quantity 数量
         */
        void setQuantity(const Units::Quantity &quantity);

        /**
         * @brief 设置数量；与 setQuantity() 等价的兼容别名
         * @param quantity 数量
         */
        void setUnit(const Units::Quantity &quantity);

        /**
         * @brief 取数值（以基准量纲表示）
         * @return 基准量纲下的数值
         */
        [[nodiscard]] double getValue() const;

        /**
         * @brief 取单位（量纲）
         * @return 单位（量纲）
         */
        [[nodiscard]] const Units::Unit &getUnit() const;

        /**
         * @brief 取数量
         * @return 数量
         */
        [[nodiscard]] const Units::Quantity &getQuantity() const;

        /**
         * @brief 取单位原文
         * @return 单位原文；空表示按数值排版
         */
        [[nodiscard]] std::string getUnitText() const;

        /**
         * @brief 取比例系数，等价于 getValue()
         * @return 比例系数
         */
        [[nodiscard]] double getScaler() const;

        /**
         * @brief 化简本节点
         * @details 重写 Expression::simplify()：单位节点本身已是常量，无需折叠子节点，
         *          直接返回等值的数值节点。
         * @return 数量与本节点相同的 NumberExpression
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /**
         * @brief 节点种类名
         * @details 重写 Expression::nodeName()：固定返回 "Unit"，供相等判定与诊断定位。
         * @return "Unit"
         */
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 重写 Expression::evaluateNode()：本节点不带运算，直接把持有的数量作为取值
         *          返回，分量与注释由基类 evaluate() 处理。
         * @return 本节点持有的数量
         */
        [[nodiscard]] Value evaluateNode() const override;

        /**
         * @brief 追加本节点的文本
         * @details 重写 Expression::appendText()：优先写单位原文（如 "2 mm"），没有原文时按
         *          数值排版；persistent 只影响数字的写法，不影响单位部分。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 Expression::copyNode()：复制数量与单位原文，分量与注释由基类 copy() 补齐。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;

    private:
        Units::Quantity m_quantity; ///< 数量
        std::string     m_unitText; ///< 单位原文
    };

    /**
     * @brief 数值节点：一个带单位的常量
     */
    class NumberExpression : public UnitExpression
    {
    public:
        /**
         * @brief 构造数值节点
         * @param resolver 对象解析器，可为空
         * @param quantity 数量
         */
        explicit NumberExpression(IObjectResolver *resolver = nullptr, const Units::Quantity &quantity = Units::Quantity());

        /**
         * @brief 化简本节点
         * @details 重写 UnitExpression::simplify()：数值节点已是常量，复制自身而不新建节点，
         *          因此分量与注释都能保留。
         * @return 本节点的副本
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /**
         * @brief 取负
         */
        void negate();

        /**
         * @brief 取整数取值
         * @return 数值为整数时返回该整数（可超出 int 范围），否则返回空
         */
        [[nodiscard]] std::optional<long> integerValue() const;

        /**
         * @brief 节点种类名
         * @details 重写 UnitExpression::nodeName()：固定返回 "Number"，与 "Unit" 区分开，
         *          使 AST 相等判定能识别节点具体种类。
         * @return "Number"
         */
        [[nodiscard]] std::string_view nodeName() const override;

        /**
         * @brief 本节点是否已是常量数值
         * @details 重写 Expression::isConstantNumeric()：数值节点恒为常量数值，无需再判断子节点。
         * @return 恒为 true
         */
        [[nodiscard]] bool isConstantNumeric() const noexcept override;

    protected:
        /**
         * @brief 追加本节点的文本
         * @details 重写 UnitExpression::appendText()：数值节点不带运算，直接写数字与单位；
         *          中间结果（simplify 产生）与用户书写的文本走同一套写法。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 UnitExpression::copyNode()：按数值节点类型复制数量与单位原文。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;
    };

    /**
     * @brief 命名常量节点：True、False 等有名字的常量，取值与名字同时保留
     */
    class ConstantExpression : public NumberExpression
    {
    public:
        /**
         * @brief 构造常量节点
         * @param resolver 对象解析器，可为空
         * @param name 常量名，如 "True"、"False"、"pi"
         * @param quantity 常量取值
         */
        explicit ConstantExpression(IObjectResolver *resolver = nullptr, std::string name = std::string(), const Units::Quantity &quantity = Units::Quantity());

        /**
         * @brief 取常量名
         * @return 常量名
         */
        [[nodiscard]] std::string getName() const;

        /**
         * @brief 常量是否按数值参与运算；True 与 False 是布尔值，不算数值
         * @return 参与数值运算时为 true
         */
        [[nodiscard]] bool isNumber() const;

        /**
         * @brief 节点种类名
         * @details 重写 NumberExpression::nodeName()：固定返回 "Constant"，使带名字的常量
         *          不与普通数值节点在相等判定中混同。
         * @return "Constant"
         */
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 重写 NumberExpression 继承来的 UnitExpression::evaluateNode()：True 与 False
         *          求值成布尔值，其余常量按无量纲数量求值。
         * @return 布尔值或数量
         */
        [[nodiscard]] Value evaluateNode() const override;

        /**
         * @brief 追加本节点的文本
         * @details 重写 NumberExpression::appendText()：非数值常量直接写常量名（如 True），
         *          数值常量沿用数字排版，保证文本能解析回同一节点。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 NumberExpression::copyNode()：额外复制常量名，否则 True 会退化成数值节点。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;

    private:
        std::string m_name; ///< 常量名
    };

    /**
     * @brief 运算符节点：一元与二元运算
     */
    class OperatorExpression : public UnitExpression
    {
    public:
        /**
         * @brief 运算符
         */
        enum class Operator
        {
            None,         ///< 未设置
            Add,          ///< 加
            Subtract,     ///< 减
            Multiply,     ///< 乘
            Divide,       ///< 除
            Modulo,       ///< 取余
            Power,        ///< 幂
            Equal,        ///< 等于
            NotEqual,     ///< 不等于
            Less,         ///< 小于
            Greater,      ///< 大于
            LessEqual,    ///< 小于等于
            GreaterEqual, ///< 大于等于
            UnitScale,    ///< 数量与单位相乘，如 2 mm
            Negate,       ///< 取负
            Positive      ///< 取正
        };

        /**
         * @brief 构造运算符节点
         * @param resolver 对象解析器，可为空
         * @param left 左操作数；Negate 与 Positive 只用左操作数
         * @param operation 运算符
         * @param right 右操作数；一元运算符可为空
         */
        explicit OperatorExpression(IObjectResolver *resolver = nullptr, ExpressionPtr left = nullptr, Operator operation = Operator::None, ExpressionPtr right = nullptr);

        ~OperatorExpression() override;

        /**
         * @brief 取运算符
         * @return 运算符
         */
        [[nodiscard]] Operator getOperator() const noexcept;

        /**
         * @brief 取左操作数
         * @return 左操作数
         */
        [[nodiscard]] const Expression *getLeft() const noexcept;

        /**
         * @brief 取右操作数
         * @return 右操作数；一元运算符为空
         */
        [[nodiscard]] const Expression *getRight() const noexcept;

        /**
         * @brief 设置左操作数
         * @param expression 新左操作数
         */
        void setLeft(ExpressionPtr expression);

        /**
         * @brief 设置右操作数
         * @param expression 新右操作数
         */
        void setRight(ExpressionPtr expression);

        /**
         * @brief 化简本节点
         * @details 重写 Expression::simplify()：两侧都化简成常量数值时直接折叠求值，否则按
         *          简化后的操作数重建同类型节点，保证化简不改变运算结构。
         * @return 常量节点或重建的运算符节点
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /**
         * @brief 取运算优先级
         * @details 重写 Expression::priority()：比较 1、加减 3、乘除取余 4、幂 5、一元与单位 6，文本化时据此补括号。
         * @return 本运算符的优先级
         */
        [[nodiscard]] int priority() const override;

        /**
         * @brief 运算是否可交换
         * @return 可交换时为 true
         */
        [[nodiscard]] bool isCommutative() const;

        /**
         * @brief 运算是否左结合
         * @return 左结合时为 true
         */
        static [[nodiscard]] bool isLeftAssociative() ;

        /**
         * @brief 运算是否右结合
         * @return 右结合时为 true
         */
        [[nodiscard]] bool isRightAssociative() const;

        /**
         * @brief 取运算符的文本写法
         * @param operation 运算符
         * @return 运算符文本
         */
        [[nodiscard]] static std::string_view operatorText(Operator operation);

        /**
         * @brief 取文本对应的运算符；无法识别时返回 None
         * @param text 运算符文本
         * @return 运算符；无法识别时为 Operator::None
         */
        [[nodiscard]] static Operator operatorFromText(std::string_view text);

        /**
         * @brief 节点种类名
         * @details 重写 UnitExpression::nodeName()：固定返回 "Operator"，比较运算与算术运算
         *          共用本节点，靠 getOperator() 区分具体运算。
         * @return "Operator"
         */
        [[nodiscard]] std::string_view nodeName() const override;

        /**
         * @brief 取本节点的运算符表达式视图
         * @details 重写 Expression::asOperatorExpression()：本类节点直接返回自身，
         *          省去基类的一次类型判断，文本化时判断结合性会频繁用到。
         * @return 本节点自身
         */
        [[nodiscard]] const OperatorExpression *asOperatorExpression() const noexcept override;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 重写 Expression::evaluateNode()：按运算符分派到对应的运算；一元运算符只用
         *          左操作数，右操作数缺失时按一元语义处理。
         * @return 运算结果
         * @throws Base::Exception 操作数类型或量纲不参与该运算
         */
        [[nodiscard]] Value evaluateNode() const override;

        /**
         * @brief 追加本节点的文本
         * @details 重写 Expression::appendText()：一元运算符在操作数前写符号，二元运算符在
         *          两侧操作数之间写符号，必要时按优先级补括号。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 Expression::copyNode()：复制运算符，操作数由基类 copy() 递归深拷贝。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;

        /**
         * @brief 递归收集左右操作数里的变量引用
         * @details 覆写基类的空实现：运算符自己不产生依赖，但两侧子树可能引用宿主属性；
         *          先左后右追加以保持「首次出现」顺序。分量索引表达式由解析器保证为常量，
         *          不参与收集。
         * @param collectedReferences 输出：追加两侧子树的引用
         */
        void collectReferencesInto(std::vector<VariableReference> &collectedReferences) const override;

    private:
        Operator      m_operator; ///< 运算符
        ExpressionPtr m_left;     ///< 左操作数
        ExpressionPtr m_right;    ///< 右操作数
    };

    /**
     * @brief 三元条件节点：条件 ? 真分支 : 假分支
     */
    class ConditionalExpression : public Expression
    {
    public:
        /**
         * @brief 构造条件节点
         * @param resolver 对象解析器，可为空
         * @param condition 条件表达式，按「非零为真」判定
         * @param trueExpression 条件为真时取值的分支
         * @param falseExpression 条件为假时取值的分支
         */
        explicit ConditionalExpression(IObjectResolver *resolver        = nullptr, ExpressionPtr condition = nullptr, ExpressionPtr trueExpression = nullptr,
                                       ExpressionPtr    falseExpression = nullptr);

        ~ConditionalExpression() override;

        /**
         * @brief 取条件表达式
         * @return 条件表达式
         */
        [[nodiscard]] const Expression *getCondition() const noexcept;

        /**
         * @brief 取真分支
         * @return 真分支表达式
         */
        [[nodiscard]] const Expression *getTrueExpression() const noexcept;

        /**
         * @brief 取假分支
         * @return 假分支表达式
         */
        [[nodiscard]] const Expression *getFalseExpression() const noexcept;

        /**
         * @brief 化简本节点
         * @details 重写 Expression::simplify()：条件化简后仍是常量时直接返回被选中分支的化简
         *          结果（未选中的分支不求值、不化简），否则重建同类型节点。
         * @return 被选中分支的化简结果，或重建的条件节点
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /**
         * @brief 取运算优先级
         * @details 重写 Expression::priority()：恒为 2，仅高于赋值类运算，使三元表达式在文本化时整体带括号。
         * @return 固定值 2
         */
        [[nodiscard]] int priority() const override;

        /**
         * @brief 节点种类名
         * @details 重写 Expression::nodeName()：固定返回 "Conditional"。
         * @return "Conditional"
         */
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 重写 Expression::evaluateNode()：先对条件做真值判定，只对选中的分支求值，
         *          另一分支完全不被触及（短路语义）。
         * @return 被选中分支的取值
         * @throws Base::TypeError 条件无法参与真值判定
         */
        [[nodiscard]] Value evaluateNode() const override;

        /**
         * @brief 追加本节点的文本
         * @details 重写 Expression::appendText()：按「条件 ? 真分支 : 假分支」书写，条件恒定时
         *          仍按完整三元式输出，保证文本能解析回同一结构。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 Expression::copyNode()：三个子表达式由基类 copy() 递归深拷贝。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;

        /**
         * @brief 递归收集条件与两个分支里的变量引用
         * @details 覆写基类的空实现：条件恒定时分支可能不参与求值，但引用仍要全部收上，
         *          否则宿主会漏建依赖；按条件、真分支、假分支的顺序追加。
         * @param collectedReferences 输出：追加三个子表达式的引用
         */
        void collectReferencesInto(std::vector<VariableReference> &collectedReferences) const override;

    private:
        ExpressionPtr m_condition;       ///< 条件表达式
        ExpressionPtr m_trueExpression;  ///< 真分支
        ExpressionPtr m_falseExpression; ///< 假分支
    };

    /**
     * @brief 函数调用节点
     */
    class FunctionExpression : public UnitExpression
    {
    public:
        /**
         * @brief 函数种类
         * @details 枚举值与函数名表一一对应；Create、List、Tuple 依赖宿主的对象工厂，本库保留条目但在构造时即报错，以免表达式被误当成可用。
         */
        enum class Function
        {
            None, ///< 未设置

            // 标量函数
            Absolute,          ///< abs：绝对值
            ArcCosine,         ///< acos：反余弦，结果带角度单位
            ArcSine,           ///< asin：反正弦
            ArcTangent,        ///< atan：反正切
            ArcTangent2,       ///< atan2：两参数反正切
            Cathetus,          ///< cath：sqrt(a^2 - b^2 - c^2)
            CubeRoot,          ///< cbrt：立方根
            Ceiling,           ///< ceil：向上取整
            Cosine,            ///< cos：余弦
            HyperbolicCosine,  ///< cosh：双曲余弦
            Exponential,       ///< exp：自然指数
            Floor,             ///< floor：向下取整
            Hypotenuse,        ///< hypot：sqrt(a^2 + b^2 + c^2)
            Logarithm,         ///< log：自然对数
            LogarithmBase10,   ///< log10：常用对数
            Modulo,            ///< mod：取余
            Power,             ///< pow：幂
            Round,             ///< round：四舍五入
            Sine,              ///< sin：正弦
            HyperbolicSine,    ///< sinh：双曲正弦
            SquareRoot,        ///< sqrt：平方根
            Tangent,           ///< tan：正切
            HyperbolicTangent, ///< tanh：双曲正切
            Truncate,          ///< trunc：截断取整

            // 向量函数
            VectorAngle,               ///< vangle：两向量夹角，结果带角度单位
            VectorCross,               ///< vcross：叉积
            VectorDot,                 ///< vdot：点积
            VectorLineDistance,        ///< vlinedist：点到直线的距离
            VectorLineSegmentDistance, ///< vlinesegdist：点到线段的最近点
            VectorLineProjection,      ///< vlineproj：点在线上的投影
            VectorNormalize,           ///< vnormalize：单位化
            VectorPlaneDistance,       ///< vplanedist：点到平面的距离
            VectorPlaneProjection,     ///< vplaneproj：点在平面上的投影
            VectorScale,               ///< vscale：三方向缩放
            VectorScaleX,              ///< vscalex：X 方向缩放
            VectorScaleY,              ///< vscaley：Y 方向缩放
            VectorScaleZ,              ///< vscalez：Z 方向缩放

            // 矩阵、旋转与位姿函数
            MatrixInvert,    ///< minvert：求逆
            MatrixRotate,    ///< mrotate：按旋转或欧拉角旋转
            MatrixRotateX,   ///< mrotatex：绕 X 轴旋转
            MatrixRotateY,   ///< mrotatey：绕 Y 轴旋转
            MatrixRotateZ,   ///< mrotatez：绕 Z 轴旋转
            MatrixScale,     ///< mscale：缩放
            MatrixTranslate, ///< mtranslate：平移

            // 构造函数
            Create,            ///< create：按类型名造对象，本库不支持
            List,              ///< list：造列表，本库不支持
            Matrix,            ///< matrix：造矩阵
            Placement,         ///< placement：造位姿
            Rotation,          ///< rotation：造旋转
            RotationX,         ///< rotationx：绕 X 轴旋转
            RotationY,         ///< rotationy：绕 Y 轴旋转
            RotationZ,         ///< rotationz：绕 Z 轴旋转
            Stringify,         ///< str：转文本
            ParseQuantity,     ///< parsequant：解析数量文本
            TranslationMatrix, ///< translationm：造平移矩阵
            Tuple,             ///< tuple：造元组，本库不支持
            Vector,            ///< vector：造向量

            // 单元格
            Address, ///< address：由行列造单元格地址

            // 引用
            HiddenReference,      ///< hiddenref：隐藏引用，取值但不建立依赖
            HiddenReferenceAlias, ///< href：hiddenref 的旧名

            // 逻辑
            LogicalNot, ///< not：逻辑非

            Aggregates, ///< 聚合函数的起始哨兵，本身不是函数；与其后的聚合函数相邻，便于范围判断

            Average,           ///< average：平均
            Count,             ///< count：计数
            Maximum,           ///< max：最大
            Minimum,           ///< min：最小
            StandardDeviation, ///< stddev：样本标准差
            Sum,               ///< sum：求和

            LogicalAnd, ///< and：逻辑与
            LogicalOr,  ///< or：逻辑或

            Last ///< 枚举末尾哨兵
        };

        /**
         * @brief 构造函数调用节点
         * @param resolver 对象解析器，可为空
         * @param function 函数种类
         * @param name 函数名，用于文本化与报错
         * @param arguments 实参，按值接收所有权
         * @throws EvaluationError 参数个数与该函数的要求不符
         * @throws Base::ParserError 函数是哨兵值，或函数需要宿主对象工厂
         */
        explicit FunctionExpression(IObjectResolver *          resolver  = nullptr, Function function = Function::None, std::string name = std::string(),
                                    std::vector<ExpressionPtr> arguments = std::vector<ExpressionPtr>());

        ~FunctionExpression() override;

        /**
         * @brief 取函数种类
         * @return 函数种类
         */
        [[nodiscard]] Function getFunction() const noexcept;

        /**
         * @brief 取实参
         * @return 实参列表
         */
        [[nodiscard]] const std::vector<ExpressionPtr> &getArguments() const noexcept;

        /**
         * @brief 化简本节点
         * @details 重写 UnitExpression::simplify()：全部实参都能化简成数值节点时直接求值，
         *          否则用简化后的实参重建节点；聚合函数带区间实参，因此保留原结构。
         * @return 常量节点或重建的函数节点
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /**
         * @brief 按函数种类求值
         * @param context 调用上下文，用于取解析器与拼报错文案
         * @param function 函数种类
         * @param arguments 实参
         * @return 求值结果
         * @throws Base::Exception 类型不符、量纲不匹配、参数个数不符等
         */
        [[nodiscard]] static Value evaluateFunction(const Expression &context, Function function, const std::vector<ExpressionPtr> &arguments);

        /**
         * @brief 取函数的规范名，如 "sqrt"
         * @param function 函数种类
         * @return 函数的规范名
         */
        [[nodiscard]] static std::string_view functionName(Function function);

        /**
         * @brief 取名字对应的函数；无法识别时返回 Function::None
         * @param name 函数名
         * @return 函数种类；无法识别时为 Function::None
         */
        [[nodiscard]] static Function functionFromName(std::string_view name);

        /**
         * @brief 节点种类名
         * @details 重写 UnitExpression::nodeName()：固定返回 "Function"。
         * @return "Function"
         */
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 重写 UnitExpression::evaluateNode()：普通函数按实参逐一求值后分派，
         *          聚合函数把区间实参展开成序列再做归约。
         * @return 函数结果
         * @throws Base::Exception 实参个数或类型与该函数要求不符
         */
        [[nodiscard]] Value evaluateNode() const override;

        /**
         * @brief 追加本节点的文本
         * @details 重写 UnitExpression::appendText()：写成「规范名(实参, …)」；聚合函数的
         *          区间实参按原写法还原，使文本能解析回同一棵树。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 UnitExpression::copyNode()：复制函数种类与名字，实参由基类 copy()
         *          递归深拷贝。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;

        /**
         * @brief 递归收集全部实参里的变量引用
         * @details 覆写基类的空实现：普通实参与聚合函数的区间实参都可能引用宿主属性，
         *          按实参顺序追加，保证依赖列表与表达式里的出现顺序一致。
         * @param collectedReferences 输出：追加各实参的引用
         */
        void collectReferencesInto(std::vector<VariableReference> &collectedReferences) const override;

    private:
        /**
         * @brief 对区间实参做聚合
         * @param context 调用上下文，用于取解析器定位单元格
         * @param function 聚合函数种类
         * @param arguments 实参
         * @return 聚合结果
         */
        [[nodiscard]] static Value evaluateAggregate(const Expression &context, Function function, const std::vector<ExpressionPtr> &arguments);

        Function                   m_function;  ///< 函数种类
        std::string                m_name;      ///< 函数名
        std::vector<ExpressionPtr> m_arguments; ///< 实参
    };

    /**
     * @brief 变量引用节点
     * @details 引用被解耦成「文档名 + 对象名 + 属性名」三元组，由宿主注入的 IObjectResolver
     *          解析成对象，再经 IPropertyContainer::findProperty 与 IProperty 读写取值。
     *          解析到基属性后分量按值语义逐段取值：下标取向量分量或文本字符；区间、映射键
     *          与名字分量在取值路径上给出明确报错（分别提示聚合函数、值模型无映射类型、
     *          名字指向子属性），完整标识符模型留给后续 Identifier 模块。
     */
    class VariableExpression : public UnitExpression
    {
    public:
        /**
         * @brief 引用路径
         * @details 与命名空间作用域的 VariableReference 是同一类型（别名而非新类型），
         *          因此两者可互相赋值、比较。
         */
        using Reference = VariableReference;

        /**
         * @brief 构造变量引用节点
         * @param resolver 对象解析器，可为空
         * @param reference 引用路径
         */
        explicit VariableExpression(IObjectResolver *resolver = nullptr, Reference reference = Reference());

        ~VariableExpression() override;

        /**
         * @brief 取引用路径
         * @return 引用路径
         */
        [[nodiscard]] const Reference &getReference() const noexcept;

        /**
         * @brief 设置引用路径
         * @param reference 新引用路径
         */
        void setReference(Reference reference);

        /**
         * @brief 属性名
         * @return 属性名
         */
        [[nodiscard]] std::string name() const;

        /**
         * @brief 引用的文本写法，如 "Part.Box.Length"，用于报错
         * @return 引用文本
         */
        [[nodiscard]] std::string pathText() const;

        /**
         * @brief 解析到宿主属性
         * @return 属性对象；解析器缺失、对象不存在或属性不存在时抛错
         * @throws Base::NameError 表达式没有绑定解析器，或对象解析不到
         * @throws Base::AttributeError 对象上没有该属性
         */
        [[nodiscard]] IProperty *resolveProperty() const;

        /**
         * @brief 把值写回宿主属性
         * @param newValue 新值
         * @throws Base::AttributeError 属性只读
         * @throws Base::ValueError 属性拒绝该取值
         */
        void assignValue(const Value &newValue) const;

        /**
         * @brief 化简本节点
         * @details 重写 UnitExpression::simplify()：引用节点是叶子，求值需要宿主对象，无法在
         *          化简期折叠，因此复制自身。
         * @return 本节点的副本
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /**
         * @brief 节点种类名
         * @details 重写 UnitExpression::nodeName()：固定返回 "Variable"。
         * @return "Variable"
         */
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 重写 UnitExpression::evaluateNode()：按引用路径解析宿主属性并读取取值；
         *          属性尚未赋值时报错而不是当作 0，避免静默使用未就绪的值。
         * @return 属性的当前取值
         * @throws Base::NameError 解析器缺失或对象解析不到
         * @throws Base::AttributeError 属性不存在
         */
        [[nodiscard]] Value evaluateNode() const override;

        /**
         * @brief 追加本节点的文本
         * @details 重写 UnitExpression::appendText()：写「文档名.对象名.属性名」形式；
         *          分量由基类统一追加。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 UnitExpression::copyNode()：复制引用路径三元组。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;

        /**
         * @brief 本节点后面能否直接跟分量
         * @details 重写 Expression::isIndexable()：引用后面可以直接跟分量与下标，如
         *          Box.Length[0]，因此返回 true（基类默认为 false）。
         * @return 恒为 true
         */
        [[nodiscard]] bool isIndexable() const override;

        /**
         * @brief 引用节点在绑定了解析器时支持分量取值
         * @details 与基类实现的差异：分量作用在解析到的属性值上（先解析基属性、再逐段取
         *          子值），因此必须存在解析器；未绑定解析器时返回 false，由基类在读数之前
         *          统一报错，避免把「没有解析器」误报成「属性不存在」。
         * @return 绑定了解析器时为 true
         */
        [[nodiscard]] bool supportsComponentAccess() const noexcept override;

        /**
         * @brief 把自身引用追加进列表
         * @details 覆写基类的空实现：引用节点是依赖的来源，直接把 m_reference 追加进 collectedReferences；
         *          分量里只有解析器保证为常量的下标表达式，没有可依赖的变量。
         * @param collectedReferences 输出：追加自身引用
         */
        void collectReferencesInto(std::vector<VariableReference> &collectedReferences) const override;

    private:
        Reference m_reference; ///< 引用路径
    };

    /**
     * @brief 文本节点；求值结果就是文本本身
     */
    class StringExpression : public Expression
    {
    public:
        /**
         * @brief 构造文本节点
         * @param resolver 对象解析器，可为空
         * @param text 文本内容
         */
        explicit StringExpression(IObjectResolver *resolver = nullptr, std::string text = std::string());

        /**
         * @brief 取文本内容
         * @return 文本内容
         */
        [[nodiscard]] std::string getText() const;

        /**
         * @brief 化简本节点
         * @details 重写 Expression::simplify()：文本已是常量，复制自身而不新建节点，
         *          保证注释与分量不丢失。
         * @return 本节点的副本
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /**
         * @brief 节点种类名
         * @details 重写 Expression::nodeName()：固定返回 "String"。
         * @return "String"
         */
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 重写 Expression::evaluateNode()：直接返回持有的文本，不做任何解析或
         *          隐式转换。
         * @return 文本内容
         */
        [[nodiscard]] Value evaluateNode() const override;

        /**
         * @brief 追加本节点的文本
         * @details 重写 Expression::appendText()：用 << >> 定界，正文里的反斜杠、'>'、'#' 与
         *          控制字符一并转义，使文本可被词法分析器原样读回。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 Expression::copyNode()：复制文本内容。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;

        /**
         * @brief 本节点后面能否直接跟分量
         * @details 重写 Expression::isIndexable()：文本可按 UTF-8 字符下标取子值，如 <<abc>>[1]，
         *          因此返回 true（基类默认为 false）。
         * @return 恒为 true
         */
        [[nodiscard]] bool isIndexable() const override;

    private:
        std::string m_text; ///< 文本内容
    };

    /**
     * @brief 取值节点：承载已经算出来的几何值
     */
    class ValueExpression : public Expression
    {
    public:
        /**
         * @brief 构造取值节点
         * @param resolver 对象解析器，可为空
         * @param value 取值
         */
        explicit ValueExpression(IObjectResolver *resolver = nullptr, Value value = Value());

        /**
         * @brief 取取值
         * @return 取值
         */
        [[nodiscard]] const Value &getValue() const noexcept;

        /**
         * @brief 化简本节点
         * @details 重写 Expression::simplify()：取值已是常量，复制自身而不新建节点。
         * @return 本节点的副本
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /**
         * @brief 节点种类名
         * @details 重写 Expression::nodeName()：固定返回 "Value"。
         * @return "Value"
         */
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 重写 Expression::evaluateNode()：直接返回持有的取值，几何值不经分量解释。
         * @return 本节点持有的取值
         */
        [[nodiscard]] Value evaluateNode() const override;

        /**
         * @brief 追加本节点的文本
         * @details 重写 Expression::appendText()：几何值写成可重新解析的函数调用形式
         *          （如 vector(...)、placement(...)），数值与文本走各自的写法。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 Expression::copyNode()：复制取值本身。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;

    private:
        Value m_value; ///< 取值
    };

    /**
     * @brief 单元格区间节点
     * @details 区间本身没有标量取值，只能作为聚合函数的实参使用；直接求值会明确报错，
     *          避免把区间悄悄当成一个单元格。支持的名字是单元格地址，别名由宿主的
     *          属性容器以属性名形式提供。
     */
    class RangeExpression : public Expression
    {
    public:
        /**
         * @brief 构造区间节点
         * @param resolver 对象解析器，可为空
         * @param begin 起始单元格地址文本
         * @param end 结束单元格地址文本
         */
        explicit RangeExpression(IObjectResolver *resolver = nullptr, std::string begin = std::string(), std::string end = std::string());

        ~RangeExpression() override;

        /**
         * @brief 取起始地址文本
         * @return 起始地址文本
         */
        [[nodiscard]] std::string getBegin() const;

        /**
         * @brief 取结束地址文本
         * @return 结束地址文本
         */
        [[nodiscard]] std::string getEnd() const;

        /**
         * @brief 取区间
         * @return 区间对象
         * @throws EvaluationError 首尾地址不是合法单元格地址
         */
        [[nodiscard]] Range getRange() const;

        /**
         * @brief 化简本节点
         * @details 重写 Expression::simplify()：区间不是标量常量，折叠会丢掉聚合函数所需的
         *          地址信息，因此原样复制。
         * @return 本节点的副本
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /**
         * @brief 节点种类名
         * @details 重写 Expression::nodeName()：固定返回 "Range"，供相等判定识别区间。
         * @return "Range"
         */
        [[nodiscard]] std::string_view nodeName() const override;

        /**
         * @brief 取本节点的区间表达式视图
         * @details 重写 Expression::asRangeExpression()：本类节点直接返回自身，省去基类的
         *          一次类型判断，聚合函数据此识别区间实参。
         * @return 本节点自身
         */
        [[nodiscard]] const RangeExpression *asRangeExpression() const noexcept override;

    protected:
        /**
         * @brief 节点自身的求值
         * @details 重写 Expression::evaluateNode()：区间没有标量取值，恒定报错并提示改用
         *          聚合函数，而不是返回首单元格或空值。
         * @return 不返回
         * @throws EvaluationError 区间被当成标量取值
         */
        [[nodiscard]] Value evaluateNode() const override;

        /**
         * @brief 追加本节点的文本
         * @details 重写 Expression::appendText()：写成「起始:结束」形式，使文本能解析回同一
         *          区间。
         * @param text 输出：在末尾追加本节点文本
         * @param persistent true 时生成可回填、可持久化的文本
         * @param indent 缩进层级，预留给多行排版
         */
        void appendText(std::string &text, bool persistent, int indent) const override;

        /**
         * @brief 生成同类型的空壳副本
         * @details 重写 Expression::copyNode()：复制首尾地址文本。
         * @return 同类型节点的新副本
         */
        [[nodiscard]] ExpressionPtr copyNode() const override;

    private:
        std::string m_begin; ///< 起始地址文本
        std::string m_end;   ///< 结束地址文本
    };

} // namespace ExpressionEngine::Expression
