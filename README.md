# ExpressionEngine

## 这是什么

从 FreeCAD 抽出的表达式引擎与单位模块，可脱离 FreeCAD 的文档模型独立使用：装上头文件、链一个静态库，
就能在宿主自己的对象模型上解析与求值表达式、处理带单位的数量。零外部依赖（不再需要 Qt、Python、Boost、ICU），
用 C++23 标准库实现。

## 许可与出处

本库按 **LGPL-2.1-or-later** 授权，派生自 [FreeCAD](https://www.freecad.org)（原始代码版权归 FreeCAD 项目及其
贡献者所有）：许可全文见 [`LICENSE`](LICENSE)，来源、派生与改动清单见 [`NOTICE`](NOTICE)。安装时会连同这两个
文件一起装到 `share/doc/ExpressionEngine/`。

本文件不承担 API 文档职责：接口契约以 `src/ExpressionEngine/**` 头文件里的 Doxygen 注释为准。

## 模块划分

- **Base**：几何与数值基础（`Vector3D`、`Matrix`、`Rotation`、`Placement`、`DualNumber`、`DualQuaternion`）以及
  异常、精度与数值格式化等公共设施。
- **Units**：单位定义与换算、带单位的数量 `Quantity`、单位方案（Schema），以及手写的 `QuantityParser`：
  支持预定义单位、乘除与幂、标量函数、方括号注释，以及 `5' 6"` 这类最多三段相邻数量求和。
- **Expression**：表达式 AST、词法分析器与 Pratt 解析器、求值器，以及宿主对象模型的抽象接口。

宿主接入：库不持有对象树的任何所有权。宿主需要实现 `Expression::IProperty`（单个属性的取值/赋值）与
`Expression::IPropertyContainer`（按名找属性），文档对象再实现 `Expression::IObject`（只需额外报一个
`documentName`）；最后实现 `Expression::IObjectResolver`，把 `Box.Length`、`<<Part>>.Box.Length` 这类引用
解析成实际对象。依赖追踪、单位换算、错误文案都由库负责。

## 构建与使用

### 构建（CMake 预设）

```sh
cmake --preset debug      # 调试：Debug
cmake --preset release    # 优化：库与用例的验收构建
cmake --preset bench      # 基准：Release + EXPRESSIONENGINE_BUILD_BENCHMARKS=ON
cmake --build build/release
ctest --test-dir build/release
```

三个预设都用 Ninja，产物落在 `build/<预设名>/`，并开启 `CMAKE_EXPORT_COMPILE_COMMANDS`。不用预设也可以直接配：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DEXPRESSIONENGINE_BUILD_TESTS=OFF
```

### 安装与下游使用

```sh
cmake --install build/release --prefix /your/prefix
```

装出来的树只含头文件、静态库与 CMake 包配置（`third_party/`、`tests/`、`benchmarks/` 都不装）：

```
include/ExpressionEngine/{Base,Units,Expression}/*.h
lib/libExpressionEngine.a               # Windows 上为 ExpressionEngine.lib
lib/cmake/ExpressionEngine/ExpressionEngineConfig.cmake
```

下游最小用法：

```cmake
find_package(ExpressionEngine REQUIRED)
target_link_libraries(app PRIVATE ExpressionEngine::ExpressionEngine)
```

### 基准

```sh
cmake --preset bench                                   # 等价于 -DEXPRESSIONENGINE_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/bench --target ParserBenchmark
./build/bench/benchmarks/ParserBenchmark
```

基准在 Debug 下没有意义，配置期会给出警告。

## 性能

以下数字来自同源对照实测：同一条语料、同一套 `Quantity` 运算跑两侧，且逐位校验两侧结果一致
（避免对照侧空转得出的假优势）。手写解析器一侧即本库实现；对照侧为 FreeCAD 26.3-dev 的 flex/bison
生成代码（`Quantity.tab.c` / `Quantity.lex.c`），**不随本仓分发**，需要对比时用 CMake 缓存变量
`EXPRESSIONENGINE_LEGACY_QUANTITY_DIR` 指向上游目录，目录缺失时基准只跑手写侧。

数量解析（Release / i5-14600KF，中位数）：

| 输入 | 手写解析器 | 相对生成代码 |
| --- | --- | --- |
| `1.5 mm` | 0.135 µs | 快 1.66× |
| `1/2 mm` | 0.177 µs | 快 1.83× |
| `5' 6"` | 0.185 µs | 快 1.84× |
| `2 m/s` | 0.194 µs | 快 1.31× |
| `1e3 kg` | 0.123 µs | 快 1.82× |
| 长式子 233 B | 84.95 MB/s | 快 1.19× |

表达式词法：首字节分派改造后，长链 66.0 → 11.6 µs（5.7×）。

## Conan（可选）

仓库自带 `conanfile.py`，包内容与手工安装同源：

```sh
conan create . -s build_type=Release
```

包内不含用例与基准（`EXPRESSIONENGINE_BUILD_TESTS` / `EXPRESSIONENGINE_BUILD_BENCHMARKS` 在配方里关闭），
消费方用同一套目标名：`find_package(ExpressionEngine)` + `ExpressionEngine::ExpressionEngine`。
