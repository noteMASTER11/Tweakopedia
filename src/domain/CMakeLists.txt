add_library(TweakopediaDomain STATIC
    BcdElement.cpp
    BcdElement.h
    PowerSetting.cpp
    PowerSetting.h
    WindowsComponent.cpp
    WindowsComponent.h
    FileOperation.cpp
    FileOperation.h
    TweakDefinition.cpp
    TweakDefinition.h
    TweakId.h
    TweakInput.cpp
    TweakInput.h
    RegistryTypes.h
    ScheduledTask.cpp
    ScheduledTask.h
    RegistryTree.cpp
    RegistryTree.h
    SystemOverview.h
    SystemProfile.h
    DetectedState.h
)

target_include_directories(TweakopediaDomain
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(TweakopediaDomain
    PUBLIC
        Qt6::Core
)
