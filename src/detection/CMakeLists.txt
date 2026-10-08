add_library(TweakopediaDetection STATIC
    CompatibilityEvaluator.cpp
    CompatibilityEvaluator.h
)

target_include_directories(TweakopediaDetection
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(TweakopediaDetection
    PUBLIC
        Qt6::Core
        TweakopediaDomain
)
