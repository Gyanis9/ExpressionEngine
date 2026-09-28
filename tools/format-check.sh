#!/usr/bin/env bash
# 样式门本地版：与 CI 的 style 作业同一条判据、同一个工具版本（clang-format 23.1.1）。
# 用法：bash tools/format-check.sh；本机 clang-format 不在 PATH 上时给 CLANG_FORMAT 指路径，例如
#   CLANG_FORMAT="/g/Tools/LLVM/bin/clang-format.exe" bash tools/format-check.sh
# 自动修复用：clang-format -i $(git ls-files '*.h' '*.hpp' '*.cpp')

set -uo pipefail

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
PINNED_VERSION="${PINNED_VERSION:-23.1.1}"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "样式门：找不到 clang-format（设 CLANG_FORMAT 指向可执行文件）" >&2
    exit 1
fi

version="$("$CLANG_FORMAT" --version 2>&1 | sed -n 's/^clang-format version \([0-9.]*\).*/\1/p')"
if [[ "$version" != "$PINNED_VERSION" ]]; then
    # 版本不同就可能判出相反的结论：同一个文件在两版之间会被排成两种形状，门禁必须钉版本
    echo "样式门：需要 clang-format ${PINNED_VERSION}，当前是 ${version:-未知}" >&2
    exit 1
fi

list="$(mktemp)"
git ls-files '*.h' '*.hpp' '*.cpp' >"$list"
count="$(wc -l <"$list" | tr -d ' ')"
if [[ "$count" == "0" ]]; then
    # 清单为空时门禁会「零违规」通过，那是假绿，必须当场拒绝
    echo "样式门：没有取到源文件清单（不在仓库根运行？）" >&2
    rm -f "$list"
    exit 1
fi

log="$(mktemp)"
# clang-format 的诊断写 stderr，且 --Werror 下的退出码不可靠（管道里还会被吞），因此一律数行数
# shellcheck disable=SC2086
"$CLANG_FORMAT" --dry-run --Werror $(cat "$list") >"$log" 2>&1 || true
violations="$(grep -ac 'should be clang-formatted' "$log" || true)"

if [[ "$violations" != "0" ]]; then
    echo "样式门：${violations} 处不符合 .clang-format，先自动修复再提交：" >&2
    echo "  ${CLANG_FORMAT} -i \$(git ls-files '*.h' '*.hpp' '*.cpp')" >&2
    sed -n '1,20p' "$log" >&2
    rm -f "$list" "$log"
    exit 1
fi

echo "样式门：全绿（${count} 个文件，clang-format ${version}）"
rm -f "$list" "$log"
