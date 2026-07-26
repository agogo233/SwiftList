add_library(index_v2 STATIC)

target_sources(index_v2
    PRIVATE
        snapshot_format.cpp
        snapshot.cpp
        snapshot_builder.cpp
        snapshot_writer.cpp
        delta_overlay.cpp
        live_index.cpp
        mapped_file.cpp
    PUBLIC
        snapshot_format.h
        snapshot.h
        snapshot_builder.h
        snapshot_writer.h
        delta_overlay.h
        live_index.h
        mapped_file.h
)

target_include_directories(index_v2
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(index_v2 PUBLIC engine)

if(MSVC)
    target_compile_options(index_v2 PRIVATE /W4 /WX-)
endif()
