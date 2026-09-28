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

有 sanitizer 与线程检测两种插桩构建，互斥（同时开会被配置期拒绝）：

```bash
cmake --preset debug -DENABLE_SANITIZERS=ON          # AddressSanitizer（GCC/Clang 上还含 UBSan）
cmake --preset debug -DENABLE_THREAD_SANITIZER=ON    # ThreadSanitizer，仅 GCC/Clang
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/debug   # TSan 默认只打印报告、退出码 0，必须加
```

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
