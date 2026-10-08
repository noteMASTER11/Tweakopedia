add_library(TweakopediaBootstrapCore STATIC
    BootstrapApplication.cpp
    BootstrapApplication.h
    IBootstrapPlatform.h
    WindowsBootstrapPlatform.cpp
    WindowsBootstrapPlatform.h
)
target_include_directories(TweakopediaBootstrapCore PUBLIC ${PROJECT_SOURCE_DIR}/apps)
target_link_libraries(TweakopediaBootstrapCore PUBLIC TweakopediaPackage PRIVATE shell32)

add_executable(TweakopediaBootstrap WIN32 main.cpp)
set_target_properties(TweakopediaBootstrap PROPERTIES OUTPUT_NAME Tweakopedia)
target_link_libraries(TweakopediaBootstrap PRIVATE TweakopediaBootstrapCore)
if(MINGW)
    target_link_options(TweakopediaBootstrap PRIVATE
        -municode -static -static-libgcc -static-libstdc++
    )
endif()
