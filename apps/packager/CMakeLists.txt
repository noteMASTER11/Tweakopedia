add_executable(TweakopediaPackager main.cpp)
set_target_properties(TweakopediaPackager PROPERTIES OUTPUT_NAME "Tweakopedia.Packager")
target_link_libraries(TweakopediaPackager PRIVATE
    TweakopediaPackage nlohmann_json::nlohmann_json
)
if(MINGW)
    target_link_options(TweakopediaPackager PRIVATE -municode)
endif()
