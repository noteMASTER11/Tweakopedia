add_library(TweakopediaExecution STATIC
    ExecutionProtocol.cpp
    ExecutionProtocol.h
    PlanValidator.cpp
    PlanValidator.h
    RegistryDwordExecutor.cpp
    RegistryDwordExecutor.h
    RegistrySnapshot.cpp
    RegistrySnapshot.h
    TransactionRunner.cpp
    TransactionRunner.h
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
        TweakopediaPlatform
        TweakopediaPersistence
)
