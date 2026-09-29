# 模糊测试目标：与 benchmarks/ 同一类——可执行体、不安装、共用仓库的零告警开关。
#
# 只走 Clang：-fsanitize=fuzzer 是 compiler-rt 的设施（入口 LLVMFuzzerTestOneInput 与它自带的 main），
# GCC 侧没有对应实现。开关打开而编译器不是 Clang 时配置期直接拒，不留下一个「配得出来但跑不动」的
# 目标——那种目标会让判据在别人的机器上变成假绿。
#
# 判据跑法在 tools/fuzz.sh：它负责配置（带 sanitizer）、逐个目标限时跑、把崩溃输入留在语料目录里。

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "EXPRESSIONENGINE_BUILD_FUZZ_TARGETS 需要 Clang（-fsanitize=fuzzer 属 compiler-rt，GCC 没有）；当前编译器：${CMAKE_CXX_COMPILER_ID}")
endif()

add_executable(ExpressionEngineFuzzParse FuzzParse.cpp)

target_link_libraries(ExpressionEngineFuzzParse PRIVATE ExpressionEngine ExpressionEngineWarnings)

# 被测的库本体也要 fuzzer-no-link 插桩，否则覆盖率引导只看得见 fuzz/ 里那一个翻译单元：
# 本机实测只插桩目标时 45 秒跑到 48 条边缘就结束了（全部在 FuzzParse.cpp 内），库里的分支一条都没进
# 判据——那种「没崩溃」不是证据。fuzzer-no-link 而非 fuzzer：静态库带不进第二份 main。
target_compile_options(ExpressionEngine PRIVATE -fsanitize=fuzzer-no-link)

# fuzzer-no-link：编译期插桩但不带 fuzzer 运行时的 main；链接期由下面这条把 fuzzer 运行时接上，
# 少了它就会报 `undefined reference to 'main'`（Scrt1.o 找入口，而入口在 libFuzzer 里）。
# 检测集与用例档一致（ASan 抓越界与泄漏、UBSan 抓未定义行为，且命中即中止）——那两项由顶层
# ENABLE_SANITIZERS 的 add_link_options 带进来，这一档配置时必须打开它。
target_compile_options(ExpressionEngineFuzzParse PRIVATE -fsanitize=fuzzer-no-link)
target_link_options(ExpressionEngineFuzzParse PRIVATE -fsanitize=fuzzer)
