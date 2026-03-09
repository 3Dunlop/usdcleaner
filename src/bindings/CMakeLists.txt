# Python bindings via boost::python (matching USD's binding system)

# Find Python
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Find Boost.Python
find_package(Boost REQUIRED COMPONENTS python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR})

add_library(usdcleaner_python SHARED
    Module.cpp
    PipelineBindings.cpp
    PassBindings.cpp
    MetricsBindings.cpp
)

# Name the output as a Python module (no 'lib' prefix, .pyd extension on Windows)
set_target_properties(usdcleaner_python PROPERTIES
    PREFIX ""
    OUTPUT_NAME "_usdcleaner"
)
if(WIN32)
    set_target_properties(usdcleaner_python PROPERTIES SUFFIX ".pyd")
endif()

target_link_libraries(usdcleaner_python PRIVATE
    usdcleaner_core
    Python3::Python
    Boost::python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}
)

target_include_directories(usdcleaner_python PRIVATE
    ${Python3_INCLUDE_DIRS}
)

usdcleaner_set_warnings(usdcleaner_python)
