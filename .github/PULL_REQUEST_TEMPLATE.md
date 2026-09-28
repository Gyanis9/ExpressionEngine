## 这一笔在做什么

<!-- 一句话说清动机：修的是哪个缺陷、加的是哪个能力。影响公开接口的请标明。 -->

## 判据（本地跑过再提，CI 只在 main 上跑）

- [ ] `cmake --preset debug && cmake --build build/debug && ctest --test-dir build/debug` 全绿零告警
- [ ] Release 档同样全绿
- [ ] `bash tools/package-check/run-gate.sh` 全绿（装包 + 仓库外消费者）
- [ ] `bash tools/format-check.sh` 全绿（clang-format 23.1.1）
- [ ] 涉并发的改动：`-DENABLE_THREAD_SANITIZER=ON` 一档跑过，且 `TSAN_OPTIONS=halt_on_error=1`

## 这条修复能被证伪吗

<!-- 新增/改动的用例，撤掉实现后必须变红。写清撤的是哪一处、红了哪几条；共享工作树里要精确还原。 -->

- 突变：
- 红集：

## 备注

- 数字（用例数、告警数、性能）请从真实输出里数，别照抄记忆。
- 改公开头文件的签名或语义、加依赖、动许可 —— 请先开 issue 讨论。
