# 顶层只声明编译标准、公共开关与子目录顺序；源文件清单由各模块目录自行登记，
# 避免在一处维护全部文件名而在增删文件时漏改。
cmake_minimum_required(VERSION 3.28)

project(ExpressionEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EXPRESSIONENGINE_BUILD_TESTS "构建单元测试" ON)
option(EXPRESSIONENGINE_BUILD_BENCHMARKS "构建解析器性能基准" OFF)

# 零告警是提交硬判据：库与测试共用同一组告警开关，第三方源码不套用
add_library(ExpressionEngineWarnings INTERFACE)
if(MSVC)
    target_compile_options(ExpressionEngineWarnings INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(ExpressionEngineWarnings INTERFACE -Wall -Wextra -Wpedantic)
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
        # -fno-omit-frame-pointer：栈回溯才有意义，否则报告里的调用栈会缺帧
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
        message(STATUS "Sanitizers: 已启用 AddressSanitizer + UndefinedBehaviorSanitizer")
    endif()
endif()

add_subdirectory(src)

if(EXPRESSIONENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(EXPRESSIONENGINE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
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

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/ExpressionEngineConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/cmake/ExpressionEngineConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/cmake/ExpressionEngineConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ExpressionEngine
)

# LGPL-2.1-or-later 要求随二进制分发许可原文。
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/ExpressionEngine)
