# 顶层只声明编译标准、公共开关与子目录顺序；源文件清单由各模块目录自行登记，
# 避免在一处维护全部文件名而在增删文件时漏改。
cmake_minimum_required(VERSION 3.28)

project(ExpressionEngine VERSION 0.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EXPRESSIONENGINE_BUILD_TESTS "构建单元测试" ON)
option(EXPRESSIONENGINE_BUILD_BENCHMARKS "构建解析器性能基准" OFF)
option(EXPRESSIONENGINE_BUILD_FUZZ_TARGETS "构建 libFuzzer 模糊测试目标（仅 Clang，跑法见 tools/fuzz.sh）" OFF)

# 零告警是提交硬判据：库与测试共用同一组告警开关，第三方源码不套用
add_library(ExpressionEngineWarnings INTERFACE)
if(MSVC)
    target_compile_options(ExpressionEngineWarnings INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(ExpressionEngineWarnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# 判据要能机检：默认把上面这组开关下的告警直接变成错误，CI 与本地构建都拦得住。
# 关掉它是给自己留后门，只在换编译器小版本、出现无法自证的第三方新告警时临时使用。
option(EXPRESSIONENGINE_WARNINGS_AS_ERRORS "把告警当作错误" ON)

if(EXPRESSIONENGINE_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(ExpressionEngineWarnings INTERFACE /WX)
    else()
        target_compile_options(ExpressionEngineWarnings INTERFACE -Werror)
    endif()
endif()

# ---------------------------------------------------------------------------
# 内存与未定义行为检测（CI 与需要插桩的构建用它）
# ---------------------------------------------------------------------------
# 能力边界决定了这里不是一套参数打天下：MSVC 只提供 AddressSanitizer
# （/fsanitize=address），GCC/Clang 上顺带打开 UBSan（ASan 不覆盖未定义行为）。
#
# 默认关闭：MSVC 的 ASan 运行库位于 cl.exe 同目录、不在系统 PATH 上，插桩出来的
# 可执行文件在裸 shell 里会以 0xC0000135 启动失败，不适合当调试构建的默认值；
# CI 的开发者环境里 cl.exe 在 PATH 上，因此由工作流显式打开。
option(ENABLE_SANITIZERS "启用 AddressSanitizer（GCC/Clang 上并含 UBSan）" OFF)

if(ENABLE_SANITIZERS)
    if(MSVC)
        add_compile_options(/fsanitize=address)
        add_link_options(/fsanitize=address)

        # Conan 提供的 gtest 没有插桩，它的目标文件里没有 annotate_vector /
        # annotate_string / annotate_optional 三个 ABI 标记，与本库插桩后的对象对不上，
        # 链接会直接以 LNK2038 失败。关掉注解即可对齐：失去的只是容器越界与迭代器
        # 失效这类注解检查，堆、栈、悬垂指针等主要检测不受影响。
        add_compile_definitions(_DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION _DISABLE_OPTIONAL_ANNOTATION)

        message(STATUS "Sanitizers: 已启用 AddressSanitizer（MSVC 无 UBSan；因 gtest 未插桩而关闭容器注解）")
    else()
        # -fno-omit-frame-pointer：栈回溯才有意义，否则报告里的调用栈会缺帧。
        # -fno-sanitize-recover=all：UBSan 默认只打印诊断然后继续执行，那样门禁永远全绿；
        # 加上它，未定义行为直接中止进程，ctest 才会真的红。
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        add_link_options(-fsanitize=address,undefined)
        message(STATUS "Sanitizers: 已启用 AddressSanitizer + UndefinedBehaviorSanitizer（不可恢复，命中即失败）")
    endif()
endif()

# ---------------------------------------------------------------------------
# 线程检测器（另开一道，不并入上面的开关）
# ---------------------------------------------------------------------------
# TSan 与 ASan 互斥：同时插桩会被编译器直接拒绝，所以它只能自成一种构建档。
# MSVC 没有 TSan，这条判据因此只在 GCC/Clang 上成立——README 里「无竞争」的证明也只挂在这里。
# 另外 TSan 默认只打印报告并继续跑（halt_on_error=0），进程退出码仍为 0；要让门禁真的红，
# 跑测试时必须给 TSAN_OPTIONS=halt_on_error=1，见 linux-ci.yml 的 tsan 作业。
option(ENABLE_THREAD_SANITIZER "启用 ThreadSanitizer（仅 GCC/Clang，与 ENABLE_SANITIZERS 互斥）" OFF)

if(ENABLE_THREAD_SANITIZER)
    if(MSVC)
        message(FATAL_ERROR "MSVC 没有 ThreadSanitizer；这条判据只能在 GCC/Clang 上跑（linux-ci.yml 的 tsan 作业）")
    endif()
    if(ENABLE_SANITIZERS)
        message(FATAL_ERROR "ENABLE_THREAD_SANITIZER 与 ENABLE_SANITIZERS 互斥：TSan 与 ASan 不能同时插桩")
    endif()

    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
    message(STATUS "Sanitizers: 已启用 ThreadSanitizer（运行时需 TSAN_OPTIONS=halt_on_error=1 才会以非零码退出）")
endif()

option(ENABLE_COVERAGE "启用 gcov 插桩（仅 GCC/Clang，判据在 tools/coverage.sh）" OFF)

if(ENABLE_COVERAGE)
    if(MSVC)
        message(FATAL_ERROR "MSVC 没有 gcov；覆盖率判据只能在 GCC/Clang 上跑（linux-ci.yml 的 coverage 作业）")
    endif()

    # -O0 与 -g 写在插桩这一行而不是依赖构建档：Release 树上开内联会把行号归到别的行，
    # 覆盖率门要能在任一构建档上跑而不改变判据的含义。
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
    message(STATUS "Coverage: 已启用 gcov 插桩（出报告与阈值判定跑 bash tools/coverage.sh）")
endif()

add_subdirectory(src)

if(EXPRESSIONENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(EXPRESSIONENGINE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(EXPRESSIONENGINE_BUILD_FUZZ_TARGETS)
    add_subdirectory(fuzz)
endif()

# ---------------------------------------------------------------------------
# 安装与导出：让下游 find_package(ExpressionEngine) 之后直接链
# ExpressionEngine::ExpressionEngine。third_party、tests、benchmarks 一律不安装。
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# 导出目标的包含目录必须区分构建树与安装树：src/CMakeLists.txt 登记的是源码树绝对
# 路径，原样导出会被 CMake 以「INTERFACE_INCLUDE_DIRECTORIES 指向源码目录」拒绝。
# 这里按导出需要重写该属性，库自身编译仍走 INCLUDE_DIRECTORIES 里的原路径。
set_target_properties(
        ExpressionEngine
        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>;$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# 头文件里的中文注释是 UTF-8（无 BOM），MSVC 默认按系统代码页读源码，会把多字节序列
# 解析错，报错位置还落在标准库头文件里。源码编码属于接口的一部分，必须随导出目标交给
# 下游，否则「装完就能用」只在 /utf-8 环境成立。
# （告警开关的 INTERFACE 目标走 $<LINK_ONLY:>，编译选项不会外溢，所以这里单独补一条。）
if(MSVC)
    target_compile_options(ExpressionEngine INTERFACE /utf-8)
endif()

# 告警开关目标（INTERFACE）必须进同一个导出集：静态库的 PRIVATE 依赖会以
# $<LINK_ONLY:...> 留在链接接口里，缺了它 install(EXPORT) 会直接报错。
# 它在导出后只是链接接口上的占位，编译选项不会外溢给下游。
install(
        TARGETS ExpressionEngine ExpressionEngineWarnings
        EXPORT ExpressionEngineTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 只装头文件，保留 <ExpressionEngine/Base/...> 的相对布局。
install(
        DIRECTORY src/ExpressionEngine
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
)

install(
        EXPORT ExpressionEngineTargets
        NAMESPACE ExpressionEngine::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ExpressionEngine
)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExpressionEngineConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/ExpressionEngineConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ExpressionEngine
)

# 0.x 阶段按语义化版本不承诺兼容，兼容判定收到「主次版本都相同」这一档；
# 用 SameMajorVersion 会让 find_package 把 0.9.0 当成 0.0.1 的兼容版本接受。
write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/ExpressionEngineConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion
)

install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/cmake/ExpressionEngineConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/cmake/ExpressionEngineConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ExpressionEngine
)

# LGPL-2.1-or-later 要求随二进制分发许可原文；NOTICE 里的第三方与上游声明同一条款要求随包，
# 更新日志一起装上，宿主拿到包就能看到迁移事项。
install(
        FILES LICENSE NOTICE CHANGELOG.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/ExpressionEngine
)
