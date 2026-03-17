cmake_minimum_required(VERSION 3.26)
project(USDCleaner VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for editor tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Windows-specific global defines
if(WIN32)
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(_USE_MATH_DEFINES)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

# Map imported config for pre-built USD (built as RelWithDebInfo).
# Order: try native config first (for vcpkg), then RelWithDebInfo (for USD).
set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE Release RelWithDebInfo "")
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Debug RelWithDebInfo "")
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL MinSizeRel Release RelWithDebInfo "")

# Include helper modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

# Subdirectories
add_subdirectory(src/core)
add_subdirectory(src/cli)

option(USDCLEANER_BUILD_FBX_PLUGIN "Build usdFBX plugin for direct FBX file support (requires FBX SDK)" OFF)
if(USDCLEANER_BUILD_FBX_PLUGIN)
    if(FBXSDK_FOUND)
        add_subdirectory(src/plugin)
    else()
        message(WARNING "USDCLEANER_BUILD_FBX_PLUGIN is ON but FBX SDK was not found. "
                        "Set FBXSDK_ROOT to your FBX SDK installation.")
    endif()
endif()

option(USDCLEANER_BUILD_BINDINGS "Build Python bindings" ON)
if(USDCLEANER_BUILD_BINDINGS)
    add_subdirectory(src/bindings)
endif()

option(USDCLEANER_BUILD_TESTS "Build tests" ON)
if(USDCLEANER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
