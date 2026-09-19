# 模块层：汇总各子目录登记到 GLOBAL PROPERTY 的源文件，建立唯一的库目标。
# 子目录只 append 自己的文件，不在此处重复写依赖关系。
add_subdirectory(ExpressionEngine/Base)
add_subdirectory(ExpressionEngine/Units)
add_subdirectory(ExpressionEngine/Expression)

get_property(BASE_SOURCES GLOBAL PROPERTY ExpressionEngineBaseSources)
get_property(UNITS_SOURCES GLOBAL PROPERTY ExpressionEngineUnitsSources)
get_property(EXPRESSION_SOURCES GLOBAL PROPERTY ExpressionEngineExpressionSources)

add_library(ExpressionEngine STATIC ${BASE_SOURCES} ${UNITS_SOURCES} ${EXPRESSION_SOURCES})

# 源码树与安装树同形：子目录集成时也能用带命名空间的目标名（安装导出映射到同一名字）。
add_library(ExpressionEngine::ExpressionEngine ALIAS ExpressionEngine)

# C++23 是接口的一部分：头文件用了 std::expected、std::format 等设施，下游必须同标准编译。
# 设为 PUBLIC 后随导出目标传递给消费者，装完即用不依赖对方手动开标准。
target_compile_features(ExpressionEngine PUBLIC cxx_std_23)

# 以 src 为包含根，对外暴露 <ExpressionEngine/Base/...> 形式的路径：
# 带库名前缀可避免与其他库同名的 Base/ 目录在同一个编译单元里撞名。
target_include_directories(ExpressionEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ExpressionEngine PRIVATE ExpressionEngineWarnings)
