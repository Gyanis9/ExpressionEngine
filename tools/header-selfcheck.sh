#!/usr/bin/env bash
# 导出头自洽性门：每个公开头单独编一个翻译单元（只 include 它自己），判据是零告警。
#
# 为什么值得单独立门：本库出过一次「用了 std::unique_lock 却没包含 <mutex>」——MSVC 的传递包含
# 把缺的那条掩盖掉了，换 GCC 直接编不过。宿主也是从单个头开始用的：头自不自洽是接口的一部分，
# 而库内的整树构建看不见这件事（别的 TU 先把包含凑齐了）。
#
# 用法（在仓库根）：
#   bash tools/header-selfcheck.sh              # 用 CXX，默认 c++
#   CXX=g++ bash tools/header-selfcheck.sh
# 告警集与非 MSVC 构建一致（-Wall -Wextra -Wpedantic -Werror），所以 GCC 与 Clang 各跑一遍有意义。

set -uo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root" || exit 1

CXX="${CXX:-c++}"
if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "头自洽门：找不到编译器 $CXX（设 CXX 指向可执行文件）" >&2
    exit 1
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/ee-headers.XXXXXX")"
log="$work/log"
: >"$log"

count=0
fails=0
headers="$(git ls-files 'src/*.h')"
if [[ -z "$headers" ]]; then
    # 清单为空会被读成「零个头、零个失败 = 通过」，那是假绿：拿不到清单就是判据没跑起来。
    # 实测踩过：在没有 .git 的解包目录里跑，git ls-files 只输出一行 fatal，脚本差点报全绿。
    echo "头自洽门：没有取到公开头清单（不在仓库根运行？还是这份树没有 .git？）" >&2
    rm -rf "$work"
    exit 1
fi

for header in $headers; do
    rel="${header#src/}"
    printf '#include <%s>\n\nint main()\n{\n    return 0;\n}\n' "$rel" >"$work/tu.cpp"
    count=$((count + 1))
    if ! "$CXX" -std=c++23 -Wall -Wextra -Wpedantic -Werror -Isrc -fsyntax-only "$work/tu.cpp" >>"$log" 2>&1; then
        fails=$((fails + 1))
        printf '=== 不自洽：%s\n' "$header" >>"$log"
    fi
done

if [[ "$fails" != "0" ]]; then
    echo "头自洽门：$fails / $count 个头不能独立编译（缺包含或依赖别处先包含），日志 $log" >&2
    grep -a -E "^=== |error:|note:" "$log" | head -30 >&2
    exit 1
fi

rm -rf "$work"
echo "头自洽门：$count 个公开头都能单独作为一个翻译单元编译（$("$CXX" --version | head -1)）"
