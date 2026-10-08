add_executable(TweakopediaExecutor WIN32 main.cpp)
set_target_properties(TweakopediaExecutor PROPERTIES OUTPUT_NAME "Tweakopedia.Executor")

target_link_libraries(TweakopediaExecutor PRIVATE
    Qt6::Core
    TweakopediaExecution
    TweakopediaPersistence
    TweakopediaPlatform
)

if(BUILD_TESTING)
    target_compile_definitions(TweakopediaExecutor PRIVATE TWEAKOPEDIA_TESTING)
endif()
