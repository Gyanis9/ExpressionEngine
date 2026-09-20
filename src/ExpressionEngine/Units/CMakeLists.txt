# 叶子目录：只登记本层源文件，汇总由上层完成
set_property(GLOBAL APPEND PROPERTY ExpressionEngineUnitsSources
        ${CMAKE_CURRENT_LIST_DIR}/Quantity.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Quantity.h
        ${CMAKE_CURRENT_LIST_DIR}/QuantityParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/QuantityParser.h
        ${CMAKE_CURRENT_LIST_DIR}/Unit.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Unit.h
        ${CMAKE_CURRENT_LIST_DIR}/UnitsApi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UnitsApi.h
        ${CMAKE_CURRENT_LIST_DIR}/UnitsConvData.h
        ${CMAKE_CURRENT_LIST_DIR}/UnitsSchema.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UnitsSchema.h
        ${CMAKE_CURRENT_LIST_DIR}/UnitsSchemas.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UnitsSchemas.h
        ${CMAKE_CURRENT_LIST_DIR}/UnitsSchemasData.h
        ${CMAKE_CURRENT_LIST_DIR}/UnitsSchemasSpecifications.h
)
