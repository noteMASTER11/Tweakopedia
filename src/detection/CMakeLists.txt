add_library(TweakopediaDetection STATIC
    AppxPackageStateDetector.cpp
    AppxPackageStateDetector.h
    CompatibilityEvaluator.cpp
    CompatibilityEvaluator.h
    FeatureStateDetector.cpp
    FeatureStateDetector.h
    RegistryDwordStateDetector.cpp
    RegistryDwordStateDetector.h
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
