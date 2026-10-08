add_library(TweakopediaContent STATIC
    CatalogError.h
    CategoryCatalog.cpp
    CategoryCatalog.h
    CategoryCatalogLoader.cpp
    CategoryCatalogLoader.h
    TweakCatalog.cpp
    TweakCatalog.h
    TweakCatalogLoader.cpp
    TweakCatalogLoader.h
)

target_include_directories(TweakopediaContent
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(TweakopediaContent
    PUBLIC
        Qt6::Core
        TweakopediaDomain
    PRIVATE
        yaml-cpp::yaml-cpp
)
