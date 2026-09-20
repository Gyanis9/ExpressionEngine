# 叶子目录：只登记本层源文件，汇总由上层完成
set_property(GLOBAL APPEND PROPERTY ExpressionEngineBaseSources
        ${CMAKE_CURRENT_LIST_DIR}/Converter.h
        ${CMAKE_CURRENT_LIST_DIR}/DualNumber.h
        ${CMAKE_CURRENT_LIST_DIR}/DualQuaternion.cpp
        ${CMAKE_CURRENT_LIST_DIR}/DualQuaternion.h
        ${CMAKE_CURRENT_LIST_DIR}/Exception.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Exception.h
        ${CMAKE_CURRENT_LIST_DIR}/FirstByteDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/Matrix.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Matrix.h
        ${CMAKE_CURRENT_LIST_DIR}/NumericFormatting.cpp
        ${CMAKE_CURRENT_LIST_DIR}/NumericFormatting.h
        ${CMAKE_CURRENT_LIST_DIR}/NumericInput.cpp
        ${CMAKE_CURRENT_LIST_DIR}/NumericInput.h
        ${CMAKE_CURRENT_LIST_DIR}/ParseFailure.h
        ${CMAKE_CURRENT_LIST_DIR}/Placement.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Placement.h
        ${CMAKE_CURRENT_LIST_DIR}/Precision.h
        ${CMAKE_CURRENT_LIST_DIR}/Rotation.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Rotation.h
        ${CMAKE_CURRENT_LIST_DIR}/Tools.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Tools.h
        ${CMAKE_CURRENT_LIST_DIR}/Vector3D.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Vector3D.h
)
