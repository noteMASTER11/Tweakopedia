add_library(TweakopediaExecution STATIC
    ExecutionProtocol.cpp
    ExecutionProtocol.h
    ExecutorClient.cpp
    ExecutorClient.h
    ExecutorLauncher.cpp
    ExecutorLauncher.h
    ExecutorServer.cpp
    ExecutorServer.h
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
        Qt6::Network
        TweakopediaDomain
        TweakopediaPlanning
        TweakopediaPlatform
        TweakopediaPersistence
)
