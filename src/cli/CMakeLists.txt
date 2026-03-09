add_executable(usdcleaner_cli main.cpp)

target_link_libraries(usdcleaner_cli PRIVATE usdcleaner_core)

if(CLI11_FOUND)
    target_link_libraries(usdcleaner_cli PRIVATE CLI11::CLI11)
    target_compile_definitions(usdcleaner_cli PRIVATE USDCLEANER_HAS_CLI11)
endif()

set_target_properties(usdcleaner_cli PROPERTIES
    OUTPUT_NAME usdcleaner
)

usdcleaner_set_warnings(usdcleaner_cli)
