# 模块层：汇总各子目录登记到 GLOBAL PROPERTY 的源文件，建立唯一的库目标。
# 子目录只 append 自己的文件，不在此处重复写依赖关系。
add_subdirectory(ExpressionEngine/Base)
add_subdirectory(ExpressionEngine/Units)
add_subdirectory(ExpressionEngine/Expression)

get_property(BASE_SOURCES GLOBAL PROPERTY ExpressionEngineBaseSources)
get_property(UNITS_SOURCES GLOBAL PROPERTY ExpressionEngineUnitsSources)
get_property(EXPRESSION_SOURCES GLOBAL PROPERTY ExpressionEngineExpressionSources)

add_library(ExpressionEngine STATIC ${BASE_SOURCES} ${UNITS_SOURCES} ${EXPRESSION_SOURCES})

# 以 src 为包含根，对外暴露 <ExpressionEngine/Base/...> 形式的路径：
# 带库名前缀可避免与其他库同名的 Base/ 目录在同一个编译单元里撞名。
target_include_directories(ExpressionEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ExpressionEngine PRIVATE ExpressionEngineWarnings)
