if(WIN32)

add_library(app_lib STATIC)

target_sources(app_lib
    PRIVATE
        pipe_client_wrapper.cpp
        search_view_model.cpp
        quick_search_window.cpp
        app.cpp
        action_menu_builder.cpp
        app_config.cpp
    PUBLIC
        pipe_client_wrapper.h
        search_view_model.h
        quick_search_window.h
        app.h
        action_menu_builder.h
        app_config.h
)

target_include_directories(app_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(app_lib PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(app_lib PUBLIC ui pipe search engine Shcore.lib)

if(MSVC)
    target_compile_options(app_lib PRIVATE /W4 /WX-)
endif()

add_executable(app_exe)
target_sources(app_exe PRIVATE main.cpp)
target_link_libraries(app_exe PRIVATE app_lib)
set_target_properties(app_exe PROPERTIES OUTPUT_NAME "SwiftListApp")

endif()
