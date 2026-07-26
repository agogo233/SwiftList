if(WIN32)

add_library(ui STATIC)

target_sources(ui
    PRIVATE
        direct2d_surface.cpp
        window.cpp
        theme.cpp
        directwrite_text.cpp
        controls.cpp
        list_box.cpp
        scroll_bar.cpp
        text_box.cpp
        icon_cache.cpp
        animation.cpp
    PUBLIC
        direct2d_surface.h
        window.h
        theme.h
        directwrite_text.h
        controls.h
        list_box.h
        scroll_bar.h
        text_box.h
        icon_cache.h
        animation.h
)

target_include_directories(ui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(ui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(ui PUBLIC dxgi d2d1 dwrite windowscodecs
                       shlwapi uxtheme dwmapi)

if(MSVC)
    target_compile_options(ui PRIVATE /W4 /WX-)
endif()

endif()
