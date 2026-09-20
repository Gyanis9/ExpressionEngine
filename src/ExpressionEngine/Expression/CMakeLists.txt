# 叶子目录：只登记本层源文件，汇总由上层完成
set_property(GLOBAL APPEND PROPERTY ExpressionEngineExpressionSources
        ${CMAKE_CURRENT_LIST_DIR}/ComponentAccess.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ComponentAccess.h
        ${CMAKE_CURRENT_LIST_DIR}/Expression.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Expression.h
        ${CMAKE_CURRENT_LIST_DIR}/ExpressionLexer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ExpressionLexer.h
        ${CMAKE_CURRENT_LIST_DIR}/ExpressionParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ExpressionParser.h
        ${CMAKE_CURRENT_LIST_DIR}/FunctionRegistry.cpp
        ${CMAKE_CURRENT_LIST_DIR}/FunctionRegistry.h
        ${CMAKE_CURRENT_LIST_DIR}/Range.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Range.h
        ${CMAKE_CURRENT_LIST_DIR}/Value.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Value.h
)
