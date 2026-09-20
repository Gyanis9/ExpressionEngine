# 安装包消费者检查：不参与主构建（顶层 CMakeLists 不 add_subdirectory 本目录）。
#
# 它存在的理由：库自己的用例都在源码树内跑，发现不了「导出头与安装规则接线坏了」这一类
# 缺陷——中文注释的导出头会让不带 /utf-8 的 MSVC 消费者直接编译失败，这类问题只有在仓库外
# 另起一个工程才看得见。
#
# 本地跑法（与 CI 里的步骤一致）：
#   cmake --preset debug && cmake --build build/debug
#   cmake --install build/debug --prefix <前缀>
#   cmake -S tools/package-check -B tools/package-check/build -DCMAKE_PREFIX_PATH=<前缀>
#   cmake --build tools/package-check/build && tools/package-check/build/consumerCheck.exe
# 退出码为 0 且末行打印 failed=0 才算过。

cmake_minimum_required(VERSION 3.28)

project(ExpressionEngineConsumerCheck CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ExpressionEngine CONFIG REQUIRED)

add_executable(consumerCheck main.cpp)
target_link_libraries(consumerCheck PRIVATE ExpressionEngine::ExpressionEngine)

if(MSVC)
    # /utf-8 是 Windows 消费者读中文导出头的必要条件，写在这里而不是依赖编译器默认值
    target_compile_options(consumerCheck PRIVATE /W4 /permissive- /utf-8)
endif()
