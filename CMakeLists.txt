cmake_minimum_required(VERSION 3.15)
project(Filemon VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)

find_package(FLTK CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)

add_subdirectory(./external/fleet)

set(FILEMON_SOURCES
    src/main.cpp
    src/Filemon.hpp
    src/Filemon.cpp
    src/File.hpp
    src/File.cpp
    src/Util.hpp
    src/Util.cpp
    src/Preferences.hpp
    src/Preferences.cpp
)

add_executable(Filemon ${FILEMON_SOURCES})
target_link_libraries(Filemon PRIVATE fltk Fleet PkgConfig::GIO)
