add_library(TweakopediaPackage STATIC
    PackageFooter.cpp
    PackageFooter.h
    PackageTypes.h
    Sha256.cpp
    Sha256.h
)

target_include_directories(TweakopediaPackage PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(TweakopediaPackage PRIVATE bcrypt)
