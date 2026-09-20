# ExpressionEngine

面向宿主应用的可复用表达式引擎与单位模块：装上头文件、链一个静态库，就能在自己的对象模型上
解析与求值表达式、处理带单位的数量。C++23 实现，零外部依赖。

- **表达式**：`Box.Length * 2`、`cells[0:5]`、`<<Part>>.Box.Length` 这类引用与运算都能解析求值，
  支持函数、条件运算、区间聚合、序列取值（`list(1; 2 mm)[0]`）与分量取值，并提供文本回写与
  依赖收集。
- **单位**：预定义单位与换算、带单位的数量 `Quantity`、多套单位方案（如 Internal、ImperialDecimal），
  能解析 `1/2 mm`、`5' 6"`、`2 m/s` 这类写法。
- **宿主解耦**：对象模型通过 4 个抽象接口接入，库不持有对象树的任何所有权；函数集也可由宿主
  注册扩充，无需改库源码。
- **工程性**：手写词法与 Pratt 解析器（无生成代码）、中文可操作报错、可恢复错误走 `std::expected`、
  零编译告警。

## 快速开始

```cpp
#include <ExpressionEngine/Expression/ExpressionParser.h>
#include <ExpressionEngine/Units/UnitsApi.h>

#include <cstdio>
#include <variant>

int main()
{
    // 没有宿主对象模型也能用：resolver 传 nullptr，纯数量表达式照样求值
    const auto expression = ExpressionEngine::Expression::ExpressionParser::parse(nullptr, "1/2 mm + 2 mm");

    const ExpressionEngine::Expression::Value value = expression->evaluate();
    const auto *length = std::get_if<ExpressionEngine::Units::Quantity>(&value);
    if (length == nullptr)
    {
        return 1;
    }

    // 按当前单位方案换算并排版；默认是毫米制、保留两位小数，输出 "2.50 mm"
    std::printf("%s\n", ExpressionEngine::Units::UnitsApi::schemaTranslate(*length).c_str());
    return 0;
}
```

只想解析一条数量文本时，可以直接用 `Units::Quantity::parse("5' 6\"")`。

解析失败有两条通道、文案完全一致，按场景选用：

```cpp
// 异常通道：解析失败抛 Base::ParserError，消息含出错列号
const auto expression = ExpressionEngine::Expression::ExpressionParser::parse(&resolver, text);

// 非异常通道：可恢复错误以值返回，适合用户输入这类场景
if (const auto parsed = ExpressionEngine::Expression::ExpressionParser::tryParse(&resolver, text))
{
    use(parsed.value());
}
else
{
    showError(parsed.error().message);
}
```

## 集成到你的项目

### 方式一：安装后 find_package（推荐）

```sh
cmake --preset release
cmake --build build/release
cmake --install build/release --prefix /your/prefix
```

安装树里只有头文件、静态库、CMake 包配置与许可文件：

```
include/ExpressionEngine/{Base,Units,Expression}/*.h
lib/libExpressionEngine.a              # Windows 上为 ExpressionEngine.lib
lib/cmake/ExpressionEngine/            # find_package 用的配置与目标文件
share/doc/ExpressionEngine/LICENSE
```

你的工程里只需要两行：

```cmake
find_package(ExpressionEngine REQUIRED)
target_link_libraries(your_app PRIVATE ExpressionEngine::ExpressionEngine)
```

导出目标自带包含路径、C++23 标准与 MSVC 下的 `/utf-8`（头文件含中文注释，必须它才能正确编译），
下游无需手工补任何编译选项。

注意：MSVC 下消费工程的运行时库必须与安装的库一致——装的是 Release 库，消费工程就用 Release
构建（链接器会以 `LNK2038` 拒绝混用）。

### 方式二：作为源码子目录（add_subdirectory）

```cmake
set(EXPRESSIONENGINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)   # 跳过本库用例（它们需要 GoogleTest）
add_subdirectory(third_party/ExpressionEngine)

target_link_libraries(your_app PRIVATE ExpressionEngine::ExpressionEngine)   # 与安装方式同一个目标名
```

### 方式三：Conan

仓库自带配方，包内容与手工安装同源（不含用例与基准）：

```sh
conan create . -s build_type=Release --build=missing    # 产出包 expressionengine/1.0.0
```

库本体零依赖；GoogleTest 只服务于用例，首次运行会按需在本机构建它。消费方在自己的
`conanfile` 里 `requires("expressionengine/1.0.0")`；CMake 侧仍是
`find_package(ExpressionEngine)` + `ExpressionEngine::ExpressionEngine`，配方已把目标名映射好。

### 方式四：手动集成

把 `src/ExpressionEngine/` 加入包含路径、把编译出的 `ExpressionEngine.lib` 加入链接即可。MSVC
下需在消费方开启 `/utf-8`（头文件含中文注释，用前三种方式集成时会自动带上）。

## 宿主对象模型接入

库通过四个抽象接口读写你的对象树：

| 接口 | 职责 |
| --- | --- |
| `IProperty` | 单个属性的读（`value()`）与写（`setValue()`） |
| `IPropertyContainer` | 按名找属性（`findProperty()`），对象的子分量同样实现本接口 |
| `IObject` | 带文档名的容器（`documentName()`），支撑 `<<Part>>.Box.Length` 这类限定引用 |
| `IObjectResolver` | 把引用解析成对象（`resolve()`），并提供对象名列表用于报错与补全 |

接入后即可解析带宿主引用与赋值的表达式：

```cpp
const auto expression = ExpressionEngine::Expression::ExpressionParser::parse(&resolver, "Box.Length * 2");
const ExpressionEngine::Expression::Value doubled = expression->evaluate();   // 经 IProperty::value() 读
```

依赖收集用 `expression->collectReferences()`（返回全部变量引用），宿主据此建立重算依赖。
完整可照抄的实现见 `tests/ExpressionEngine/Expression/TestExpression.cpp` 里的
`FakeProperty` / `FakeObject` / `FakeResolver`。

### 扩充函数集

内置函数表之外的名字转向 `FunctionRegistry` 查询，宿主不改库源码就能加自己的函数：

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

回调拿到的是 `FunctionCall`（实参表达式），可以只对自己需要的分支求值，从而做出条件与短路语义；
不带 `registry` 参数的解析重载查询进程级 `FunctionRegistry::global()`。函数名须是词法器认可的
函数名写法、区分大小写，且不与内置函数重名——登记失败以 `std::expected` 返回中文原因。

完整可照抄的实现见 `tests/ExpressionEngine/Expression/TestFunctionRegistry.cpp`。

## 从源码构建

```sh
cmake --preset debug        # 调试构建，产物在 build/debug
cmake --preset release      # 优化构建（用例的验收构建），产物在 build/release
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

- 库本体零外部依赖；只有构建用例时才需要 GoogleTest——预设会经 `conan_provider.cmake` 自动
  执行 `conan install`（首次需要本机已安装 Conan）。
- 开关：`EXPRESSIONENGINE_BUILD_TESTS`（默认 ON）、`EXPRESSIONENGINE_BUILD_BENCHMARKS`（默认 OFF）。
- 告警口径：MSVC 用 `/W4 /permissive- /utf-8 /Zc:__cplusplus`，其它编译器用
  `-Wall -Wextra -Wpedantic`；零告警是提交硬判据。

## 性能

手写解析器与生成式解析器（flex/bison 生成代码）在同源语料上的对照实测：两侧共用同一套
`Quantity` 运算，且逐位校验两侧结果一致。

数量解析（Release / i5-14600KF，中位数）：

| 输入 | 手写解析器 | 相对生成代码 |
| --- | --- | --- |
| `1.5 mm` | 0.135 µs | 快 1.66× |
| `1/2 mm` | 0.177 µs | 快 1.83× |
| `5' 6"` | 0.185 µs | 快 1.84× |
| `2 m/s` | 0.194 µs | 快 1.31× |
| `1e3 kg` | 0.123 µs | 快 1.82× |
| 长式子 233 B | 84.95 MB/s | 快 1.19× |

表达式词法在首字节分派改造后，长链 66.0 → 11.6 µs（5.7×）。

复跑基准：

```sh
cmake --preset release -DEXPRESSIONENGINE_BUILD_BENCHMARKS=ON
cmake --build build/release --target ParserBenchmark
./build/release/benchmarks/ParserBenchmark
```

对照侧需要本机有一份生成代码（`Quantity.tab.c` / `Quantity.lex.c`，不随仓库分发）：用
`-DEXPRESSIONENGINE_LEGACY_QUANTITY_DIR=<目录>` 指定；目录缺失时基准只跑手写侧。

## 目录结构

```
src/ExpressionEngine/
  Base/          几何与数值基础：Vector3D、Matrix、Rotation、Placement、精度、异常、数值格式化
  Units/         单位、带单位数量、单位方案与数量解析器
  Expression/    表达式 AST、词法/语法分析、求值、宿主对象模型接口
tests/           用例（目录逐级镜像 src/）
benchmarks/      解析器性能基准
cmake/           find_package 的配置模板
```

## 许可

本库按 **LGPL-2.1-or-later** 授权，许可全文见 [`LICENSE`](LICENSE)；`cmake --install` 时会随库
装到 `share/doc/ExpressionEngine/`。

本文件不承担 API 文档职责：接口契约以 `src/ExpressionEngine/**` 头文件里的 Doxygen 注释为准。
