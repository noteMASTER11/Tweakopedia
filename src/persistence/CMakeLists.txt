add_library(TweakopediaPersistence STATIC
    AppPaths.cpp
    AppPaths.h
    AppSettings.cpp
    AppSettings.h
    Database.cpp
    Database.h
    PendingInputStore.cpp
    PendingInputStore.h
    TransactionFiles.cpp
    TransactionFiles.h
    TransactionRecord.h
    TransactionRepository.cpp
    TransactionRepository.h
)

qt_add_resources(TweakopediaPersistence persistence_migrations
    PREFIX "/tweakopedia/persistence"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        migrations/001_initial.sql
)

target_include_directories(TweakopediaPersistence
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(TweakopediaPersistence
    PUBLIC
        Qt6::Core
        Qt6::Sql
)
