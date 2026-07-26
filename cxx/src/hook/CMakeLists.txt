if(WIN32)

add_library(hook_lib STATIC)

target_sources(hook_lib
    PRIVATE
        keyboard_hook.cpp
        mouse_hook.cpp
        explorer_tracker.cpp
        inline_search_manager.cpp
    PUBLIC
        keyboard_hook.h
        mouse_hook.h
        explorer_tracker.h
        inline_search_manager.h
)

target_include_directories(hook_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(hook_lib PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(hook_lib PUBLIC ole32 oleaut32 shell32 wtsapi32)

if(MSVC)
    target_compile_options(hook_lib PRIVATE /W4 /WX-)
endif()

add_executable(hook_exe)
target_sources(hook_exe PRIVATE main.cpp)
target_link_libraries(hook_exe PRIVATE hook_lib)
set_target_properties(hook_exe PROPERTIES OUTPUT_NAME "SwiftListHook")

endif()
