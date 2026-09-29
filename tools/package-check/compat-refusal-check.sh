#!/usr/bin/env bash
# 包配置兼容判定的反面用例：装出去的包「自称」的版本与消费者请求的版本主次不同且更新时，
# find_package 必须拒绝。交付门与 Windows CI 跑的是同一条脚本，判据不会有两套标准。
#
# 为什么不是「请求一个更高的版本被拒」：本轮实测过，装的是 0.0.1 时请求 9.9.9，任何标准策略
# 都会拒绝——那种写法测不出策略本身。改的必须是被装好的那份版本文件里的版本号：把它抬到 0.9.0
# （主次与请求的 0.0.1 不同、且更新），SameMinorVersion 会拒绝，SameMajorVersion 会接受
# （0.9.0 >= 0.0.1 且主次版本要求不冲突），这一步因此真的能分辨两者。
#
# 用法（在仓库根）：bash tools/package-check/compat-refusal-check.sh <安装前缀，本机原生写法>
# 只改安装前缀里的那一份版本文件，判定后原样放回；消费者工程建在前缀下面的一目录，不碰仓库。

set -u

prefix_native="${1:-}"
if [ "$prefix_native" = "" ]; then
    echo "兼容判定：没给安装前缀。用法：bash tools/package-check/compat-refusal-check.sh <prefix>" >&2
    exit 1
fi

root="$(git rev-parse --show-toplevel)"
consumer_src="$root/tools/package-check"

# 同一个目录的两种方言：文件操作（find/sed/grep/cp）用 shell 认得的形态，cmake 参数用原生形态。
native()
{
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s\n' "$1"
    fi
}

prefix_posix="$prefix_native"
if command -v cygpath >/dev/null 2>&1; then
    prefix_posix="$(cygpath -u "$prefix_native")"
fi

version_file="$(find "$prefix_posix" -name 'ExpressionEngineConfigVersion.cmake' -type f -print -quit)"
if [ -z "$version_file" ]; then
    echo "兼容判定：前缀 $prefix_native 里没有 ExpressionEngineConfigVersion.cmake，安装包接线本身就有问题" >&2
    exit 1
fi

backup="$version_file.compat-refusal.bak"
cp "$version_file" "$backup"
sed -i 's/"0\.0\.1"/"0.9.0"/g' "$version_file"
if ! grep -aq 'set(PACKAGE_VERSION "0.9.0")' "$version_file"; then
    # 版本文件的生成格式变了就拒绝判定，而不是让「改不动」冒充「策略正确」
    mv "$backup" "$version_file"
    echo "兼容判定：改不动版本文件里的 0.0.1（生成写法变了？$version_file）" >&2
    exit 1
fi

log="$(mktemp "${TMPDIR:-/tmp}/ee-compat-refusal.XXXXXX")"
consumer_dir="$prefix_posix/compat-refusal-consumer"
# 每次都要空目录：CMake 会把成功的配置结果写进缓存，留着旧缓存这一步测的是上一次的答案。
rm -rf "$consumer_dir"

cmake -S "$(native "$consumer_src")" -B "$(native "$consumer_dir")" \
    -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$prefix_native" >"$log" 2>&1
rc=$?

mv "$backup" "$version_file"

if [ "$rc" -eq 0 ]; then
    echo "兼容判定：版本自称 0.9.0 时请求 0.0.1 竟然配上了——COMPATIBILITY 不再是 SameMinorVersion（日志 $log）" >&2
    exit 1
fi
if ! grep -aq "requested version" "$log"; then
    # 只看退出码会把「前一步没装好」当成判据通过，那是败得碰巧
    echo "兼容判定：configure 确实失败了，但不是败在版本不兼容上（日志 $log）" >&2
    tail -20 "$log" >&2
    exit 1
fi

echo "兼容判定：主次版本不同且更新时按 SameMinorVersion 拒绝（日志 $log）"
