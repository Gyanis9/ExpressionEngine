#!/usr/bin/env bash
# 源码分发包：产出 dist/ExpressionEngine-<版本>.tar.gz 与 SHA256SUMS，并当场自检。
#
# 为什么要这道门：README 教宿主 `find_package` 用装出来的包，也教他们从源码构建，但没人回答
# 「我手上这份包是不是那份提交的内容」。校验和与文件清单把这句话变成可判定的。
#
# 三种模式（都在仓库根跑）：
#   bash tools/make-dist.sh                  只产出包并自检完整性（版本一致、清单与提交逐条相同、校验和可重放）
#   bash tools/make-dist.sh --check-packaging  解包后配置并装包、跑仓库外消费者（不开用例，快）
#   bash tools/make-dist.sh --verify           解包后跑完整链路：配置 -> 构建 -> 全量用例 -> 安装 -> 消费者
#
# 判据一律 fail-closed：读不到版本号、工作树脏、清单少一个文件、校验和对不上，都是当场拒绝而不是
# 给一份看起来成功的产物。

set -uo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root" || exit 1
mode="${1:-pack}"

die()
{
    echo "分发包：$*" >&2
    exit 1
}

native()
{
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s\n' "$1"
    fi
}

# 模式校验要放在 die 定义之后：放在前面时 die 还不存在，bash 只会报「command not found」然后
# 继续往下跑——未知模式于是被当成 --verify 执行，本该拒绝判定的门变成了放行。
case "$mode" in
    pack | --check-packaging | --verify) ;;
    *) die "未知模式 $mode（可用：不带参数、--check-packaging、--verify）" ;;
esac

# ---------------------------------------------------------------------------
# 版本与来历：同一份版本信息写在两个文件里（CMakeLists 与 conanfile.py），
# 没有任何东西保证它们一致——发出去才发现包名与包配置的版本号不同，宿主两头对不上。
# ---------------------------------------------------------------------------
# 版本判定上收到 tools/version-check.sh：两处配置源、每个头文件的 @version、HEAD 上的标签一次判完。
# 同一份口径不该在两个脚本里各写一遍——写两遍就会有一天只改一遍。
cmake_version="$(bash tools/version-check.sh --print)" || die "取不到规范版本号（tools/version-check.sh 读不出配置源）"
[[ -n "$cmake_version" ]] || die "tools/version-check.sh --print 给出空版本号"
# 红的时候把逐条点名的清单原样打出去，绿的时候保持安静：判据的输出不该被包一层日志再让人翻
version_report="$(bash tools/version-check.sh 2>&1)" || { printf '%s\n' "$version_report" >&2; die "版本一致性门未过"; }

# 包必须对应一个提交：脏工作树打出来的包与同一个提交的包内容不同，校验和就失去意义
git diff --quiet HEAD -- || die "工作树有未提交的改动，分发包必须是某个提交的内容"
git diff --cached --quiet || die "暂存区有未提交的改动，分发包必须是某个提交的内容"

commit="$(git rev-parse --short HEAD)"
name="ExpressionEngine-$cmake_version"
mkdir -p dist
tarball="dist/$name.tar.gz"

# ---------------------------------------------------------------------------
# 产出：git archive 只取被版本控制的内容，因此 .gitignore 里的构建产物、IDE 配置
# 与本地生成的 CMakeUserPresets.json 一律不会混进包里。
# ---------------------------------------------------------------------------
git archive --format=tar.gz --prefix="$name/" -o "$tarball" HEAD || die "git archive 失败"

( cd dist && sha256sum -- "./$name.tar.gz" > SHA256SUMS ) || die "写校验和失败"

# 校验和可重放：把清单读回来再核一遍，抓到「写完就被截断/改写」这一类问题
( cd dist && sha256sum -c --status SHA256SUMS ) || die "SHA256SUMS 复核不过"

# ---------------------------------------------------------------------------
# 文件清单逐条比对：数量相同还不够，要每一条都对得上。
# ---------------------------------------------------------------------------
inside="$(mktemp)"
outside="$(mktemp)"
tar -tzf "$tarball" | grep -v '/$' | sed "s|^$name/||" | sort >"$inside"
git ls-files | sort >"$outside"

if ! diff -u "$outside" "$inside" >"$root/dist/filelist.diff"; then
    echo "分发包：包内文件清单与提交 $commit 的受版本控制清单不一致，差异见 dist/filelist.diff" >&2
    head -20 "$root/dist/filelist.diff" >&2
    rm -f "$inside" "$outside"
    die "清单不一致"
fi
rm -f "$root/dist/filelist.diff"

# 缺了这些文件，包就装不出可用产品或跑不了判据：单独点名核一遍，别让上面那条清单比对
# 在某天改成「只比数量」时静默放过
required=(
    CMakeLists.txt CMakePresets.json conan_provider.cmake conanfile.py conandata.yml
    LICENSE README.md CHANGELOG.md CONTRIBUTING.md
    .clang-format .clang-tidy .github/workflows/windows-ci.yml .github/workflows/linux-ci.yml
    tools/format-check.sh tools/tidy-check.sh tools/coverage.sh tools/make-dist.sh tools/fuzz.sh
    fuzz/CMakeLists.txt fuzz/FuzzParse.cpp fuzz/parse.dict
    tools/package-check/run-gate.sh tools/package-check/compat-refusal-check.sh
    tools/package-check/CMakeLists.txt tools/package-check/main.cpp
)
for file in "${required[@]}"; do
    grep -qx "$file" "$inside" || die "包里少了 $file"
done

tracked="$(wc -l <"$outside" | tr -d ' ')"
rm -f "$inside" "$outside"
echo "分发包：dist/$name.tar.gz（提交 $commit，版本 $cmake_version，$tracked 个文件，校验和已复核）"

if [[ "$mode" == "pack" ]]; then
    exit 0
fi

# ---------------------------------------------------------------------------
# 解包后按真实用法走一遍：临时前缀，跑完删掉；失败时把目录留在原地供人查。
# ---------------------------------------------------------------------------
work="$(mktemp -d "${TMPDIR:-/tmp}/ee-dist.XXXXXX")"
src="$work/$name"
prefix="$work/prefix"
prefix_native="$(native "$prefix")"
src_native="$(native "$src")"
rc=0

echo "分发包：解到 $src 验证（模式 $mode）"
tar -xzf "$tarball" -C "$work" || die "解包失败"
# 自证真的解出来了，而不是后面所有步骤都在读一个空目录
[[ -f "$src/CMakeLists.txt" ]] || die "解包后找不到 CMakeLists.txt"
grep -q "VERSION $cmake_version" "$src/CMakeLists.txt" || die "解包后的版本号与打包时不一致"

run_step()
{
    local step=$1
    shift
    if "$@" >"$work/$step.log" 2>&1; then
        echo "OK   $step"
    else
        echo "FAIL $step（退出码 $?，日志 $work/$step.log）" >&2
        tail -20 "$work/$step.log" >&2
        rc=1
    fi
}

if [[ "$mode" == "--check-packaging" ]]; then
    run_step configure cmake -S "$src_native" -B "$(native "$src/build/noint")" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DEXPRESSIONENGINE_BUILD_TESTS=OFF
    run_step build cmake --build "$(native "$src/build/noint")"
    run_step install cmake --install "$(native "$src/build/noint")" --prefix "$prefix_native"
else
    run_step configure cmake -S "$src_native" --preset debug -B "$(native "$src/build/debug")"
    run_step build cmake --build "$(native "$src/build/debug")"
    run_step tests ctest --test-dir "$(native "$src/build/debug")" --output-on-failure
    # 报出实际跑了多少条：只说「OK tests」的人看不出这一步是空跑还是全量
    grep -a -h "tests passed" "$work/tests.log" | tail -1
    run_step install cmake --install "$(native "$src/build/debug")" --prefix "$prefix_native"
fi

if [[ "$rc" -eq 0 ]]; then
    run_step consumer-configure cmake -S "$(native "$src/tools/package-check")" -B "$(native "$src/tools/package-check/build")" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$prefix_native"
    run_step consumer-build cmake --build "$(native "$src/tools/package-check/build")"
    exe="$(find "$src/tools/package-check/build" -maxdepth 3 -type f \( -name consumerCheck -o -name 'consumerCheck.exe' \) -print -quit)"
    if [[ -z "$exe" ]]; then
        echo "FAIL 找不到消费者可执行文件" >&2
        rc=1
    elif "$exe" >"$work/consumer-run.log" 2>&1; then
        echo "OK   consumer-run：$(tail -1 "$work/consumer-run.log")"
    else
        echo "FAIL consumer-run（日志 $work/consumer-run.log）" >&2
        tail -20 "$work/consumer-run.log" >&2
        rc=1
    fi
fi

if [[ "$rc" -eq 0 ]]; then
    rm -rf "$work"
    echo "分发包：从包里的内容装出来的包能用（日志目录已清理）"
else
    echo "分发包：有失败步骤，日志留在 $work" >&2
fi
exit "$rc"
