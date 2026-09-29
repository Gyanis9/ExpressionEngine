#!/usr/bin/env bash
# 交付门：对**已提交**的树跑一遍完整链路——克隆 -> 构建 -> 全量用例 -> 安装 -> 跑包消费者
# -> 再确认包配置的兼容判定会拒绝更高的版本。
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

# 交给 Windows 一侧（git、cmake/ctest 与被构建出来的 .exe）的路径必须是本机原生写法。
# 在 MSYS/Git Bash 里把 /tmp/... 原样递过去会被改写：实测 `-D TEST_EXECUTABLE=/tmp/...` 变成盘符相对的
# `\tmp/...`，gtest 的构建期用例于是报 "Error running test executable ... no such file or directory"，
# 整棵树以 *_NOT_BUILT 收场——看起来像代码坏了，其实只是路径被吞；而带 MSYS_NO_PATHCONV=1 跑时同一条
# `/tmp/...` 又不转，git 会把克隆落到「当前盘:\tmp\...」，与 cygpath 报出的目录不是同一处。
# 所以克隆目的地与 Windows 工具用的都是 cygpath 转出来的那份，POSIX 形态只留给本 shell 的
# find、日志与清理——两者指向磁盘上同一个目录，第 27 行的「克隆到」就是这个前提的自检。
native()
{
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s\n' "$1"
    fi
}

clone_native=$(native "$clone")
prefix_native=$(native "$prefix")

echo "gate: 分支 $branch_or_head HEAD=$head 构建档 $kind 工作目录 $work"

git clone -q "$root" "$clone_native" || { echo "clone 失败"; exit 1; }
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

run_step configure cmake -S "$clone_native" --preset "$kind" -B "$(native "$clone/build/$kind")"
run_step build cmake --build "$(native "$clone/build/$kind")"
run_step tests ctest --test-dir "$(native "$clone/build/$kind")" --output-on-failure
run_step install cmake --install "$(native "$clone/build/$kind")" --prefix "$prefix_native"

# LGPL-2.1 要求随包分发许可原文与第三方声明；缺一个文件就是合规缺口，而不是「文档没写上」：
# 装完直接按安装树点名核一遍。找不到时退回按名字搜整棵前缀树，避免 CMAKE_INSTALL_DATADIR
# 被改过造成假红。
for doc in LICENSE NOTICE CHANGELOG.md; do
    if [ -f "$prefix/share/doc/ExpressionEngine/$doc" ] || [ -n "$(find "$prefix" -name "$doc" -print -quit 2>/dev/null)" ]; then
        echo "OK   随包文件 $doc"
    else
        echo "FAIL 随包文件 $doc 没装进安装树（期望 share/doc/ExpressionEngine/）"
        rc=1
    fi
done

# 请求的版本取克隆树自己的版本号（`tools/version-check.sh --print`），不在这里写死字面量：
# 写死的那份在库里升版后会与包配置对不上，兼容判定的反面用例随即失去分辨力。
run_step consumer-configure cmake -S "$(native "$clone/tools/package-check")" -B "$(native "$clone/tools/package-check/build")" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="$prefix_native" "-DEXPRESSIONENGINE_REQUESTED_VERSION=$(bash "$clone/tools/version-check.sh" --print)"
run_step consumer-build cmake --build "$(native "$clone/tools/package-check/build")"


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

# 兼容判定的反面用例：判据与 Windows CI 共用同一条脚本，两套标准迟早分叉。放在最后一步，
# 因为它要改动装出去的版本文件——消费者跑完之后再动那份包。
if [ "$rc" -eq 0 ]; then
    run_step consumer-refuses-mismatched-minor bash "$(native "$root/tools/package-check/compat-refusal-check.sh")" "$prefix_native"
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
