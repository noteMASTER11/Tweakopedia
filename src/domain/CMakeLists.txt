add_library(TweakopediaDomain STATIC
    TweakDefinition.cpp
    TweakDefinition.h
    TweakId.h
    RegistryTypes.h
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
