cmake_minimum_required(VERSION 3.30)

project(Tweakopedia VERSION 0.9.2 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(cmake/Dependencies.cmake)
include(cmake/BootstrapDependencies.cmake)

find_package(Qt6 6.8.3 REQUIRED COMPONENTS Concurrent Core Gui Network Qml Quick QuickControls2 QuickTest Sql Test)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_subdirectory(src/domain)
add_subdirectory(src/content)
add_subdirectory(src/platform)
add_subdirectory(src/detection)
add_subdirectory(src/planning)
add_subdirectory(src/persistence)
add_subdirectory(src/execution)
add_subdirectory(src/app)
add_subdirectory(src/package)
add_subdirectory(apps/bootstrap)
add_subdirectory(apps/packager)
add_subdirectory(apps/executor)
add_subdirectory(apps/tweakopedia)

install(TARGETS Tweakopedia TweakopediaExecutor RUNTIME DESTINATION .)
install(DIRECTORY content/ DESTINATION content)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
