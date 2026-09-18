# 叶子目录：只登记本层源文件，汇总由上层完成
set_property(GLOBAL APPEND PROPERTY ExpressionEngineUnitsSources
        ${CMAKE_CURRENT_LIST_DIR}/Quantity.cpp
        ${CMAKE_CURRENT_LIST_DIR}/QuantityParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Unit.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UnitsApi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UnitsSchema.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UnitsSchemas.cpp
)
