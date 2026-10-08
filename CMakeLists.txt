cmake_minimum_required(VERSION 3.30)

project(Tweakopedia LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(cmake/Dependencies.cmake)

find_package(Qt6 6.8.3 REQUIRED COMPONENTS Core Test)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_subdirectory(src/domain)
add_subdirectory(src/content)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
