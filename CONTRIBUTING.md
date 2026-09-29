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
版本抬到 `0.9.0`，再要求同一个消费者按 `0.0.2` 配不上——`tools/package-check/compat-refusal-check.sh` 与
Windows CI 调的是同一条脚本。

有 sanitizer 与线程检测两种插桩构建，互斥（同时开会被配置期拒绝）：

```bash
cmake --preset debug -DENABLE_SANITIZERS=ON          # AddressSanitizer（GCC/Clang 上还含 UBSan）
cmake --preset debug -DENABLE_THREAD_SANITIZER=ON    # ThreadSanitizer，仅 GCC/Clang
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/debug   # TSan 默认只打印报告、退出码 0，必须加
```

插桩构建只用于跑用例，**不要拿它当交付形态验包**：ASan/UBSan 插桩过的静态库里留着一堆对
`__asan_report_*` / `__ubsan_handle_*_abort` 的外部引用，要靠最终可执行体链接时把检测运行时带进来，
而仓库外的消费者按默认标志链接拿不到它们（Linux CI 首跑实测：`undefined reference to '__asan_report_store1'`）。
MSVC 侧因为 ASan 是 DLL 运行期才看起来"没问题"，那也不是用户拿到的那一版。装包与包消费者因此跑在一份
不带插桩的 Release 树上，本地同形：

```bash
cmake -S . -B build/pkg -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF -DEXPRESSIONENGINE_BUILD_TESTS=OFF
cmake --build build/pkg && cmake --install build/pkg --prefix /tmp/ee-pfx
cmake -S tools/package-check -B /tmp/ee-consumer -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/tmp/ee-pfx
cmake --build /tmp/ee-consumer && /tmp/ee-consumer/consumerCheck
```

Linux 上的 Clang 取 **20 或以上**：libstdc++ 13/14 把 `<expected>` 整段挡在 `__cpp_concepts >= 202002L`
之后，Clang ≤ 18 只报 `201907L`，于是 `std::expected` 直接不可见；换 libc++ 又撞上 libc++ 18 的浮点
`from_chars` 是删除的。两条都是上游的事，绕法见 README「已验证的编译器」。

覆盖率门只在 GCC/Clang 上有数据（gcov 是 GNU 系的设施，MSVC 侧没有对应插桩），因此它算 CI 的判据而不是
本地的第三条硬判据：

```bash
cmake --preset debug -DENABLE_COVERAGE=ON && cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
GCOVR=/path/to/gcovr bash tools/coverage.sh build/debug   # 阈值写死在脚本里，改动会进 diff
```

模糊测试门只在 Clang 上有目标（`-fsanitize=fuzzer` 属 compiler-rt，GCC 没有对应实现），本地有 clang++
就跑得动，也不必装 Conan——模糊目标只链库本体：

```bash
bash tools/fuzz.sh                    # 每个目标 60 秒；本地长跑给 FUZZ_SECONDS=600
CXX=clang++-20 bash tools/fuzz.sh     # 默认 clang++，取别的档用 CXX/CC 指
```

它自己配一份 `build/fuzz`（Release + ASan/UBSan，**库本体也带 `fuzzer-no-link` 插桩**——只插桩 `fuzz/`
那一个翻译单元时，覆盖率引导看不见库里的分支，实测 45 秒停在 48 条边缘，那种「没崩溃」不是证据）。
脚本读 cov 边缘数与 exec/s 两个数防空转，崩溃输入留在临时语料目录并打印复现命令；红了不清理现场。
新增或改动 `fuzz/` 下任何东西之后，重跑一次突变自证：把 `tryParse` 的实现换成会抛的那条 `parse` 通道，
脚本必须在几十秒内变红——不变红的判据等于没有判据。

静态分析门读的是构建生成的 `compile_commands.json`，所以先配置再跑；判据是零告警，工具版本钉死 22.1.7。
它属于 CI 的判据（本机装了同一版本的 clang-tidy 就能跑同样的命令，没装不拦本地提交）：


```bash
cmake --preset release && cmake --build build/release
CLANG_TIDY=/path/to/clang-tidy bash tools/tidy-check.sh build/release
```

要报出真缺陷就改实现，不要关检查：`.clang-tidy` 里明确不开的四项每条都写了理由与实测命中数，新增排除项
必须同样写明为什么。就地豁免用 `NOLINT`/`NOLINTBEGIN`-`NOLINTEND`，并把理由写在紧邻的注释里——
`NOLINTNEXTLINE` 只作用于紧邻的下一行，解释文字要写在它**上面**，否则豁免落在注释行上而检查照样报。

新增或改动公开头时，本地跑一下导出头自洽性判据（Linux 矩阵的两个编译器各跑一遍，MSVC 用户的机器上
有 g++/clang++ 就能跑）：

```bash
CXX=g++ bash tools/header-selfcheck.sh
```

它把每个公开头单独编一个翻译单元，专抓「这条包含其实是被别的编译单元带进来的」——本库真出过一次
`std::unique_lock` 没带 `<mutex>`，MSVC 下看不出来，换 GCC 直接编不过。清单取不到时脚本拒绝判定，
不会把「零个头」报成全绿。改过安装规则（`install(DIRECTORY src/ExpressionEngine ...)`）之后再加一步：

```bash
cmake --install build/release --prefix /tmp/ee-pfx
CXX=g++ bash tools/header-selfcheck.sh --prefix /tmp/ee-pfx   # 装出来的头逐个编，并核该装的都装到
```

升版时四处版本号由同一条脚本判（`CMakeLists.txt` 的 `project(VERSION)`、`conanfile.py` 的 `version`、
每个头文件文件头的 `@version`、指向 HEAD 的 `v*` 标签）：

```bash
bash tools/version-check.sh          # 判定，不符项逐条点名
bash tools/version-check.sh --print  # 只吐规范版本号（tools/make-dist.sh 用它取包名）
```

新增头文件忘了写 `@version` 同样会红（报「出现 0 次」）；清单取自 `git ls-files`，不受工作树里
未跟踪的残留文件影响，取不到清单时脚本拒绝判定而不是报成全绿。

发布前（或改动了版本号之后）跑一次分发包验证：

```bash
bash tools/make-dist.sh --verify     # 产包 -> 核版本一致与清单逐条相同 -> 解包 -> 构建 -> 全量用例 -> 装包 -> 消费者
```

它在工作树不干净时直接拒绝产出——脏树打出来的包与同一提交的包内容不同，校验和就没有意义。
版本号同时写在 `CMakeLists.txt` 的 `project(... VERSION)` 与 `conanfile.py` 的 `version` 两处，
脚本会核它们相等（HEAD 上带标签时还要标签去掉 `v` 前缀后一致）；只想快速验「包自洽且装得出可用产品」
就用 `--check-packaging`（不开用例）。

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
