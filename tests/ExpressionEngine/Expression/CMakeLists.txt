# 叶子目录：只登记本层用例文件，汇总由上层完成
set_property(GLOBAL APPEND PROPERTY ExpressionEngineExpressionTestSources
        ${CMAKE_CURRENT_LIST_DIR}/TestComponentAccess.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestExpression.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestExpressionLexer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestExpressionParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestFunctionRegistry.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestValue.cpp
)
