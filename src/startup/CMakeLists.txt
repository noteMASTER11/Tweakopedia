add_library(TweakopediaStartup STATIC
    StartupIpc.cpp
    StartupIpc.h
    StartupStage.cpp
    StartupStage.h
)

target_include_directories(TweakopediaStartup PUBLIC ${PROJECT_SOURCE_DIR}/src)
