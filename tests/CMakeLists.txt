# 测试与 src 逐级镜像：叶子目录只登记本层用例，这里汇总成各模块的测试可执行文件。
# GoogleTest 已 vendor 进仓库，构建期不联网下载。
add_subdirectory(ExpressionEngine/Base)

set(FETCHCONTENT_FULLY_DISCONNECTED ON)
add_subdirectory(
        ${CMAKE_SOURCE_DIR}/third_party/googletest
        ${CMAKE_BINARY_DIR}/third_party/googletest
        EXCLUDE_FROM_ALL
)

include(GoogleTest)

foreach(MODULE Base)
    get_property(TEST_SOURCES GLOBAL PROPERTY ExpressionEngine${MODULE}TestSources)
    if(TEST_SOURCES)
        add_executable(ExpressionEngine${MODULE}Tests ${TEST_SOURCES})
        target_link_libraries(ExpressionEngine${MODULE}Tests
                PRIVATE ExpressionEngine ExpressionEngineWarnings GTest::gtest_main)
        gtest_discover_tests(ExpressionEngine${MODULE}Tests)
    endif()
endforeach()
