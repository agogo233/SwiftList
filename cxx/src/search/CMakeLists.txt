add_library(search STATIC)

target_sources(search
    PRIVATE
        fzf_scoring.cpp
        fzf_pattern.cpp
        fzf_matcher.cpp
        search_matcher.cpp
        highlight_mask.cpp
    PUBLIC
        fzf_scoring.h
        fzf_pattern.h
        fzf_matcher.h
        fzf_top_n.h
        search_matcher.h
        highlight_mask.h
)

target_include_directories(search
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(search PUBLIC engine index_v2)

if(MSVC)
    target_compile_options(search PRIVATE /W4 /WX-)
endif()
