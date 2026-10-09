add_library(TweakopediaBootstrapProgress STATIC
    BootstrapProgressClient.cpp
    BootstrapProgressClient.h
)
target_include_directories(TweakopediaBootstrapProgress PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TweakopediaBootstrapProgress PUBLIC Qt6::Core TweakopediaStartup)
if(WIN32)
    target_link_libraries(TweakopediaBootstrapProgress PRIVATE user32)
endif()

qt_add_executable(Tweakopedia WIN32
    main.cpp
    UiTypography.cpp
    UiTypography.h
    resources.qrc
    ${PROJECT_SOURCE_DIR}/assets/branding/tweakopedia.rc
)
set_target_properties(Tweakopedia PROPERTIES OUTPUT_NAME "Tweakopedia.App")

if(BUILD_TESTING)
    target_compile_definitions(Tweakopedia PRIVATE
        TWEAKOPEDIA_SOURCE_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content"
    )
endif()

target_link_libraries(Tweakopedia PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    TweakopediaApp
    TweakopediaContent
    TweakopediaDetection
    TweakopediaExecution
    TweakopediaPersistence
    TweakopediaPlatform
    TweakopediaBootstrapProgress
)

if(WIN32)
    target_link_libraries(Tweakopedia PRIVATE dwmapi)
endif()
