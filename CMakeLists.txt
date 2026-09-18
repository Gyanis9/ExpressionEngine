# 顶层只声明编译标准、公共开关与子目录顺序；源文件清单由各模块目录自行登记，
# 避免在一处维护全部文件名而在增删文件时漏改。
cmake_minimum_required(VERSION 3.28)

project(ExpressionEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EXPRESSIONENGINE_BUILD_TESTS "构建单元测试" ON)

add_subdirectory(src)

if(EXPRESSIONENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
