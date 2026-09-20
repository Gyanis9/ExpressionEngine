# 更新日志

本文件记录 ExpressionEngine 的显著变更：格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循[语义化版本](https://semver.org/lang/zh-CN/)。接口细节一律以头文件注释为准，这里只写「宿主会
感觉到什么」。

## [1.0.0] - 未发布

首个公开发布版本。条目相对早期的 FreeCAD 移植形态；打标签时本节即为发布说明。

### 新增

- **宿主注册函数**：`FunctionRegistry` 收 `CustomFunctionSpec`（名字、回调、实参个数上下限、用途说明）。
  内置表查不到的名字转向进程级默认注册表，也可以按次传入宿主自己的注册表，两种写法表达式文本一致。
- **值模型加入序列**：`Value` 现有 9 个候选类型（数量、double、bool、文本、向量、矩阵、旋转、位姿、序列）。
  `list(1; 2 mm)[0]` 可建可取；聚合函数会把序列实参摊平，`sum(list(1; 2))` 与 `sum(1; 2)` 同义。
- **序列与文本互转**：`join(序列; 分隔符)`、`split(文本; 分隔符)`，两者互为逆运算（连续与末尾分隔符留空段）。
- **按 UTF-8 字符计的文本函数**：`len`、`upper`、`lower`、`trim`、`substr`、`contains`、`replace`、`concat`。
- **内置名字到取值的字典**：`Dictionary` 直接实现宿主接口，不写 `IProperty`/`IObject` 也能喂变量，
  支持 `Box.Length` 这类子字典、写回与只读标记。
- **非异常错误通道**：`ExpressionParser::tryParse`、`Expression::tryEvaluate`、`QuantityParser::tryParse`
  以 `std::expected` 返回 `ParseFailure` / `EvaluationFailure`，文案与异常通道逐字相同。
- **可解析成数量的文本参与运算**：算术操作数、单元格读取与聚合函数都走 `toQuantity` 的文本通道，
  `sum(split(<<1 mm; 2 mm>>; <<; >>))` 直接可算。

### 变更

- **表达式文本可往返**：回写结果保证解析回同一棵树。文本取值改用 `<<…>>` 定界（`"` 是英寸符号，
  不再兼任字符串定界符）；单位常量按 `2 * mm` 写出；折成常量的节点保留量纲；乘方按右结合回写，
  左结合的写法保住括号（`(2^3)^4` 不再被写成 `2 ^ 3 ^ 4`）；跨文档引用 `<<Sheet#A1>>` 不再丢文档名。
- **解析限深 100 层**：表达式解析器与数量文本解析器都在超深嵌套时报中文解析错，
  不再让宿主进程撞上无法捕获的栈溢出。
- **区间语义**：`sum(B2:A1)` 与 `sum(A1:B2)` 同义（端点先整理再遍历），反向拖选不再只算起点；
  `Range::normalize()` 同时把游标复位到左上角。
- **类型与配置口径收紧**：`Quantity::getValueAs` 要求参照量同量纲（此前跨量纲也给得出数）；
  分数分母为 0 时排版报错（此前把任何长度都写成 `0`）；`Base::convertTo` 的依赖类型名补齐
  `typename`，换严格按标准的编译器不再编译失败。
- **单位书写优先级**：单位后置与 `*`、`/` 同级，`3 mm * 4 mm` 是面积、`60 mm / 4 s` 是长度×时间；
  单位乘除的前瞻只认真正的单位符号，`(2 m) / (4 s)` 可原样回读。

### 移除

- `create()` 占位函数删除，`tuple()` 收成 `list()` 的名字别名（函数枚举里的 `Create`、`Tuple` 一并去掉）。
- flex + bison 生成的单位解析器换成手写词法与递归下降，仓库不再生成代码；Qt / `QLocale` 依赖换成
  自带的区域数字格式层（`QuantityFormat` 的选项位仍与 `QLocale::NumberOptions` 数值对齐，便于沿用旧配置值）。

## 从旧版迁移

按需要动手的顺序排列，只列宿主会碰到的：

1. **表达式存档**：多数旧文本仍可解析，但保存时会被改写成新形态。唯一的硬不兼容是用 `"…"` 写的
   文本取值：`"` 现在只作英寸符号，这类文本要改成 `<<…>>` 定界。
2. **函数名**：`create(...)` 改为构造对应几何值或用宿主注册的函数；`tuple(...)` 改 `list(...)`
   （不改文本也仍可运行，它已是别名）。
3. **异常面**：`getValueAs` 与分数排版在输入不合法时改抛 `Base::UnitsMismatchError` /
   `Base::ValueError`；超深嵌套从「进程消失」变成 `Base::ParserError`，`tryParse` / `tryEvaluate`
   可以不走异常拿到同一份文案。
4. **区间**：依赖 `sum(B2:A1)` 旧行为（只算起点）的代码要复核一次——现在按整片算。
5. **反向调用**：`OperatorExpression::isLeftAssociative()` 从静态方法变成 const 成员，
   需经对象调用；`Vector3::projectToLine(point, line)` 现在真正把调用者投到直线上并写回垂足
   （旧实现不读调用者，只返回与它无关的位移）。
