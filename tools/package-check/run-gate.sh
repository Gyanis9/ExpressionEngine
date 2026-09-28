#!/usr/bin/env bash
# 交付门：对**已提交**的树跑一遍完整链路——克隆 -> 构建 -> 全量用例 -> 安装 -> 跑包消费者。
#
# 为什么要克隆而不是用工作树：未提交的改动会搭车，跑绿了不代表别人拿到的那一版能编。
# 为什么最后一步是仓库外消费者：库内用例从源码树 include，看不见导出头漏装、包配置写错、
# 版本号不匹配这类接线缺陷（tools/package-check 的 CMakeLists 里写了手动跑法）。
#
# 每步的退出码单独记，最后一起判：管道里的 grep 会吞掉构建失败，只看末条命令的退出码
# 会得到「构建红了但脚本报成功」的假绿。
#
# 用法：bash tools/package-check/run-gate.sh [构建档，默认 debug]

set -u

branch_or_head=$(git rev-parse --abbrev-ref HEAD)
head=$(git rev-parse --short HEAD)
root=$(git rev-parse --show-toplevel)
kind=${1:-debug}
work=$(mktemp -d "${TMPDIR:-/tmp}/ee-gate.XXXXXX")
clone="$work/clone"
prefix="$work/prefix"
rc=0

echo "gate: 分支 $branch_or_head HEAD=$head 构建档 $kind 工作目录 $work"

git clone -q "$root" "$clone" || { echo "clone 失败"; exit 1; }
echo "克隆到 $(cd "$clone" && git rev-parse --short HEAD)"

run_step()
{
    local name=$1
    shift
    if "$@" > "$work/$name.log" 2>&1; then
        echo "OK   $name"
    else
        echo "FAIL $name（退出码 $?，日志 $work/$name.log）"
        tail -20 "$work/$name.log"
        rc=1
    fi
}

run_step configure cmake -S "$clone" --preset "$kind" -B "$clone/build/$kind"
run_step build cmake --build "$clone/build/$kind"
run_step tests ctest --test-dir "$clone/build/$kind" --output-on-failure
run_step install cmake --install "$clone/build/$kind" --prefix "$prefix"
run_step consumer-configure cmake -S "$clone/tools/package-check" -B "$clone/tools/package-check/build" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="$prefix"
run_step consumer-build cmake --build "$clone/tools/package-check/build"

if [ "$rc" -eq 0 ]; then
    # 产物名与位置随平台、生成器而变（Windows 带 .exe；VS 多配置放进 <配置>/ 子目录），按实际文件找
    exe=$(find "$clone/tools/package-check/build" -maxdepth 3 -type f \( -name consumerCheck -o -name 'consumerCheck.exe' \) -print -quit)
    if [ -z "$exe" ]; then
        echo "FAIL 找不到消费者可执行文件，目录 $clone/tools/package-check/build"
        rc=1
    elif "$exe" > "$work/consumer-run.log" 2>&1; then
        echo "OK   consumer-run：$(tail -1 "$work/consumer-run.log")"
    else
        echo "FAIL consumer-run（日志 $work/consumer-run.log）"
        tail -20 "$work/consumer-run.log"
        rc=1
    fi
fi

# 只数真正的编译器诊断：宽松匹配会把 -Werror、EXPRESSIONENGINE_WARNINGS_AS_ERRORS 这类
# 命令行里的字样算成告警。
if [ -f "$work/build.log" ]; then
    echo "构建诊断行数：$(grep -acE "warning C[0-9]+|error C[0-9]+|: warning:|: error:" "$work/build.log")"
fi

if [ "$rc" -ne 0 ]; then
    echo "日志留在 $work"
else
    rm -rf "$work"
fi
echo "gate 结论：$([ "$rc" -eq 0 ] && echo 全绿 || echo 有失败)"
exit "$rc"
