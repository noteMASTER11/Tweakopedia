qt_add_executable(Tweakopedia WIN32
    main.cpp
    resources.qrc
)

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
)
