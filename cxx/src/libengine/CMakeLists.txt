add_library(engine STATIC)

target_sources(engine
    PRIVATE
        common/utf8.cpp
        common/arena.cpp
)

target_include_directories(engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Headers are picked up via target_include_directories.
# FILE_SET HEADERS (CMake 3.23+) is avoided for wider CMake compatibility.

if(MSVC)
    target_compile_options(engine PRIVATE /W4 /WX-)
endif()
