#!/usr/bin/env bash
# 导出头自洽性门。两种形态：
#
#   bash tools/header-selfcheck.sh                      源码树里的每个公开头单独编一个翻译单元
#   bash tools/header-selfcheck.sh --prefix <安装前缀>   装出来的头按宿主看到的模样再编一遍，并核「该装的都装到了」
#
# 为什么要这道门：库内的整树构建看不见「这条包含其实是别的编译单元先带进来的」，而宿主是从单个头开始的。
# 本库出过一次 std::unique_lock 没包含 <mutex>——MSVC 的传递包含把它掩盖了，换 GCC 直接编不过。
# 第二种形态管的是另一半：安装规则漏装一个头，只有按包装出来的树去 include 才看得见。
#
# 编译器由 CXX 指定（默认 c++）；告警集与非 MSVC 构建一致，所以 GCC 与 Clang 各跑一遍有意义。

set -uo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root" || exit 1

CXX="${CXX:-c++}"
mode="source"
prefix="${2:-}"
if [[ "${1:-}" == "--prefix" ]]; then
    mode="installed"
    [[ -n "$prefix" ]] || { echo "头自洽门：--prefix 后面要跟安装前缀" >&2; exit 1; }
fi

if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "头自洽门：找不到编译器 $CXX（设 CXX 指向可执行文件）" >&2
    exit 1
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/ee-headers.XXXXXX")"
log="$work/log"
: >"$log"

count=0
fails=0

check_one()
{
    # $1 = 传给编译器的 include 名，$2 = 报错时显示的路径，$3 = 搜索路径参数
    local include_name=$1 shown=$2 incflag=$3
    printf '#include <%s>\n\nint main()\n{\n    return 0;\n}\n' "$include_name" >"$work/tu.cpp"
    count=$((count + 1))
    if ! "$CXX" -std=c++23 -Wall -Wextra -Wpedantic -Werror "$incflag" -fsyntax-only "$work/tu.cpp" >>"$log" 2>&1; then
        fails=$((fails + 1))
        printf '=== 不自洽：%s\n' "$shown" >>"$log"
    fi
}

tracked="$(git ls-files 'src/*.h')"
if [[ -z "$tracked" ]]; then
    # 清单为空会被读成「零个头、零个失败 = 通过」，那是假绿。实测踩过：在没有 .git 的解包目录里
    # 跑，git ls-files 只往 stderr 打一行 fatal，脚本差点报「0 个头全部通过」。
    echo "头自洽门：没有取到公开头清单（不在仓库根运行？还是这份树没有 .git？）" >&2
    rm -rf "$work"
    exit 1
fi

if [[ "$mode" == "source" ]]; then
    for header in $tracked; do
        check_one "${header#src/}" "$header" "-Isrc"
    done
else
    if [[ ! -d "$prefix/include/ExpressionEngine" ]]; then
        echo "头自洽门：$prefix/include/ExpressionEngine 不存在，先 cmake --install" >&2
        rm -rf "$work"
        exit 1
    fi

    # 装出来的头逐个编一遍
    installed_count=0
    for header in $(cd "$prefix/include" && find ExpressionEngine -name '*.h' | LC_ALL=C sort); do
        installed_count=$((installed_count + 1))
        check_one "$header" "$prefix/include/$header" "-I$prefix/include"
    done

    # 反向核对：受版本控制的每个公开头都要真的在包里（安装规则的 PATTERN 或目录写错时会漏装，
    # 而消费者只 include 两三个头，看不见漏掉的那一个）
    tracked_count=0
    missing=0
    for header in $tracked; do
        rel="${header#src/}"
        tracked_count=$((tracked_count + 1))
        if [[ ! -f "$prefix/include/$rel" ]]; then
            missing=$((missing + 1))
            echo "漏装：$rel" >>"$log"
        fi
    done

    if [[ "$missing" != "0" ]]; then
        echo "头自洽门：$tracked_count 个公开头里有 $missing 个没装进包里（日志 $log）" >&2
        grep -a "^漏装：" "$log" | head -20 >&2
        exit 1
    fi

    if [[ "$installed_count" != "$tracked_count" ]]; then
        # 装多了同样要说：多出来的通常是该被 .gitignore 或安装规则排除的内部头
        echo "头自洽门：包里有 $installed_count 个头，受版本控制的公开头是 $tracked_count 个，两者对不上（日志 $log）" >&2
        exit 1
    fi
fi

if [[ "$fails" != "0" ]]; then
    echo "头自洽门：$fails / $count 个头不能独立编译（缺包含或依赖别处先包含），日志 $log" >&2
    grep -a -E "^=== |error:|note:" "$log" | head -30 >&2
    rm -rf "$work"
    exit 1
fi

rm -rf "$work"
if [[ "$mode" == "source" ]]; then
    echo "头自洽门：$count 个公开头都能单独作为一个翻译单元编译（$("$CXX" --version | head -1)）"
else
    echo "头自洽门：包里 $count 个头都能独立编译，且与受版本控制的 $tracked_count 个公开头一一对应"
fi
