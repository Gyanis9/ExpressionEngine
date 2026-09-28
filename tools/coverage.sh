#!/usr/bin/env bash
# 覆盖率门本地版：与 CI 的 coverage 作业同一条判据、同一组阈值。
# 完整跑法（在仓库根）：
#   cmake -S . -B build/coverage -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
#   cmake --build build/coverage
#   ctest --test-dir build/coverage --output-on-failure
#   bash tools/coverage.sh build/coverage
# 本机 gcovr 不在 PATH 上时给 GCOVR 指路径，例如
#   GCOVR=/root/cov-venv/bin/gcovr bash tools/coverage.sh build/coverage

set -uo pipefail

BUILD_DIR="${1:-build/coverage}"
GCOVR="${GCOVR:-gcovr}"

# 阈值取自 2026-09-29 的实测：g++ 13.3 + 322 条用例，库本体（src/ExpressionEngine）
# 行 83.2%（5746/6907）、函数 81.1%（821/1012）、分支 58.1%（4485/7719）。
# 线画在实测之下几个点：新增代码允许短暂低于历史值，但整体退化到一定程度必须当场变红。
# 阈值写死在这里而不是环境变量：想让门变松要改这个文件，改动会进 diff、会被评审看见。
FAIL_UNDER_LINE=80
FAIL_UNDER_FUNCTION=78
FAIL_UNDER_BRANCH=55

if ! command -v "$GCOVR" >/dev/null 2>&1; then
    echo "覆盖率门：找不到 gcovr（设 GCOVR 指向可执行文件；CI 用 venv 里装的那件）" >&2
    exit 1
fi

# gcovr 7.x 与 8.x 的开关取值形式不同（--gcov-ignore-parse-errors 带参数值是 8.x 起的），
# 版本不符时判据会静默改变含义，所以与样式门一样先钉住。
gcovr_major="$("$GCOVR" --version 2>&1 | sed -n 's/^gcovr \([0-9]\+\).*/\1/p')"
if [[ -z "$gcovr_major" || "$gcovr_major" -lt 8 ]]; then
    echo "覆盖率门：需要 gcovr 8 及以上，当前是 $("$GCOVR" --version 2>&1 | head -1)" >&2
    exit 1
fi

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    echo "覆盖率门：${BUILD_DIR} 里没有 CMakeCache.txt，先按上面的用法配置构建树" >&2
    exit 1
fi

if ! grep -q '^ENABLE_COVERAGE:BOOL=ON$' "${BUILD_DIR}/CMakeCache.txt"; then
    # 没插桩的树上一条计数都不会有，报告会是「0 行覆盖」而不是报错，那是最典型的假绿形状
    echo "覆盖率门：${BUILD_DIR} 不是 -DENABLE_COVERAGE=ON 配出来的，gcov 插桩没开" >&2
    exit 1
fi

gcda_count="$(find "$BUILD_DIR" -name '*.gcda' | wc -l | tr -d ' ')"
if [[ "$gcda_count" == "0" ]]; then
    echo "覆盖率门：${BUILD_DIR} 下没有 .gcda 计数文件，先跑 cmake --build 与 ctest" >&2
    exit 1
fi

report="${BUILD_DIR}/coverage.txt"
# gcov 在没跑到的分支上会写出负数计数（gcc bug 68080），gcovr 默认把这种情况当解析错误整份中止；
# 这里按官方建议降级为「每文件警告一次」，其余真实缺陷仍会报出来。
# 过滤式两种写法都吃过亏：gcovr 报的路径在根之内是相对形式（src/ExpressionEngine/...），
# 只匹配绝对路径的 '.*/src/...' 会筛空整份数据（筛空后是 0%，不是报错，正是最危险的假绿形状）。
# (^|.*/) 同时吃相对与绝对两种形态，构建树放在源码树内或旁边都得到同一份读数。
"$GCOVR" -r . "$BUILD_DIR" \
    --filter '(^|.*/)src/ExpressionEngine/.*' \
    --gcov-ignore-parse-errors negative_hits.warn_once_per_file \
    --fail-under-line "$FAIL_UNDER_LINE" \
    --fail-under-function "$FAIL_UNDER_FUNCTION" \
    --fail-under-branch "$FAIL_UNDER_BRANCH" \
    --print-summary --txt "$report"
rc=$?

if [[ "$rc" != "0" ]]; then
    echo "覆盖率门：低于阈值（行 ${FAIL_UNDER_LINE}% / 函数 ${FAIL_UNDER_FUNCTION}% / 分支 ${FAIL_UNDER_BRANCH}%），逐文件明细见 ${report}" >&2
    exit "$rc"
fi

echo "覆盖率门：达标（插桩对象 ${gcda_count} 个，逐文件明细见 ${report}）"
