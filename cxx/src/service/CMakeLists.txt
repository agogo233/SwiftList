if(WIN32)

add_library(service_lib STATIC)

target_sources(service_lib
    PRIVATE
        usn_service.cpp
        service_installer.cpp
        hook_mode_launcher.cpp
    PUBLIC
        usn_service.h
        service_installer.h
        hook_mode_launcher.h
)

target_include_directories(service_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(service_lib PUBLIC pipe)

target_compile_definitions(service_lib PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
    target_compile_options(service_lib PRIVATE /W4 /WX-)
endif()

add_executable(service_exe)
target_sources(service_exe PRIVATE main.cpp)
target_link_libraries(service_exe PRIVATE service_lib)
set_target_properties(service_exe PROPERTIES OUTPUT_NAME "SwiftListService")

endif()
