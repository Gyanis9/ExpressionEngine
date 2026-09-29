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
# 退出码为 0 且末行打印 failed=0 才算过。整条链路（克隆->构建->用例->安装->消费者）
# 已由同目录的 run-gate.sh 包起来，日常验收入口是：bash tools/package-check/run-gate.sh

cmake_minimum_required(VERSION 3.28)

project(ExpressionEngineConsumerCheck CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 带上版本号，包配置的兼容判定（COMPATIBILITY）才会被走到；不带版本时它根本不被读取。
# 请求哪个版本由调用方传（交付门与 CI 都传库里那份版本号），这样升版时不必来改这里——
# 版本号写两处的下场是：库里升到 0.0.2，这一步还在请求 0.0.1，兼容判定的反面用例改不动版本文件。
set(EXPRESSIONENGINE_REQUESTED_VERSION "" CACHE STRING "消费者向包配置请求的最低版本")
if (EXPRESSIONENGINE_REQUESTED_VERSION STREQUAL "")
    find_package(ExpressionEngine CONFIG REQUIRED)
else()
    find_package(ExpressionEngine ${EXPRESSIONENGINE_REQUESTED_VERSION} CONFIG REQUIRED)
endif()

add_executable(consumerCheck main.cpp)
target_link_libraries(consumerCheck PRIVATE ExpressionEngine::ExpressionEngine)

if(MSVC)
    # /utf-8 是 Windows 消费者读中文导出头的必要条件，写在这里而不是依赖编译器默认值
    target_compile_options(consumerCheck PRIVATE /W4 /permissive- /utf-8)
endif()
