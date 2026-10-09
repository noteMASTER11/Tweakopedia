add_library(TweakopediaDetection STATIC
    BcdStateDetector.cpp
    BcdStateDetector.h
    PowerSettingStateDetector.cpp
    PowerSettingStateDetector.h
    WindowsComponentStateDetector.cpp
    WindowsComponentStateDetector.h
    AppxPackageStateDetector.cpp
    AppxPackageStateDetector.h
    CompatibilityEvaluator.cpp
    CompatibilityEvaluator.h
    FeatureStateDetector.cpp
    FeatureStateDetector.h
    RegistryDwordStateDetector.cpp
    RegistryDwordStateDetector.h
    RegistryValueStateDetector.cpp
    RegistryValueStateDetector.h
    RegistryTreeStateDetector.cpp
    RegistryTreeStateDetector.h
    ScheduledTaskStateDetector.cpp
    ScheduledTaskStateDetector.h
)

target_include_directories(TweakopediaDetection
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(TweakopediaDetection
    PUBLIC
        Qt6::Core
        TweakopediaDomain
        TweakopediaPlatform
)
