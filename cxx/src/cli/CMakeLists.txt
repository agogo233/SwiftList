if(WIN32)

add_library(cli STATIC)

target_sources(cli
    PRIVATE
        terminal.cpp
        app_search_pipe_client.cpp
        search_session.cpp
        renderer.cpp
    PUBLIC
        terminal.h
        app_search_pipe_client.h
        search_session.h
        renderer.h
)

target_include_directories(cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(cli PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(cli PUBLIC pipe)

if(MSVC)
    target_compile_options(cli PRIVATE /W4 /WX-)
endif()

add_executable(slf_exe)
target_sources(slf_exe PRIVATE main.cpp)
target_link_libraries(slf_exe PRIVATE cli)
set_target_properties(slf_exe PROPERTIES OUTPUT_NAME "slf")

endif()
