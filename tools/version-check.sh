#!/usr/bin/env bash
# 版本号一致性门：同一份版本号写在四处地方——CMakeLists.txt 的 project(VERSION)、conanfile.py 的
# version、每个头文件文件头的 @version，以及（打过标签时）指向 HEAD 的 v<版本> 标签。
# 上一轮升版是手工改 35 个文件头改齐的，没有任何东西保证下一次不漏；漏掉的表现为
# 「包配置说 0.1.0、头文件说 0.0.1」，构建与用例都不会因此变红。
#
# 判据（全部取自 git ls-files，不看工作树里未跟踪的残留文件）：
#   1) 两处配置源都读得出版本号，且相等；
#   2) 每个受版本控制的 .h 恰好带一条 @version，取值等于 1)；
#   3) 带 @version 的 .cpp（规范不要求 .cpp 写文件头，写了就得对）取值同样相等；
#   4) 指向 HEAD 的每个 v* 标签去掉前缀后相等。
# 空清单判红而不是判绿：「0 个头、0 处不符」和「全部一致」在退出码上必须能区分。
#
# 用法：
#   bash tools/version-check.sh          跑判定，打印口径
#   bash tools/version-check.sh --print  只打印规范版本号（tools/make-dist.sh 用它取包名版本）

set -uo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root" || exit 1

die()
{
    echo "版本一致性门：$*" >&2
    exit 1
}

# 从一份文件里取 @version 的取值，可能有多条（多条本身就是问题，交给调用方按计数判）
versions_in()
{
    grep -a -o '@version[[:space:]]*[0-9][0-9A-Za-z.+-]*' "$1" 2>/dev/null | sed 's/@version[[:space:]]*//'
}

cmake_version="$(sed -n 's/^project(ExpressionEngine VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
conan_version="$(sed -n 's/^[[:space:]]*version = "\([0-9.]*\)".*/\1/p' conanfile.py)"
[[ -n "$cmake_version" ]] || die "读不到 CMakeLists.txt 里的 project(... VERSION)"
[[ -n "$conan_version" ]] || die "读不到 conanfile.py 里的 version"
[[ "$cmake_version" == "$conan_version" ]] || die "两处版本号不一致：CMakeLists 是 $cmake_version，conanfile.py 是 $conan_version"

if [[ "${1:-}" == "--print" ]]; then
    printf '%s\n' "$cmake_version"
    exit 0
fi

for tag in $(git tag --points-at HEAD); do
    [[ "${tag#v}" == "$cmake_version" ]] || die "标签 $tag 与版本号 $cmake_version 不符"
done

headers="$(git ls-files '*.h')"
[[ -n "$headers" ]] || die "没有取到任何 .h（不在仓库根运行？还是这份树没有 .git？）"

checked=0
offenders=0
for header in $headers; do
    checked=$((checked + 1))
    found="$(versions_in "$header")"
    lines=$(printf '%s' "$found" | grep -a -c . || true)
    if [[ "$lines" != "1" ]]; then
        echo "  $header：@version 出现 $lines 次（要恰好 1 次）" >&2
        offenders=$((offenders + 1))
        continue
    fi
    if [[ "$found" != "$cmake_version" ]]; then
        echo "  $header：写着 $found，应为 $cmake_version" >&2
        offenders=$((offenders + 1))
    fi
done

cpp_checked=0
for source in $(git ls-files '*.cpp'); do
    found="$(versions_in "$source")"
    [[ -n "$found" ]] || continue
    cpp_checked=$((cpp_checked + 1))
    if [[ "$(printf '%s\n' "$found" | sort -u | wc -l)" != "1" ]] || [[ "$(printf '%s\n' "$found" | sort -u)" != "$cmake_version" ]]; then
        echo "  $source：@version 取值为 $(printf '%s\n' "$found" | tr '\n' ' ')，应为 $cmake_version" >&2
        offenders=$((offenders + 1))
    fi
done

if [[ "$offenders" != "0" ]]; then
    die "$checked 个头文件 + $cpp_checked 个带文件头的源文件里有 $offenders 处与 $cmake_version 不符"
fi

echo "版本一致性门：$cmake_version —— 两处配置源、$checked 个头文件、$cpp_checked 个带 @version 的源文件全部对齐"
