add_executable(usdcleaner_cli main.cpp)

target_link_libraries(usdcleaner_cli PRIVATE usdcleaner_core)

if(CLI11_FOUND)
    # Use CLI11 header-only to avoid ABI mismatch between vcpkg-compiled lib
    # and our MSVC version. We must NOT define CLI11_COMPILE (which the vcpkg
    # target's INTERFACE_COMPILE_DEFINITIONS sets) — that makes the headers
    # emit extern declarations instead of inline definitions.
    get_target_property(_cli11_inc CLI11::CLI11 INTERFACE_INCLUDE_DIRECTORIES)
    if(_cli11_inc)
        target_include_directories(usdcleaner_cli PRIVATE ${_cli11_inc})
    endif()
    target_compile_definitions(usdcleaner_cli PRIVATE USDCLEANER_HAS_CLI11)
endif()

set_target_properties(usdcleaner_cli PROPERTIES
    OUTPUT_NAME usdcleaner
)

usdcleaner_set_warnings(usdcleaner_cli)
