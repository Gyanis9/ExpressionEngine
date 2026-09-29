#!/usr/bin/env bash
# 模糊测试判据：把 fuzz/ 下的每个 libFuzzer 目标限时跑一遍，崩溃即红。
#
# 为什么这道门值得有：TestUntrustedInput.cpp 用两万条**自己造的**随机文本判同一组不变式，
# 那是「按零件拼装」的形状；libFuzzer 按覆盖率引导，会自己找到拼装规则没覆盖到的边角
# （半个数字后面接单位、括号与区间混排、函数名撞上关键字）。同一组判据，探索方式互补。
#
# 用法：
#   bash tools/fuzz.sh                      每个目标跑 ${FUZZ_SECONDS}（默认 60）秒
#   FUZZ_SECONDS=600 bash tools/fuzz.sh     本地长跑
#
# 编译器取 CXX（默认 clang++）、CC（默认 clang）；不是 Clang 就直接拒——`-fsanitize=fuzzer` 是
# compiler-rt 的设施，GCC 没有。构建目录独立（build/fuzz）：用例那一档要经 Conan 拉 gtest，
# 而模糊目标只链库本体。
#
# 两个防空转的读数写在脚本末尾的判定里：
#   cov:    覆盖率引导的「已覆盖边缘」数。目标若根本没走进解析器（入口写错、语料被拒收），
#           它会停在个位数——那时「没崩溃」不构成证据；
#   exec/s: 每秒执行次数，为 0 说明进程没真跑起来。
# 下限按本机实测取有余量的位置（见下面两个默认值）；换判据要重新量，不要照抄数字。
#
# 判据本身会不会红，用突变自证：把 tryParse 的实现改成会抛的那条通道（或放宽解析器的嵌套上限），
# 本脚本应当在几十秒内崩溃并把 repro 落进语料目录。跑完还原并复跑确认回到全绿。

set -u

# 仓库根取自脚本自身的位置，不用 `git rev-parse --show-toplevel`：源码包与容器里的解包树没有
# .git，那条命令只留下一行 fatal 和一个空字符串，而 `cd ""` 在 bash 里是成功的空操作——脚本会拿
# 当前目录继续跑，实测表现为 -dict 拼成 /fuzz/parse.dict、libFuzzer 退出码 1 且不给判据原因。
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[[ -d "$root/fuzz" ]] || { echo "模糊测试判据：$root 下没有 fuzz/ 目录（脚本被挪出仓库了？）" >&2; exit 1; }
cd "$root" || exit 1

CC="${CC:-clang}"
CXX="${CXX:-clang++}"
BUILD_DIR="${BUILD_DIR:-build/fuzz}"
# 相对路径一律钉在仓库根上：调用者从别处启动时，构建目录不该跟着当前目录跑
[[ "$BUILD_DIR" == /* ]] || BUILD_DIR="$root/$BUILD_DIR"
SECONDS_PER_TARGET="${FUZZ_SECONDS:-60}"
# 下限取自本机实测（clang 20.1.2，45 秒）：cov=2901 条边缘、exec/s=17257。cov 取约三分之一、
# exec/s 取不到百分之一：换台慢机器不至于变红，而「根本没走进解析器」必定变红——
# 库本体没插桩的那一轮实测只跑到 48 条边缘，这条线当场把它判红了。
MIN_COVERAGE_EDGES="${MIN_COVERAGE_EDGES:-1000}"
MIN_EXEC_PER_SEC="${MIN_EXEC_PER_SEC:-100}"

die()
{
    echo "模糊测试判据：$*" >&2
    exit 1
}

command -v "$CXX" >/dev/null 2>&1 || die "找不到编译器 $CXX（设 CXX 指向 clang++）"
command -v "$CC" >/dev/null 2>&1 || die "找不到编译器 $CC（设 CC 指向 clang）"
compiler_banner="$("$CXX" --version | head -1)"
printf '%s\n' "$compiler_banner" | grep -qi "clang" || die "$CXX 不是 Clang：$compiler_banner"
# 词典缺了会静默退化成「纯随机字节」：覆盖率照样能过下限，但函数名与关键字那一类形状基本不再出现，
# 这道门看着还是绿的、实际换成了另一道。缺文件就拒，不悄悄降级
[[ -s "$root/fuzz/parse.dict" ]] || die "缺 fuzz/parse.dict（或它是空文件）"
# 语料目录里存着历史崩溃件：缺了它这道门照绿、却不再记得曾经红过的那些形状，因此也算缺判据
[[ -n "$(ls -A "$root/fuzz/seed" 2>/dev/null)" ]] || die "缺 fuzz/seed/ 语料，或它是空目录"

work="$(mktemp -d "${TMPDIR:-/tmp}/ee-fuzz.XXXXXX")"
configure_log="$work/configure.log"
build_log="$work/build.log"
rc=0

# 只在脚本整体成功时清理工作目录；红的时候语料、repro 与日志就是现场，删了就等于把证据一起销毁。
# 退出码从这里取，不从 rc 取：配置或构建失败走的是 die()，那时 rc 还是 0。
finish()
{
    if [[ "$1" == "0" ]]; then
        rm -rf "$work"
    else
        echo "现场留在 $work" >&2
    fi
}
trap 'finish $?' EXIT

cmake -S "$root" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF \
    -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
    -DENABLE_SANITIZERS=ON \
    -DEXPRESSIONENGINE_BUILD_TESTS=OFF \
    -DEXPRESSIONENGINE_BUILD_FUZZ_TARGETS=ON >"$configure_log" 2>&1 \
    || { tail -25 "$configure_log" >&2; die "配置失败（日志 $configure_log）"; }

cmake --build "$BUILD_DIR" >"$build_log" 2>&1 \
    || { tail -25 "$build_log" >&2; die "构建失败（日志 $build_log）"; }

warnings=$(grep -a -c "warning:" "$build_log")
[[ "$warnings" == "0" ]] || die "模糊目标带来 $warnings 条告警：这道门也受零告警判据约束"

targets=()
while IFS= read -r line; do
    targets+=("$line")
done < <(find "$BUILD_DIR" -type f -name '*Fuzz*' -perm -u+x | LC_ALL=C sort)
[[ ${#targets[@]} -gt 0 ]] || die "在 $BUILD_DIR 下没找到可执行的模糊目标（构建没产出？「没有目标」不能报成全绿）"

for target in "${targets[@]}"; do
    name=$(basename "$target")
    corpus="$work/corpus-$name"
    mkdir -p "$corpus"
    # 种子复制进工作区再喂：libFuzzer 会往语料目录里写它自己发现的输入，不能写回仓库
    cp "$root"/fuzz/seed/* "$corpus/" || die "复制 fuzz/seed 进语料目录失败"

    log="$work/$name.log"
    # 崩溃件单独落一个空目录：与语料混在一起时分不出「哪个才是 repro」——实测过，
    # 打印出来的是种子副本而不是 libFuzzer 写出的那个文件。
    artifacts="$work/artifacts-$name"
    mkdir -p "$artifacts"
    echo "=== $name：${SECONDS_PER_TARGET} 秒 ==="
    run_args=(
        -max_total_time="$SECONDS_PER_TARGET"
        -max_len=4096
        -rss_limit_mb=2048
        -artifact_prefix="$artifacts/"
    )
    run_args+=(-dict="$root/fuzz/parse.dict")
    "$target" "${run_args[@]}" "$corpus" >"$log" 2>&1
    run_rc=$?

    # libFuzzer 的统计行形如「#1048576	EXEC cov: 1234 ft: 567 corp: ... exec/s: 20000 rss: ...」，
    # 冒号后面带空格——按 'cov:[0-9]+' 提会整批读成 0，判据就变成「永远不够下限」的假红。
    edges=$(grep -a -o 'cov:[[:space:]]*[0-9]\+' "$log" | tail -1 | grep -a -o '[0-9]\+$')
    rate=$(grep -a -o 'exec/s:[[:space:]]*[0-9]\+' "$log" | tail -1 | grep -a -o '[0-9]\+$')
    edges=${edges:-0}
    rate=${rate:-0}

    if [[ "$run_rc" != "0" ]]; then
        echo "FAIL $name 退出码 $run_rc" >&2
        # -A2 是必须的：violate() 把「原文=[..] 持久文本=[..]」写在第二行，只匹配首行就会
        # 报出一条没有输入可复现的判据——CI 上把这条 grep 贴进日志才发现丢的就是那一行
        reason="$(grep -a -E -A2 "^fuzz 判据失败|^ERROR: |^SUMMARY: " "$log" | head -16)"
        # 崩溃之外的失败（词典读不到、语料目录不存在、参数拼错）不带上面任何前缀；
        # 没有兜底就会出现「这道门红了但一行原因都没有」——上一轮在解包树里就是这么空的
        [[ -n "$reason" ]] || reason="$(tail -6 "$log")"
        printf '%s\n' "$reason" >&2
        repro=$(find "$artifacts" -maxdepth 1 -type f | LC_ALL=C sort | head -1)
        [[ -n "$repro" ]] && echo "复现：$target \"$repro\"" >&2
        rc=1
        continue
    fi

    if [[ "$edges" -lt "$MIN_COVERAGE_EDGES" ]]; then
        echo "FAIL $name 只覆盖到 $edges 条边缘（下限 $MIN_COVERAGE_EDGES）：目标可能根本没走进解析器" >&2
        tail -5 "$log" >&2
        rc=1
        continue
    fi

    if [[ "$rate" -lt "$MIN_EXEC_PER_SEC" ]]; then
        echo "FAIL $name 每秒执行 $rate 次（下限 $MIN_EXEC_PER_SEC）：不像真跑过" >&2
        rc=1
        continue
    fi

    echo "OK   $name：cov=$edges exec/s=$rate，${SECONDS_PER_TARGET} 秒无崩溃"
done

if [[ "$rc" == "0" ]]; then
    echo "模糊测试判据：${#targets[@]} 个目标全部通过（每个 ${SECONDS_PER_TARGET} 秒，$compiler_banner）"
else
    echo "模糊测试判据：有目标未通过" >&2
fi
exit "$rc"
