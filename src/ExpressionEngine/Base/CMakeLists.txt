# 叶子目录：只登记本层源文件，汇总由上层完成
set_property(GLOBAL APPEND PROPERTY ExpressionEngineBaseSources
        ${CMAKE_CURRENT_LIST_DIR}/Exception.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Tools.cpp
)
