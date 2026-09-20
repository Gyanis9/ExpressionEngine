# 叶子目录：只登记本层用例文件，汇总由上层完成
set_property(GLOBAL APPEND PROPERTY ExpressionEngineBaseTestSources
        ${CMAKE_CURRENT_LIST_DIR}/TestException.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestMatrix.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestNumericFormatting.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestNumericInput.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestPlacement.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestRotation.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestTools.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TestVector3D.cpp
)
