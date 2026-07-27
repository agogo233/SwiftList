include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Do NOT set gtest_force_shared_crt: let CMAKE_MSVC_RUNTIME_LIBRARY
# (set at top level to MultiThreaded) propagate to GTest as well.
# Mixing static/dynamic CRT in one binary causes heap corruption.
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(engine_tests)

target_sources(engine_tests
    PRIVATE
        defs_tests.cpp
        utf8_tests.cpp
        arena_tests.cpp
        snapshot_format_tests.cpp
        snapshot_tests.cpp
        fzf_tests.cpp
        indexer_tests.cpp
        usn_parser_tests.cpp
        pipe_tests.cpp
        ui_tests.cpp
        app_tests.cpp
        app_config_tests.cpp
        mft_index_scanner_tests.cpp
        usn_monitor_tests.cpp
        hook_ipc_tests.cpp
)

target_link_libraries(engine_tests
    PRIVATE
        engine
        index_v2
        search
        indexer
        pipe
        ui
        app_lib
        hook_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(engine_tests)

if(MSVC)
    target_compile_options(engine_tests PRIVATE /W4 /WX-)
endif()
