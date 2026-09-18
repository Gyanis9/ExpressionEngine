# 测试与 src 逐级镜像：叶子目录只登记本层用例，这里汇总成各模块的测试可执行文件。
# GoogleTest 由 Conan 供给（依赖版本见 conandata.yml），按它生成的 find_package 名取用。
find_package(GTest REQUIRED)

add_subdirectory(ExpressionEngine/Base)
add_subdirectory(ExpressionEngine/Units)
add_subdirectory(ExpressionEngine/Expression)

include(GoogleTest)

foreach(MODULE Base Units Expression)
    get_property(TEST_SOURCES GLOBAL PROPERTY ExpressionEngine${MODULE}TestSources)
    if(TEST_SOURCES)
        add_executable(ExpressionEngine${MODULE}Tests ${TEST_SOURCES})
        # 用例没有自己的 main，链接 gtest_main 提供入口
        target_link_libraries(ExpressionEngine${MODULE}Tests
                PRIVATE ExpressionEngine ExpressionEngineWarnings GTest::gtest_main)
        gtest_discover_tests(ExpressionEngine${MODULE}Tests)
    endif()
endforeach()
