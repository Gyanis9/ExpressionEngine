/**
 * @file Expression.h
 * @brief 表达式抽象语法树与求值
 * @author Gyanis
 * @date 2026-09-18
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Gyanis. LGPL-2.1-or-later，派生自 FreeCAD
 */

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ExpressionEngine/Base/Exception.h>
#include <ExpressionEngine/Base/Matrix.h>
#include <ExpressionEngine/Base/Placement.h>
#include <ExpressionEngine/Base/Rotation.h>
#include <ExpressionEngine/Base/Vector3D.h>
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

    /// 表达式节点的所有权句柄；子节点由父节点独占持有
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
        /// 分量种类
        enum class ComponentKind
        {
            Name,  ///< 属性路径上的一段名字，如 .Rotation
            Index, ///< 数组下标，如 [0]
            Range, ///< 区间，如 [1:3]
            MapKey ///< 映射键，如 ['Length']
        };

        /// 引用路径上的一段分量
        struct Component
        {
            ComponentKind kind{ComponentKind::Name}; ///< 分量种类
            std::string   name;                      ///< Name 与 MapKey 分量的文本
            ExpressionPtr index;                     ///< Index 与 Range 分量的起始下标
            ExpressionPtr endIndex;                  ///< Range 分量的结束下标；空表示开放区间
            ExpressionPtr step;                      ///< Range 分量的步长；空表示步长 1

            Component() = default;

            /// 造一个名字分量，如 .Rotation
            explicit Component(std::string componentName);

            Component(const Component &other);
            Component(Component &&other) noexcept;
            ~Component();
            Component &operator=(const Component &other);
            Component &operator=(Component &&other) noexcept;

            /// 造一个数组下标分量，如 [0]
            static Component arrayIndex(ExpressionPtr indexExpression);

            /// 造一个映射键分量，如 ['Length']
            static Component mapKey(std::string key);

            /// 造一个区间分量，如 [1:3]；endIndex 为空表示开放区间
            static Component rangeComponent(ExpressionPtr begin, ExpressionPtr end, ExpressionPtr stepExpression = nullptr);

            /// 两段分量是否结构相同（名字相同、下标表达式结构相同）
            [[nodiscard]] bool isSame(const Component &other) const;

            /// 追加分量的文本写法
            void appendText(std::string &text, bool persistent) const;
        };

        using ComponentList = std::vector<Component>;

        /// 默认优先级；优先级数值更小的节点在合成文本时需要括号
        static constexpr int s_defaultPriority = 20;

        /**
         * @brief 构造表达式节点
         * @param resolver 对象解析器，可为空；为空时引用型节点求值会明确报错
         */
        explicit Expression(IObjectResolver *resolver = nullptr);

        virtual ~Expression();

        Expression(const Expression &)            = delete;
        Expression &operator=(const Expression &) = delete;

        /// 取对象解析器，可为空
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
        [[nodiscard]] ExpressionPtr eval() const;

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

        /// 深拷贝，连同分量与注释
        [[nodiscard]] ExpressionPtr copy() const;

        /**
         * @brief 判断两棵树是否相同
         * @param other 另一棵树
         * @param checkComment true 时注释也要相同
         * @return 节点种类、持久化文本与注释都相同时为 true
         */
        [[nodiscard]] bool isSame(const Expression &other, bool checkComment = true) const;

        /// 是否带分量
        [[nodiscard]] bool hasComponent() const noexcept;

        /// 取分量列表
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

        /// 取注释
        [[nodiscard]] const std::string &comment() const noexcept;

        /// 设置注释
        void setComment(std::string text);

        /// 节点种类名，用于相等判定与诊断
        [[nodiscard]] virtual std::string_view nodeName() const = 0;

        /// 本节点是区间表达式时返回自身，否则返回 nullptr；供聚合函数识别区间参数
        [[nodiscard]] virtual const RangeExpression *asRangeExpression() const noexcept;

        /// 本节点是运算符节点时返回自身，否则返回 nullptr；供文本化判断结合性
        [[nodiscard]] virtual const OperatorExpression *asOperatorExpression() const noexcept;

        /// 本节点是否已是常量数值（数值节点或命名常量节点）；供常量折叠判断
        [[nodiscard]] virtual bool isConstantNumeric() const noexcept;

    protected:
        /// 节点自身的求值；分量能否作用在求值结果上由 supportsComponentAccess() 约定
        [[nodiscard]] virtual Value evaluateNode() const = 0;

        /// 追加本节点的文本
        virtual void appendText(std::string &text, bool persistent, int indent) const = 0;

        /// 生成同类型的空壳副本，分量与注释由 copy() 补齐
        [[nodiscard]] virtual ExpressionPtr copyNode() const = 0;

        /// 本节点后面能否直接跟分量
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
         * @param out 输出：按首次出现顺序追加引用
         */
        virtual void _collectReferences(std::vector<VariableReference> &out) const;

        /**
         * @brief 供复合节点的递归钩子把子表达式的引用追加到同一列表
         * @details 输出列表由调用方复用，去重只在 collectReferences() 做一次；空指针表示
         *          该分支不存在，直接跳过。
         * @param expression 子表达式；可为空
         * @param out 输出：追加收集到的引用
         */
        static void collectReferencesFrom(const Expression *expression, std::vector<VariableReference> &out);

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
     * @details 同时是数值、运算符、函数与变量节点的基类：FreeCAD 里这些节点的"取值"都是
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
        explicit UnitExpression(IObjectResolver *resolver = nullptr, const Units::Quantity &quantity = Units::Quantity(), const std::string &unitText = std::string());

        ~UnitExpression() override;

        /// 设置数量
        void setQuantity(const Units::Quantity &quantity);

        /// 设置数量；与 setQuantity() 等价，保留 FreeCAD 的命名以便对照
        void setUnit(const Units::Quantity &quantity);

        /// 取数值（以基准量纲表示）
        [[nodiscard]] double getValue() const;

        /// 取单位（量纲）
        [[nodiscard]] const Units::Unit &getUnit() const;

        /// 取数量
        [[nodiscard]] const Units::Quantity &getQuantity() const;

        /// 取单位原文
        [[nodiscard]] std::string getUnitText() const;

        /// 取比例系数，等价于 getValue()
        [[nodiscard]] double getScaler() const;

        /// 化简：单位节点本身就是常量，返回数值节点
        [[nodiscard]] ExpressionPtr simplify() const override;

        /// 节点种类名
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        [[nodiscard]] Value evaluateNode() const override;

        void appendText(std::string &text, bool persistent, int indent) const override;

        [[nodiscard]] ExpressionPtr copyNode() const override;

    private:
        Units::Quantity m_quantity; ///< 数量
        std::string     m_unitText; ///< 单位原文
    };

    /// 数值节点：一个带单位的常量
    class NumberExpression : public UnitExpression
    {
    public:
        /**
         * @brief 构造数值节点
         * @param resolver 对象解析器，可为空
         * @param quantity 数量
         */
        explicit NumberExpression(IObjectResolver *resolver = nullptr, const Units::Quantity &quantity = Units::Quantity());

        /// 化简：数值节点已经是常量，返回自身副本
        [[nodiscard]] ExpressionPtr simplify() const override;

        /// 取负
        void negate();

        /**
         * @brief 取整数取值
         * @return 数值为整数时返回该整数（可超出 int 范围），否则返回空
         */
        [[nodiscard]] std::optional<long> integerValue() const;

        /// 节点种类名
        [[nodiscard]] std::string_view nodeName() const override;

        /// 数值节点本身就是常量数值
        [[nodiscard]] bool isConstantNumeric() const noexcept override;

    protected:
        void appendText(std::string &text, bool persistent, int indent) const override;

        [[nodiscard]] ExpressionPtr copyNode() const override;
    };

    /// 命名常量节点：True、False 等有名字的常量，取值与名字同时保留
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

        /// 取常量名
        [[nodiscard]] std::string getName() const;

        /// 常量是否按数值参与运算；True 与 False 是布尔值，不算数值
        [[nodiscard]] bool isNumber() const;

        /// 节点种类名
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        [[nodiscard]] Value evaluateNode() const override;

        void appendText(std::string &text, bool persistent, int indent) const override;

        [[nodiscard]] ExpressionPtr copyNode() const override;

    private:
        std::string m_name; ///< 常量名
    };

    /// 运算符节点：一元与二元运算
    class OperatorExpression : public UnitExpression
    {
    public:
        /// 运算符
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

        /// 取运算符
        [[nodiscard]] Operator getOperator() const noexcept;

        /// 取左操作数
        [[nodiscard]] const Expression *getLeft() const noexcept;

        /// 取右操作数；一元运算符返回 nullptr
        [[nodiscard]] const Expression *getRight() const noexcept;

        /// 设置左操作数
        void setLeft(ExpressionPtr expression);

        /// 设置右操作数
        void setRight(ExpressionPtr expression);

        /// 化简：两侧都是常量时折叠求值，否则重建节点
        [[nodiscard]] ExpressionPtr simplify() const override;

        /// 取优先级，与 FreeCAD 一致：比较 1、加减 3、乘除取余 4、幂 5、一元与单位 6
        [[nodiscard]] int priority() const override;

        /// 运算是否可交换
        [[nodiscard]] bool isCommutative() const;

        /// 运算是否左结合
        [[nodiscard]] bool isLeftAssociative() const;

        /// 运算是否右结合
        [[nodiscard]] bool isRightAssociative() const;

        /// 取运算符的文本写法
        [[nodiscard]] static std::string_view operatorText(Operator operation);

        /// 取文本对应的运算符；无法识别时返回 None
        [[nodiscard]] static Operator operatorFromText(std::string_view text);

        /// 节点种类名
        [[nodiscard]] std::string_view nodeName() const override;

        /**
         * @brief 取本节点的运算符表达式视图
         * @details 重写 Expression::asOperatorExpression()：本类节点直接返回自身，
         *          省去基类的一次类型判断，文本化时判断结合性会频繁用到。
         * @return 本节点自身
         */
        [[nodiscard]] const OperatorExpression *asOperatorExpression() const noexcept override;

    protected:
        [[nodiscard]] Value evaluateNode() const override;

        void appendText(std::string &text, bool persistent, int indent) const override;

        [[nodiscard]] ExpressionPtr copyNode() const override;

        /**
         * @brief 递归收集左右操作数里的变量引用
         * @details 覆写基类的空实现：运算符自己不产生依赖，但两侧子树可能引用宿主属性；
         *          先左后右追加以保持「首次出现」顺序。分量索引表达式由解析器保证为常量，
         *          不参与收集。
         * @param out 输出：追加两侧子树的引用
         */
        void _collectReferences(std::vector<VariableReference> &out) const override;

    private:
        Operator      m_operator; ///< 运算符
        ExpressionPtr m_left;     ///< 左操作数
        ExpressionPtr m_right;    ///< 右操作数
    };

    /// 三元条件节点：条件 ? 真分支 : 假分支
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
        explicit ConditionalExpression(IObjectResolver *resolver = nullptr, ExpressionPtr condition = nullptr, ExpressionPtr trueExpression = nullptr,
                                       ExpressionPtr falseExpression = nullptr);

        ~ConditionalExpression() override;

        /// 取条件表达式
        [[nodiscard]] const Expression *getCondition() const noexcept;

        /// 取真分支
        [[nodiscard]] const Expression *getTrueExpression() const noexcept;

        /// 取假分支
        [[nodiscard]] const Expression *getFalseExpression() const noexcept;

        /**
         * @brief 化简
         * @details 条件化简后仍是常量时直接返回被选中分支的化简结果，否则重建节点。
         */
        [[nodiscard]] ExpressionPtr simplify() const override;

        /// 取优先级，与 FreeCAD 一致为 2
        [[nodiscard]] int priority() const override;

        /// 节点种类名
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        [[nodiscard]] Value evaluateNode() const override;

        void appendText(std::string &text, bool persistent, int indent) const override;

        [[nodiscard]] ExpressionPtr copyNode() const override;

        /**
         * @brief 递归收集条件与两个分支里的变量引用
         * @details 覆写基类的空实现：条件恒定时分支可能不参与求值，但引用仍要全部收上，
         *          否则宿主会漏建依赖；按条件、真分支、假分支的顺序追加。
         * @param out 输出：追加三个子表达式的引用
         */
        void _collectReferences(std::vector<VariableReference> &out) const override;

    private:
        ExpressionPtr m_condition;       ///< 条件表达式
        ExpressionPtr m_trueExpression;  ///< 真分支
        ExpressionPtr m_falseExpression; ///< 假分支
    };

    /// 函数调用节点
    class FunctionExpression : public UnitExpression
    {
    public:
        /**
         * @brief 函数种类
         * @details 名字与 FreeCAD 的函数表一一对应；Create、List、Tuple 依赖宿主的对象工厂，
         *          本库保留条目但在构造时即报错，以免表达式被误当成可用。
         */
        enum class Function
        {
            None,

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

            /// 逻辑
            LogicalNot, ///< not：逻辑非

            /// 聚合函数的哨兵，本身不是函数；与其后的聚合函数相邻，便于范围判断
            Aggregates,

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
        explicit FunctionExpression(IObjectResolver *resolver = nullptr, Function function = Function::None, std::string name = std::string(),
                                    std::vector<ExpressionPtr> arguments = std::vector<ExpressionPtr>());

        ~FunctionExpression() override;

        /// 取函数种类
        [[nodiscard]] Function getFunction() const noexcept;

        /// 取实参
        [[nodiscard]] const std::vector<ExpressionPtr> &getArguments() const noexcept;

        /**
         * @brief 化简
         * @details 全部实参都能化简成数值节点时直接求值，否则重建节点。
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

        /// 取函数的规范名，如 "sqrt"
        [[nodiscard]] static std::string_view functionName(Function function);

        /// 取名字对应的函数；无法识别时返回 Function::None
        [[nodiscard]] static Function functionFromName(std::string_view name);

        /// 节点种类名
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        [[nodiscard]] Value evaluateNode() const override;

        void appendText(std::string &text, bool persistent, int indent) const override;

        [[nodiscard]] ExpressionPtr copyNode() const override;

        /**
         * @brief 递归收集全部实参里的变量引用
         * @details 覆写基类的空实现：普通实参与聚合函数的区间实参都可能引用宿主属性，
         *          按实参顺序追加，保证依赖列表与表达式里的出现顺序一致。
         * @param out 输出：追加各实参的引用
         */
        void _collectReferences(std::vector<VariableReference> &out) const override;

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
        /// 引用路径的临时表示；与命名空间作用域的 VariableReference 是同一类型
        using Reference = VariableReference;

        /**
         * @brief 构造变量引用节点
         * @param resolver 对象解析器，可为空
         * @param reference 引用路径
         */
        explicit VariableExpression(IObjectResolver *resolver = nullptr, Reference reference = Reference());

        ~VariableExpression() override;

        /// 取引用路径
        [[nodiscard]] const Reference &getReference() const noexcept;

        /// 设置引用路径
        void setReference(Reference reference);

        /// 属性名
        [[nodiscard]] std::string name() const;

        /// 引用的文本写法，如 "Part.Box.Length"，用于报错
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
        void assignValue(const Value &newValue);

        /// 化简：引用节点本身是叶子，返回自身副本
        [[nodiscard]] ExpressionPtr simplify() const override;

        /// 节点种类名
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        [[nodiscard]] Value evaluateNode() const override;

        void appendText(std::string &text, bool persistent, int indent) const override;

        [[nodiscard]] ExpressionPtr copyNode() const override;

        /// 引用后面可以直接跟分量与下标，如 Box.Length[0]
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
         * @details 覆写基类的空实现：引用节点是依赖的来源，直接把 m_reference 追加进 out；
         *          分量里只有解析器保证为常量的下标表达式，没有可依赖的变量。
         * @param out 输出：追加自身引用
         */
        void _collectReferences(std::vector<VariableReference> &out) const override;

    private:
        Reference m_reference; ///< 引用路径
    };

    /// 文本节点；求值结果就是文本本身
    class StringExpression : public Expression
    {
    public:
        /**
         * @brief 构造文本节点
         * @param resolver 对象解析器，可为空
         * @param text 文本内容
         */
        explicit StringExpression(IObjectResolver *resolver = nullptr, std::string text = std::string());

        /// 取文本内容
        [[nodiscard]] std::string getText() const;

        /// 化简：文本已是常量，返回自身副本
        [[nodiscard]] ExpressionPtr simplify() const override;

        /// 节点种类名
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        [[nodiscard]] Value evaluateNode() const override;

        void appendText(std::string &text, bool persistent, int indent) const override;

        [[nodiscard]] ExpressionPtr copyNode() const override;

        [[nodiscard]] bool isIndexable() const override;

    private:
        std::string m_text; ///< 文本内容
    };

    /// 取值节点：承载已经算出来的几何值，等价于 FreeCAD 的 PyObjectExpression
    class ValueExpression : public Expression
    {
    public:
        /**
         * @brief 构造取值节点
         * @param resolver 对象解析器，可为空
         * @param value 取值
         */
        explicit ValueExpression(IObjectResolver *resolver = nullptr, Value value = Value());

        /// 取取值
        [[nodiscard]] const Value &getValue() const noexcept;

        /// 化简：取值已是常量，返回自身副本
        [[nodiscard]] ExpressionPtr simplify() const override;

        /// 节点种类名
        [[nodiscard]] std::string_view nodeName() const override;

    protected:
        [[nodiscard]] Value evaluateNode() const override;

        void appendText(std::string &text, bool persistent, int indent) const override;

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

        /// 取起始地址文本
        [[nodiscard]] std::string getBegin() const;

        /// 取结束地址文本
        [[nodiscard]] std::string getEnd() const;

        /**
         * @brief 取区间
         * @return 区间对象
         * @throws EvaluationError 首尾地址不是合法单元格地址
         */
        [[nodiscard]] Range getRange() const;

        /// 化简：区间节点保持原样，返回自身副本
        [[nodiscard]] ExpressionPtr simplify() const override;

        /// 节点种类名
        [[nodiscard]] std::string_view nodeName() const override;

        /// 返回自身，供聚合函数识别
        [[nodiscard]] const RangeExpression *asRangeExpression() const noexcept override;

    protected:
        [[nodiscard]] Value evaluateNode() const override;

        void appendText(std::string &text, bool persistent, int indent) const override;

        [[nodiscard]] ExpressionPtr copyNode() const override;

    private:
        std::string m_begin; ///< 起始地址文本
        std::string m_end;   ///< 结束地址文本
    };

} // namespace ExpressionEngine::Expression
