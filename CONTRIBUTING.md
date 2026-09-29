# 参与开发

先说清楚这个库的判据：改动合进来之前，下面几条必须自己跑过并绿。CI 只在 `main` 上跑，日常开发在
`develop` 上，所以本地跑绿是硬要求，不是建议。

## 三条硬判据

```bash
# 1. 两种构建档都零告警、全量用例通过（用例数从 ctest 汇总行取，别把数字抄进文档）
cmake --preset debug && cmake --build build/debug && ctest --test-dir build/debug --output-on-failure
cmake --preset release && cmake --build build/release && ctest --test-dir build/release --output-on-failure

# 2. 排版按仓库根的 .clang-format 判（工具版本钉死 23.1.1）
CLANG_FORMAT=/path/to/clang-format bash tools/format-check.sh

# 3. 交付门：从已提交的克隆重建整棵树，跑用例、装包，再编译并运行仓库外消费者
bash tools/package-check/run-gate.sh
```

交付门看的是**提交出去的那一版**：工作树里的未提交改动不会参与，所以「本地绿了」不等于别人拿到的那一版能编。
它能抓到导出头漏装、包配置写错、导出头里的中文注释缺 `/utf-8` 让 MSVC 消费者编译失败这类只有仓库外才看得见的缺陷。
最后一步（`consumer-refuses-mismatched-minor`）是包配置兼容判定的反面用例：它把临时安装前缀里那份版本文件自称的
版本抬到 `0.9.0`，再要求同一个消费者按 `0.0.1` 配不上——`tools/package-check/compat-refusal-check.sh` 与
Windows CI 调的是同一条脚本。

有 sanitizer 与线程检测两种插桩构建，互斥（同时开会被配置期拒绝）：

```bash
cmake --preset debug -DENABLE_SANITIZERS=ON          # AddressSanitizer（GCC/Clang 上还含 UBSan）
cmake --preset debug -DENABLE_THREAD_SANITIZER=ON    # ThreadSanitizer，仅 GCC/Clang
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/debug   # TSan 默认只打印报告、退出码 0，必须加
```

覆盖率门只在 GCC/Clang 上有数据（gcov 是 GNU 系的设施，MSVC 侧没有对应插桩），因此它算 CI 的判据而不是
本地的第三条硬判据：

```bash
cmake --preset debug -DENABLE_COVERAGE=ON && cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
GCOVR=/path/to/gcovr bash tools/coverage.sh build/debug   # 阈值写死在脚本里，改动会进 diff
```

静态分析门读的是构建生成的 `compile_commands.json`，所以先配置再跑；判据是零告警，工具版本钉死 22.1.7：

```bash
cmake --preset release && cmake --build build/release
CLANG_TIDY=/path/to/clang-tidy bash tools/tidy-check.sh build/release
```

要报出真缺陷就改实现，不要关检查：`.clang-tidy` 里明确不开的四项每条都写了理由与实测命中数，新增排除项
必须同样写明为什么。就地豁免用 `NOLINT`/`NOLINTBEGIN`-`NOLINTEND`，并把理由写在紧邻的注释里——
`NOLINTNEXTLINE` 只作用于紧邻的下一行，解释文字要写在它**上面**，否则豁免落在注释行上而检查照样报。

脚本在三种情况下会当场变红而不是给出一份好看的报告：构建树没开插桩开关、树里没有 `.gcda`（说明用例还没跑）、
以及 gcovr 版本低于 8——最后这条是因为 7.x 与 8.x 的开关取值形式不同，混用会把判据静默换成另一种含义。

README 里「解析 + 求值」那张表的分配次数也有判据（只钉整数，不判耗时——耗时随负载变化，判它等于判一个
不可复现的断言）：

```bash
cmake --preset release -DEXPRESSIONENGINE_BUILD_BENCHMARKS=ON
cmake --build build/release --target ParserBenchmark
./build/release/benchmarks/ParserBenchmark --check-allocations
```

改动了单次调用的分配次数就要重新实测并把钉值改到 `benchmarks/ParserBenchmark.cpp` 里那张表上，同时改 README；
钉值与语料表在同一个文件里，只改一边会当场报「钉值找不到对应语料」。这张表钉的是 MSVC 的读数，别的编译器上
这一判据直接拒绝给出结论，不会假装通过。

## 代码规范

- C++23。命名：类与文件名大驼峰，函数与变量小驼峰，成员变量 `m_` 前缀。
- 注释一律中文 Doxygen，**写在声明处**（函数、结构体、类、枚举、别名、常量都要有；覆写函数在派生类里
  写完整注释，不靠继承基类文档）。`.cpp` 不写文件头注释块。
- 测试目录逐层镜像 `src`：新增源文件要登记进所在子目录的 `CMakeLists.txt`（各层往
  `GLOBAL PROPERTY` 里 append，顶层汇总成唯一库目标），测试同理。
- 公开 API 的形状先对齐业界惯例再自创：注释里写的每条承诺（单位口径、生命周期、线程边界）都算契约，
  写不出来的不要写。

## 测试怎么算数

一条修复用例必须**能被证伪**：把实现临时改回缺陷行为，用例要变红；再还原，重跑确认绿。共享工作树里做这种
临时改动要用精确逆操作还原（不要用 `git checkout` 清未提交内容），改完单独抬 mtime 再重编，避免测到旧二进制。

时序类断言不许依赖调度：不写「等 N 毫秒应该怎样」，改成构造出条件再判（等到可观测的完成点、比值配余量、
能力探针按用例真正要建的对象构造）。带单位量、量纲、UTF-8 计数这类"看起来显然"的取值，断言要能算得出绝对值。

## 提交信息

约定式提交（Conventional Commits）：`<类型>[ 范围]: <中文标题>`，类型前缀用小写英文
（`feat` `fix` `refactor` `perf` `test` `docs` `build` `ci` `chore` `revert`）。一笔只做一件事；
正文里写清「为什么」与**实测到的证据**（跑过什么、数字从真实输出里重数，不要照抄记忆）。破坏性变更
用 `!` 与 `BREAKING CHANGE:` 脚注。

## 先讨论再动手的改动

公开头文件里的签名与语义、新增第三方依赖、许可与分发方式、`SECURITY.md` 与安全披露流程。
这类改动请在 issue 里说清楚意图与替代方案，再提 PR。
