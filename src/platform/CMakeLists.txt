add_library(TweakopediaPlatform STATIC
    ISystemProfileProvider.h
    WindowsSystemProfileProvider.cpp
    WindowsSystemProfileProvider.h
)

target_include_directories(TweakopediaPlatform
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(TweakopediaPlatform
    PUBLIC
        Qt6::Core
        TweakopediaDomain
    PRIVATE
        advapi32
)
