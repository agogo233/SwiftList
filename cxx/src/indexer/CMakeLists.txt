add_library(indexer STATIC)

target_sources(indexer
    PRIVATE
        mft_parser.cpp
        usn_record_parser.cpp
        volume_helper.cpp
        journal_reader.cpp
        mft_index_scanner.cpp
        usn_monitor.cpp
    PUBLIC
        mft_parser.h
        usn_record_parser.h
        volume_helper.h
        journal_reader.h
        file_record_store.h
        mft_index_scanner.h
        usn_monitor.h
)

target_include_directories(indexer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(indexer PUBLIC engine index_v2)

if(MSVC)
    target_compile_options(indexer PRIVATE /W4 /WX-)
endif()
