# 模块层：汇总各子目录登记到 GLOBAL PROPERTY 的源文件，建立唯一的库目标。
# 子目录只 append 自己的文件，不在此处重复写依赖关系。
add_subdirectory(ExpressionEngine/Base)

get_property(BASE_SOURCES GLOBAL PROPERTY ExpressionEngineBaseSources)

add_library(ExpressionEngine STATIC ${BASE_SOURCES})

# 以 src 为包含根，对外暴露 <ExpressionEngine/Base/...> 形式的路径：
# 带库名前缀可避免与其他库同名的 Base/ 目录在同一个编译单元里撞名。
target_include_directories(ExpressionEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 零告警是提交硬判据；/Zc:__cplusplus 让 __cplusplus 反映真实标准版本
if(MSVC)
    target_compile_options(ExpressionEngine PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(ExpressionEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()
