add_library(TweakopediaPackage STATIC
    PackageFooter.cpp
    PackageFooter.h
    PackageTypes.h
    PayloadManifest.cpp
    PayloadManifest.h
    Sha256.cpp
    Sha256.h
    ZipPayload.cpp
    ZipPayload.h
)

tweakopedia_prepare_bootstrap_dependencies()
target_include_directories(TweakopediaPackage PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(TweakopediaPackage
    PRIVATE bcrypt nlohmann_json::nlohmann_json TweakopediaMiniz
)
