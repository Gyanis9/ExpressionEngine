# 叶子目录：只登记本层用例文件，汇总由上层完成
set_property(GLOBAL APPEND PROPERTY ExpressionEngineUnitsTestSources
        ${CMAKE_CURRENT_LIST_DIR}/TestQuantity.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestQuantityParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestUnit.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestUnitsSchema.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestUnitsSchemas.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestUnitsSchemasData.cpp
)
