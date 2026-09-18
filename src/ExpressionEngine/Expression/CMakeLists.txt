# 叶子目录：只登记本层源文件，汇总由上层完成
set_property(GLOBAL APPEND PROPERTY ExpressionEngineExpressionSources
        ${CMAKE_CURRENT_LIST_DIR}/ComponentAccess.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Expression.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ExpressionLexer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ExpressionParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Range.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Value.cpp
)
