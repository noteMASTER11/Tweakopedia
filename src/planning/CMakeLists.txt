add_library(TweakopediaPlanning STATIC
    ExecutionPlan.cpp
    ExecutionPlan.h
    PlanBuilder.cpp
    PlanBuilder.h
    TweakQueue.cpp
    TweakQueue.h
)

target_include_directories(TweakopediaPlanning
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(TweakopediaPlanning
    PUBLIC
        Qt6::Core
        TweakopediaContent
        TweakopediaDetection
        TweakopediaDomain
)
