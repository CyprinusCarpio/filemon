cmake_minimum_required(VERSION 3.15)
project(Filemon VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)

find_package(FLTK CONFIG REQUIRED)

add_subdirectory(./external/fleet)

target_include_directories(Fleet PUBLIC external/xdgmime/src)

set(XDGMIME_SOURCES
    external/xdgmime/src/xdgmime.c
    external/xdgmime/src/xdgmimeglob.c
    external/xdgmime/src/xdgmimeint.c
    external/xdgmime/src/xdgmimemagic.c
    external/xdgmime/src/xdgmimealias.c
    external/xdgmime/src/xdgmimeparent.c
    external/xdgmime/src/xdgmimecache.c
    external/xdgmime/src/xdgmimeicon.c
)

set(FILEMON_SOURCES
    src/main.cpp
    src/Filemon.hpp
    src/Filemon.cpp
    src/File.hpp
    src/File.cpp
    src/Util.hpp
    src/Util.cpp
)

add_compile_options(-DHAVE_MMAP) # Needed by xdgmime
add_executable(Filemon ${FILEMON_SOURCES} ${XDGMIME_SOURCES})
target_link_libraries(Filemon PRIVATE fltk Fleet)
