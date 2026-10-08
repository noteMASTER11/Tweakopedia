if(NOT DEFINED WINDEPLOYQT OR NOT EXISTS "${WINDEPLOYQT}")
    message(FATAL_ERROR "windeployqt не найден: ${WINDEPLOYQT}")
endif()
if(NOT DEFINED PACKAGE_ROOT OR NOT IS_DIRECTORY "${PACKAGE_ROOT}")
    message(FATAL_ERROR "Portable-каталог не найден: ${PACKAGE_ROOT}")
endif()
if(NOT DEFINED QML_SOURCE OR NOT IS_DIRECTORY "${QML_SOURCE}")
    message(FATAL_ERROR "Каталог QML не найден: ${QML_SOURCE}")
endif()

execute_process(
    COMMAND "${WINDEPLOYQT}"
        --release
        --compiler-runtime
        --no-translations
        --qmldir "${QML_SOURCE}"
        --dir "${PACKAGE_ROOT}"
        "${PACKAGE_ROOT}/Tweakopedia.exe"
    RESULT_VARIABLE deploy_result
    OUTPUT_VARIABLE deploy_output
    ERROR_VARIABLE deploy_error
)
if(NOT deploy_result EQUAL 0)
    message(FATAL_ERROR "windeployqt завершился с кодом ${deploy_result}:\n${deploy_output}\n${deploy_error}")
endif()

message(STATUS "Qt runtime deployed to ${PACKAGE_ROOT}")
