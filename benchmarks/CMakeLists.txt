# 基准目录：只登记基准可执行文件，不引入第三方基准库，计时循环写在源文件里。
# 目标链接主库并共用仓库的告警开关，保证基准代码也按零告警标准编译。
add_executable(ParserBenchmark ParserBenchmark.cpp)

target_link_libraries(ParserBenchmark PRIVATE ExpressionEngine ExpressionEngineWarnings)

# 基准必须跑 Release：Debug 的迭代器调试与不优化会让数字失去意义，配置期就把话说清。
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(WARNING "基准在 ${CMAKE_BUILD_TYPE} 构建下没有意义，请用 -DCMAKE_BUILD_TYPE=Release 重新配置")
endif()

# 把构建类型与关键编译选项送进二进制，基准报告里直接打印，避免数字脱离构建配置。
string(TOUPPER "${CMAKE_BUILD_TYPE}" PARSERBENCH_BUILD_TYPE_UPPER)
if(MSVC)
    set(PARSERBENCH_WARNING_FLAGS "/W4 /permissive- /utf-8 /Zc:__cplusplus")
else()
    set(PARSERBENCH_WARNING_FLAGS "-Wall -Wextra -Wpedantic")
endif()
target_compile_definitions(
        ParserBenchmark
        PRIVATE BENCH_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
                BENCH_COMPILER_FLAGS="${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${PARSERBENCH_BUILD_TYPE_UPPER}} ${PARSERBENCH_WARNING_FLAGS}"
)

# 可选的数量解析对比：FreeCAD 的 flex/bison 生成代码（Quantity.tab.c / Quantity.lex.c）不随仓库分发，
# 由本机目录提供；目录缺失时只跑手写解析器一侧，不阻塞基准构建。
set(EXPRESSIONENGINE_LEGACY_QUANTITY_DIR "C:/Users/Gyanis/Downloads/FreeCAD/src/Base"
    CACHE PATH "FreeCAD 生成代码（Quantity.tab.c / Quantity.lex.c）所在目录，仅供基准对比")
if(EXISTS "${EXPRESSIONENGINE_LEGACY_QUANTITY_DIR}/Quantity.tab.c" AND EXISTS "${EXPRESSIONENGINE_LEGACY_QUANTITY_DIR}/Quantity.lex.c")
    target_sources(ParserBenchmark PRIVATE LegacyQuantityParser.cpp)
    target_include_directories(ParserBenchmark PRIVATE "${EXPRESSIONENGINE_LEGACY_QUANTITY_DIR}")
    target_compile_definitions(ParserBenchmark PRIVATE BENCH_HAS_LEGACY_QUANTITY=1)
    # 生成代码是第三方 C 风格代码，只对这个文件放宽告警（/W 是后者生效），不动全局告警级别。
    if(MSVC)
        set_source_files_properties(LegacyQuantityParser.cpp PROPERTIES COMPILE_OPTIONS "/W0")
    else()
        set_source_files_properties(LegacyQuantityParser.cpp PROPERTIES COMPILE_OPTIONS "-w")
    endif()
    message(STATUS "数量解析对比已启用，生成代码来自：${EXPRESSIONENGINE_LEGACY_QUANTITY_DIR}")
else()
    message(STATUS "未找到 ${EXPRESSIONENGINE_LEGACY_QUANTITY_DIR} 下的生成代码，基准只跑手写解析器")
endif()
