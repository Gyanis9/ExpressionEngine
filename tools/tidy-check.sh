#!/usr/bin/env bash
# 静态分析门本地版：与 CI 的 tidy 作业同一条判据、同一个工具版本（clang-tidy 22.1.7）。
#
# 用法（在仓库根）：
#   cmake --preset release            # 生成 compile_commands.json（任一档都行，判据读的就是它）
#   bash tools/tidy-check.sh build/release
# 本机 clang-tidy 不在 PATH 上时给 CLANG_TIDY 指路径，例如
#   CLANG_TIDY=/g/Tools/Qt/Tools/llvm-mingw2217_64/bin/clang-tidy.exe bash tools/tidy-check.sh build/release
#
# 判据是「零告警」：clang-tidy 报一条就算红。检查集与豁免理由都写在仓库根的 .clang-tidy 里，
# 想让门变松要改那份配置——配置里每一项排除都得带理由，改动会进 diff、会被评审看见。

set -uo pipefail

BUILD_DIR="${1:-build/release}"
CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"
PINNED_VERSION="${PINNED_VERSION:-22.1.7}"

if ! command -v "$CLANG_TIDY" >/dev/null 2>&1; then
    echo "静态分析门：找不到 clang-tidy（设 CLANG_TIDY 指向可执行文件；CI 用 venv 里装的那件）" >&2
    exit 1
fi

version="$("$CLANG_TIDY" --version 2>&1 | sed -n 's/.*LLVM version \([0-9.]*\).*/\1/p')"
if [[ "$version" != "$PINNED_VERSION" ]]; then
    # 检查集与规则实现随 LLVM 版本变：同一个文件在两版之间会报出不同的东西，
    # 不钉版本这道门会在某天全体变红，或者更糟——在某天悄悄不再报某个缺陷。
    echo "静态分析门：需要 clang-tidy ${PINNED_VERSION}，当前是 ${version:-未知}" >&2
    exit 1
fi

if [[ ! -f "${BUILD_DIR}/compile_commands.json" ]]; then
    # clang-tidy 要靠编译数据库才知道 include 路径与 -std；没有它就只能猜，猜出来的结论不算判据
    echo "静态分析门：${BUILD_DIR}/compile_commands.json 不存在，先配置并构建该目录" >&2
    exit 1
fi

list="$(mktemp)"
git ls-files 'src/*.cpp' >"$list"
count="$(wc -l <"$list" | tr -d ' ')"
if [[ "$count" == "0" ]]; then
    # 空清单会被读成「零告警」，那是假绿
    echo "静态分析门：没有取到源文件清单（不在仓库根运行？）" >&2
    rm -f "$list"
    exit 1
fi

log="$(mktemp)"
# shellcheck disable=SC2046
"$CLANG_TIDY" -p "$BUILD_DIR" --config-file=.clang-tidy --quiet $(cat "$list") >"$log" 2>&1 || true

# 诊断写进日志后按行计数：clang-tidy 在有告警时也会以 0 退出（--warnings-as-errors 在管道里
# 的退出码还会被 shell 吞掉），所以判据一律数行数
violations="$(grep -ac ' warning: ' "$log" || true)"

if [[ "$violations" != "0" ]]; then
    echo "静态分析门：${violations} 条告警（分析 ${count} 个翻译单元，配置 .clang-tidy，构建目录 ${BUILD_DIR}）" >&2
    grep -a ' warning: ' "$log" | sed -n '1,25p' >&2
    echo "…完整日志：$log" >&2
    rm -f "$list"
    exit 1
fi

echo "静态分析门：零告警（${count} 个翻译单元，clang-tidy ${version}）"
rm -f "$list" "$log"
