if(WIN32)

add_library(pipe STATIC)

target_sources(pipe
    PRIVATE
        pipe_server.cpp
        pipe_client.cpp
        pipe_security.cpp
        search_request_wire.cpp
        search_response_wire.cpp
    PUBLIC
        pipe_server.h
        pipe_client.h
        pipe_security.h
        wire_format.h
        search_request_wire.h
        search_response_wire.h
        hook_ipc_message.h
)

target_include_directories(pipe
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(pipe PUBLIC engine)

target_compile_definitions(pipe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
    target_compile_options(pipe PRIVATE /W4 /WX-)
endif()

endif()
