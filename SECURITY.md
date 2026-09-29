# 安全策略

本文件说明 ExpressionEngine 的安全问题怎么报告、哪些版本在维护范围内，以及这个库自己声明的
边界。它是公式解析与求值库，不是网络服务：威胁面是**宿主把不可信的表达式文本交给本库解析或求值**。

## 受支持的版本

| 版本 | 是否受安全修复 | 说明 |
| --- | --- | --- |
| 最新发布版（见 GitHub Releases 与 `CHANGELOG.md`） | 是 | 安全问题只在最新发布版上修 |
| 更早的 0.x 版本 | 否 | 0.x 期间接口仍在收敛，不做 backport |

本项目按开源惯例维护，**没有商业支持协议，也没有 SLA 承诺**。下面的响应时间是尽力值，
不是义务；需要合同级保障时请自行评估。

## 报告漏洞

优先用 GitHub 仓库的私密漏洞报告通道：仓库首页 → **Security** → **Report a vulnerability**，
在那里写清版本、编译器与构建档、触发输入、以及观察到的行为。

不要在公开 issue 或讨论区里贴触发输入的细节——在修复发布之前，那就是可执行的攻击样例。
如果该通道对本仓库不可用，开一个只写「需要私报安全问题」的 issue，等维护者私信补细节。

尽力值：确认收到 5 个工作日内；是否需要公告与修复版本随严重程度决定，结论会写进
`CHANGELOG.md` 的「安全」相关条目与该版本的 Release 说明。

## 本库声明的边界（哪些行为是设计，不是漏洞）

不可信文本这一面的既有判据，都可回代码核对：

- 非异常通道：`ExpressionParser::tryParse`、`Expression::tryEvaluate`、`QuantityParser::tryParse`
  以 `std::expected` 返回失败，**承诺不抛异常**；异常通道是 `parse` / `evaluate`。
- 递归深度：`Expression::maxAstDepth = 64`，超限按错误拒绝（解析与化简两条路都判）。
- 区间规模：`FunctionExpression::maxRangeCells = 65536`，超出拒绝，避免一条文本放大成无界遍历。
- 取值：求值出口不把 NaN 或无穷大交给宿主（`Base::ValueError`），除零早已报错。
- 单位与文本写法：持久文本（`toString(true)`）必须能解析回同一棵树——这条有模糊测试门与
  两万条随机不可信文本用例在判（见 `fuzz/` 与 `tools/fuzz.sh`）。
- 内存安全：CI 在 GCC 与 Clang 两档带 AddressSanitizer + UndefinedBehaviorSanitizer
  跑全量用例，另有 ThreadSanitizer 作业与 libFuzzer 作业（每目标 120 秒）。

## 不算漏洞的情况

- **输入长度没有库侧上限**：多长的表达式文本算「过大」由宿主决定并负责（README 里写明这一条
  留给宿主）。本库只保证深度与区间规模两条上限，不保证「任意长文本的耗时上界」。
- 宿主自己实现的 `IObjectResolver` / `IProperty` 返回的数据质量或其内部的注入问题。
- 关闭 `EXPRESSIONENGINE_WARNINGS_AS_ERRORS`、不用带 sanitizer 的构建、或自行改写
  `.clang-tidy` / 门禁脚本之后出现的问题。
- 交付包之外的组件：GoogleTest 只用于测试可执行体，不进入 `cmake --install` 的库与头文件
  （见 `NOTICE`）。

## 已知的开放边界

只有一条，且不是本库的文本层：**宿主自己能不能存放名字段里的 NUL 与控制字节**。
本库的口径已经定成公开契约（见 `VariableReference` 的类注释）——引用路径的三格名字是任意字节序列，
持久文本靠转义规则不丢信息，256 个字节取值有逐条用例钉住（`ReferenceNameSlotsRoundTripEveryByte`）。
所以「表达式文本这一层」不会因为名字里有怪字节而丢信息；会出问题的是宿主自己的存储与传输栈
（把属性名塞进 CSV、URL、数据库字段、日志时按各自规则截断或转义）。这一面归宿主负责。
