include(FetchContent)

FetchContent_Declare(
    tweakopedia_miniz
    URL https://github.com/richgel999/miniz/archive/refs/tags/3.1.2.zip
    URL_HASH SHA256=F0446D863F9C19926AD9483C523FDC42E42B8D4A6A431D27E09D49C79A140D9A
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
    tweakopedia_nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip
    URL_HASH SHA256=B8CB0EF2DD7F57F18933997C9934BB1FA962594F701CD5A8D3C2C80541559372
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

function(tweakopedia_prepare_bootstrap_dependencies)
    if(TARGET TweakopediaMiniz AND TARGET nlohmann_json::nlohmann_json)
        return()
    endif()

    cmake_policy(PUSH)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_GetProperties(tweakopedia_miniz)
    if(NOT tweakopedia_miniz_POPULATED)
        FetchContent_Populate(tweakopedia_miniz)
    endif()
    FetchContent_GetProperties(tweakopedia_nlohmann_json)
    if(NOT tweakopedia_nlohmann_json_POPULATED)
        FetchContent_Populate(tweakopedia_nlohmann_json)
    endif()
    cmake_policy(POP)

    add_library(TweakopediaMiniz STATIC
        ${tweakopedia_miniz_SOURCE_DIR}/miniz.c
        ${tweakopedia_miniz_SOURCE_DIR}/miniz_tdef.c
        ${tweakopedia_miniz_SOURCE_DIR}/miniz_tinfl.c
        ${tweakopedia_miniz_SOURCE_DIR}/miniz_zip.c
    )
    target_include_directories(TweakopediaMiniz PUBLIC ${tweakopedia_miniz_SOURCE_DIR})

    add_library(TweakopediaNlohmannJson INTERFACE)
    add_library(nlohmann_json::nlohmann_json ALIAS TweakopediaNlohmannJson)
    target_include_directories(TweakopediaNlohmannJson INTERFACE
        ${tweakopedia_nlohmann_json_SOURCE_DIR}/include
    )
endfunction()
