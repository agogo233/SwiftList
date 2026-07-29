cmake_minimum_required(VERSION 3.20)

# C++ port of SwiftList — Phase 1: Foundation
# Windows-only build (Win32 API). Static CRT (/MT) for zero runtime dependency.
project(swiftlist-cxx
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "SwiftList C++ port: high-performance NTFS file search"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT: no VC++ Redistributable dependency
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
    add_compile_options(/W4 /WX)
    add_compile_options($<$<CONFIG:Release>:/GL>)
    add_link_options($<$<CONFIG:Release>:/LTCG>)
endif()

# Build library by default
add_subdirectory(src)

# Tests (on by default if this is the top-level project)
if(BUILD_TESTING OR (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME))
    enable_testing()
    add_subdirectory(tests)
endif()
