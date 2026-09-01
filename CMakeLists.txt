cmake_minimum_required(VERSION 3.16)
project(Mirror LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(UXPLAY_AS_LIBRARY ON CACHE BOOL "Build UxPlay as a library" FORCE)

add_subdirectory(third_party/uxplay)

add_executable(Mirror WIN32
    app/main.cpp
    app/window.cpp
    app/receiver.cpp
    app/firewall.cpp
    app/icon.cpp
    app/mirror.rc
)
set_source_files_properties(app/mirror.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/mirror.ico"
    COMPILE_FLAGS "-I\"${CMAKE_SOURCE_DIR}/assets\""
)

target_compile_definitions(Mirror PRIVATE UNICODE _UNICODE)
target_include_directories(Mirror PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/third_party/uxplay
    ${CMAKE_SOURCE_DIR}/third_party/uxplay/renderers
)
target_link_libraries(Mirror PRIVATE
    uxplay_lib
    user32
    gdi32
    msimg32
    dwmapi
    d2d1
    dwrite
    windowscodecs
    ole32
)
