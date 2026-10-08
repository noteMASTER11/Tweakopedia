add_library(TweakopediaExecution STATIC
    ExecutionProtocol.cpp
    ExecutionProtocol.h
    PlanValidator.cpp
    PlanValidator.h
)

target_include_directories(TweakopediaExecution
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(TweakopediaExecution
    PUBLIC
        Qt6::Core
        TweakopediaDomain
        TweakopediaPlanning
)
