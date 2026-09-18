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

# LGPL-2.1-or-later 要求随二进制分发许可原文与出处说明。
install(FILES LICENSE NOTICE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/ExpressionEngine)
