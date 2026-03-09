# Centralized dependency discovery

# OpenUSD -- try CONFIG mode first (vcpkg or pre-built), then fallback
find_package(pxr CONFIG QUIET)
if(NOT pxr_FOUND)
    # Fallback: try our custom FindUSD module
    find_package(USD QUIET)
    if(NOT USD_FOUND)
        message(WARNING
            "OpenUSD not found. Set pxr_DIR or USD_ROOT to your USD installation.\n"
            "  Option 1: vcpkg install usd\n"
            "  Option 2: Download NVIDIA pre-built from https://developer.nvidia.com/usd\n"
            "  Option 3: Build from source via build_usd.py"
        )
    endif()
endif()

# meshoptimizer
find_package(meshoptimizer CONFIG QUIET)
if(NOT meshoptimizer_FOUND)
    message(STATUS "meshoptimizer not found -- vertex cache optimization will be disabled")
endif()

# Google Test
find_package(GTest CONFIG QUIET)

# CLI11
find_package(CLI11 CONFIG QUIET)

# Intel TBB (usually comes with USD, but search independently too)
find_package(TBB CONFIG QUIET)
