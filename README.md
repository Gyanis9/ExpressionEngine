# ExpressionEngine

<p align="center">
  <a href="https://github.com/Gyanis9/ExpressionEngine/actions/workflows/windows-ci.yml"><img src="https://github.com/Gyanis9/ExpressionEngine/actions/workflows/windows-ci.yml/badge.svg" alt="Windows CI"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  <img src="https://img.shields.io/badge/license-LGPL--2.1-blue.svg" alt="LGPL-2.1-or-later">
</p>

面向宿主应用的可复用**表达式引擎**与**单位模块**：链一个静态库，就能在你自己的对象模型上解析、
求值表达式并处理带单位的数量。C++23 实现，库本体零外部依赖（GoogleTest 只服务于用例），手写词法
与 Pratt 解析器，仓库内不生成任何解析代码。

- 表达式与单位两半可以单独用：没有宿主对象模型时，`resolver` 传 `nullptr`，纯数量表达式照样求值。
- 报错一律中文且写清「原因 + 替代做法」；异常与 `std::expected` 两条通道文案逐字相同。
- 接口契约以 `src/ExpressionEngine/**` 头文件里的 Doxygen 注释为准，本文件不承担 API 文档职责。

## 快速开始

```cpp
#include <ExpressionEngine/Expression/ExpressionParser.h>
#include <ExpressionEngine/Units/UnitsApi.h>

#include <cstdio>
#include <variant>

int main()
{
    const auto expression = ExpressionEngine::Expression::ExpressionParser::parse(nullptr, "1/2 mm + 2 mm");
    const ExpressionEngine::Expression::Value value = expression->evaluate();

    const auto *length = std::get_if<ExpressionEngine::Units::Quantity>(&value);
    if (length == nullptr)
    {
        return 1;
    }

    // 按当前单位方案换算并排版；默认 Internal 方案、毫米、两位小数
    std::printf("%s\n", ExpressionEngine::Units::UnitsApi::schemaTranslate(*length).c_str());   // 2.50 mm
}
```

只要一条数量文本，不必建表达式树：`Units::Quantity::parse("5' 6\"")`。

## 错误处理

库抛出的运行期故障全部派生自 `Base::Exception`，按处置方式分型：改文本（`ParserError`）、改单位
（`UnitsMismatchError`）、改数值范围（`OverflowError` / `UnderflowError`），其余按引擎故障记录
（`ValueError` 取值不合法、`TypeError` 类型不符、`NameError` 名字找不到、`AttributeError` 属性不可
读写、`IndexError` 下标越界、`ExpressionError` 运算本身失败）。编程错误（空指针、违反前置条件）
刻意不并入本族，直接抛 `std::invalid_argument`，免得宿主把自己的 bug 当成可恢复故障吞掉。

用户输入这类场景走非异常通道，文案与异常通道逐字相同：

```cpp
if (const auto parsed = ExpressionEngine::Expression::ExpressionParser::tryParse(&resolver, text))
{
    if (const auto evaluated = parsed.value()->tryEvaluate())
    {
        use(evaluated.value());
    }
    else
    {
        showError(evaluated.error().message);   // 量纲不符、引用解析不到……
    }
}
else
{
    showError(parsed.error().message);          // 已含出错列号
}
```

`tryEvaluate()` 只兜库自己的异常：宿主实现（`IProperty`、自定义函数回调）抛出的异常照旧向上传播。
超深嵌套在解析阶段就报 `ParserError`（限深 100 层），不会撞上无法捕获的栈溢出。

## 表达式能写什么

| 写法 | 含义 |
| --- | --- |
| `Box.Length * 2`、`<<Part>>.Box.Length` | 变量引用与点号分量；`<<…>>` 填的是引用里的文档名槽位 |
| `A1`、`B2:A1`、`cells[0:5]` | 单元格与区间；区间端点先整理再遍历，`sum(B2:A1)` 与 `sum(A1:B2)` 同义 |
| `<<Sheet#A1>>` | 跨文档引用 |
| `list(1; 2 mm)[0]`、`sum(list(1; 2))` | 序列取值与聚合；聚合函数会把序列实参摊平 |
| `join(序列; 分隔符)`、`split(文本; 分隔符)` | 序列与文本互转，两者互为逆运算 |
| `len`、`upper`、`substr`、`contains`、`replace`、`concat` | 文本函数，按 UTF-8 **字符**计数 |
| `x > 0 ? x : -x`、`abs(-7)`、`sqrt(16)` | 条件运算与 78 个内置函数 |
| `pi`、`e`、`True` | 引擎内置常量，字典不能覆盖 |

把结果写回宿主属性不在表达式里：文本没有赋值语句，写回走 C++ 侧的
`VariableExpression::assignValue()`（属性只读时抛 `Base::AttributeError`）。

一条书写规则值得单列：**单位后置与 `*`、`/` 同级**。所以 `3 mm * 4 mm` 是面积，而 `60 mm / 4 s`
是长度×时间——要速度得写 `(60 mm) / (4 s)`。`1/2 mm`、`5' 6"`、`2 m/s` 这类写法都能解析。

取值回写：`expression->toString(true)` 产出可存档文本，**保证解析回同一棵树**（`2 mm` 写成
`2 * mm`、`(2^3)^4` 保住括号、跨文档引用不丢文档名）。依赖收集用 `collectReferences()`，宿主据此
建立重算关系。

## 集成

### 安装后 `find_package`

```sh
cmake --preset release
cmake --build build/release
cmake --install build/release --prefix /your/prefix
```

安装树里只有头文件、静态库、CMake 包配置与随包文档（`LICENSE`、`CHANGELOG.md`）。消费工程两行就
够：

```cmake
find_package(ExpressionEngine REQUIRED)
target_link_libraries(your_app PRIVATE ExpressionEngine::ExpressionEngine)
```

导出目标自带包含路径、C++23 标准与 MSVC 下的 `/utf-8`——头文件含中文注释，缺它消费端直接编译失败，
所以下游不必也不该手工补编译选项。两条 Windows 专属注意：装的是 Release 库则消费工程也用 Release
构建（运行时库混用会被 `LNK2038` 拒绝）；直接把 `src/ExpressionEngine/` 塞进包含路径手动集成同样要开
`/utf-8`。

### 源码子目录

```cmake
set(EXPRESSIONENGINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)   # 用例需要 GoogleTest
add_subdirectory(third_party/ExpressionEngine)
target_link_libraries(your_app PRIVATE ExpressionEngine::ExpressionEngine)   # 与安装方式同一个目标名
```

### Conan

仓库自带配方，包内容与手工安装同源（不含用例与基准）：

```sh
conan create . -s build_type=Release --build=missing     # 产出 expressionengine/0.0.1
```

`requires("expressionengine/0.0.1")` 之后 CMake 侧仍是 `find_package(ExpressionEngine)`，配方已把
目标名映射好。

## 三个扩展点

### 1. 宿主对象模型

四个抽象接口把表达式接到你的对象树，库不持有对象树的任何所有权：

| 接口 | 职责 |
| --- | --- |
| `IProperty` | 单个属性的读（`value()`）与写（`setValue()`） |
| `IPropertyContainer` | 按名找属性（`findProperty()`），对象的子分量同样实现本接口 |
| `IObject` | 带文档名的容器（`documentName()`），支撑 `<<Part>>.Box.Length` 这类限定引用 |
| `IObjectResolver` | 把引用解析成对象（`resolve()`），并提供对象名列表用于报错与补全 |

可照抄的最小实现：`tests/ExpressionEngine/Expression/TestExpression.cpp` 里的
`FakeProperty` / `FakeObject` / `FakeResolver`。

**只想喂几个变量**，不必自己实现整套接口——内置的 `Dictionary` 把四个接口都实现好了：

```cpp
using namespace ExpressionEngine;

Units::Quantity millimetre(double value)
{
    return Units::Quantity(value, Units::Unit::Length);
}

Expression::Dictionary dictionary;
dictionary.define("Length", millimetre(3.0));
auto &box = dictionary.addObject("Box", std::make_unique<Expression::Dictionary>());
box.define("Length", millimetre(4.0));

const auto expression = Expression::ExpressionParser::parse(&dictionary, "Length + Box.Length");
```

条目默认可写，`VariableExpression::assignValue()` 就落在这里；`setReadOnly(true)` 的名字即普通常量。

### 2. 自定义函数

内置表查不到的名字转向 `FunctionRegistry`，不改库源码就能扩充函数集：

```cpp
// 以下示例假定写在 ExpressionEngine::Expression 命名空间内
FunctionRegistry registry;
static_cast<void>(registry.registerFunction({.name         = "taxed",
                                             .function     = [](const FunctionCall &call) {
                                                 return Units::Quantity(1.13) * toQuantity(call.argumentValue(0), "taxed 的实参");
                                             },
                                             .minArguments = 1,
                                             .maxArguments = 1}));

const auto expression = ExpressionParser::parse(nullptr, "taxed(100 mm) + 2 mm", registry);
```

回调拿到的是 `FunctionCall`（实参表达式），可以只对需要的分支求值，从而做出条件与短路语义。不带
`registry` 参数的解析重载查询进程级 `FunctionRegistry::global()`。函数名区分大小写、须是词法器认可
的写法、不与内置函数重名，登记失败以 `std::expected` 返回中文原因。可照抄的实现见
`tests/ExpressionEngine/Expression/TestFunctionRegistry.cpp`。

### 3. 单位方案

排版走哪套换算规则由 `UnitsApi` 决定：内置 10 套方案（`Internal`、`MKS`、`Imperial`、
`ImperialDecimal`、`FEM`…）可用 `setSchema()` 按名或按编号切换；宿主也能用
`UnitsSchemasDataPack` 整体换成自带的一套——

```cpp
// 以下示例假定写在 ExpressionEngine::Units 命名空间内
UnitsSchemaSpecification custom;
custom.number                  = 0;
custom.name                    = "SiteUnits";
custom.basicLengthUnitString   = "mm";
custom.translationSpecifications["Length"] = {{0, "mm", 1.0}};

UnitsApi::applyPack(UnitsSchemasDataPack{.specifications = {custom}, .defaultDecimals = 3, .defaultDenominator = 16});
```

替换后当前方案、默认小数位数与默认分数分母都取自该数据包，宿主显式设置过的精度与分母保持不动。
每个换算条目还能挂一个 `callback` 接管整段排版（内置的 `toDMS`、`toFractional` 优先于它）。不想碰
全局状态就自己构造 `UnitsSchema`。

## 从源码构建

```sh
cmake --preset debug        # 调试构建（可加 -DENABLE_SANITIZERS=ON），产物在 build/debug
cmake --preset release      # 优化构建，用例的验收构建
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

- 库本体零外部依赖；构建用例时才需要 GoogleTest——预设经 `conan_provider.cmake` 自动执行
  `conan install`（首次需要本机已装 Conan）。
- 开关：`EXPRESSIONENGINE_BUILD_TESTS`（默认 ON）、`EXPRESSIONENGINE_BUILD_BENCHMARKS`（默认 OFF）。
- 零编译告警是提交硬判据：MSVC 用 `/W4 /permissive- /utf-8 /Zc:__cplusplus`，其它编译器用
  `-Wall -Wextra -Wpedantic`。
- CI（`.github/workflows/windows-ci.yml`）在 `main` 上跑 Debug + AddressSanitizer 全量用例，之后
  `cmake --install` 到临时前缀、另起 `tools/package-check/` 工程编译并跑 28 条宿主可见行为断言——
  导出头漏装、包配置写错这类缺陷只有仓库外消费者才看得见。

## 性能

Release 构建、i5-14600KF、2026-09-20 实测（`ParserBenchmark` 一次运行的中位数；同机多次跑有
±5%~20% 抖动，长输入一列尤其明显）。数量解析与生成式解析器（flex/bison 生成代码）对照：两侧共用
同一套 `Quantity` 运算、同一份语料，且每次调用逐位校验两侧结果一致。

| 输入 | 手写解析器 | 相对生成代码 |
| --- | --- | --- |
| `1.5 mm` | 0.117 µs | 快 1.60× |
| `1/2 mm` | 0.105 µs | 快 1.81× |
| `5' 6"` | 0.118 µs | 快 1.82× |
| `2 m/s` | 0.141 µs | 快 1.35× |
| `1e3 kg` | 0.095 µs | 快 1.84× |
| 长式子 233 B | 78.6 MB/s | 快 1.17× |

解析 + 求值整条链路（每次调用含 AST 构建与释放，「分配」是全局 `operator new` 计到的堆分配次数）：

| 表达式 | 中位数 | 分配 |
| --- | --- | --- |
| `1 + 2 * 3` | 0.48 µs | 5 次 |
| `2 mm + 3 mm` | 0.64 µs | 7 次 |
| `sqrt(16) + abs(-7)` | 1.00 µs | 10 次 |
| `taxed(100 mm)`（宿主注册的函数） | 0.60 µs | 5 次 |
| `list(1; 2; 3)[1]` | 1.33 µs | 13 次 |
| `sum(list(1; 2; 3; 4)[0:2])` | 2.26 µs | 24 次 |
| `Length * 2`（字典里的名字） | 0.36 µs | 3 次 |
| `Length + Width * 2` | 0.62 µs | 5 次 |

读法：`sqrt(16) + abs(-7)` 折到单次约 0.5 µs，与宿主注册的 `taxed(100 mm)` 同量级，看不出注册表这层
额外的分派开销；字典的名字查找（虚调用 + 有序表）相对建树与求值可以忽略。序列两条最贵，因为除 AST
之外还要为序列本身与分量存储分配。词法与解析的分工：319 字节的长链只跑词法 12.1 µs（26.5 MB/s），
走完整解析 37.0 µs，50 层括号 5.2 µs。

```sh
cmake --preset release -DEXPRESSIONENGINE_BUILD_BENCHMARKS=ON
cmake --build build/release --target ParserBenchmark
./build/release/benchmarks/ParserBenchmark
```

对照侧需要本机另备一份生成代码（`Quantity.tab.c` / `Quantity.lex.c`，不随仓库分发），用
`-DEXPRESSIONENGINE_LEGACY_QUANTITY_DIR=<目录>` 指定；目录缺失时基准只跑手写侧。

## 目录结构

```
src/ExpressionEngine/
  Base/          几何与数值基础：Vector3D、Matrix、Rotation、Placement、精度、异常、数值格式化
  Units/         单位（61 个具名量纲）、带单位数量（119 个预定义量）、单位方案与数量解析器
  Expression/    表达式 AST、词法与语法分析、求值、取值模型、宿主对象模型接口
tests/           用例，目录逐级镜像 src/
benchmarks/      解析器性能基准
tools/           仓库外消费者（安装包的交付门）
cmake/           find_package 的配置模板
```

## 版本与许可

- 当前版本 **v0.0.1**。0.x 表示接口仍在收敛：破坏性变更照常记进
  [`CHANGELOG.md`](CHANGELOG.md)，宿主升级前先看那一节。
- 按 **LGPL-2.1-or-later** 授权，许可全文见 [`LICENSE`](LICENSE)；`LICENSE` 与 `CHANGELOG.md`
  都会随 `cmake --install` 装到 `share/doc/ExpressionEngine/`。
