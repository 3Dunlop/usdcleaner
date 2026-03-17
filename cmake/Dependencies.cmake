# Centralized dependency discovery
#
# Strategy for NVIDIA pre-built USD:
# pxrConfig.cmake calls find_dependency(TBB), find_dependency(OpenSubdiv),
# find_dependency(MaterialX), find_dependency(Imath). These ship in the USD
# installation but mostly without CMake config files. We provide stub configs
# in cmake/stubs/ and add them to CMAKE_PREFIX_PATH before find_package(pxr).

# ---- USD_ROOT setup ----
if(DEFINED ENV{USD_ROOT} AND NOT DEFINED USD_ROOT)
    set(USD_ROOT "$ENV{USD_ROOT}")
endif()

if(USD_ROOT)
    message(STATUS "USD_ROOT: ${USD_ROOT}")

    # Add stub config directories so pxrConfig.cmake's find_dependency calls succeed
    list(PREPEND CMAKE_PREFIX_PATH
        "${CMAKE_CURRENT_LIST_DIR}/stubs/TBB"
        "${CMAKE_CURRENT_LIST_DIR}/stubs/OpenSubdiv"
        "${CMAKE_CURRENT_LIST_DIR}/stubs/Imath"
        "${USD_ROOT}/lib/cmake"            # MaterialX ships its own config here
        "${USD_ROOT}"
    )

    # Python3 -- USD 25.08 was built with Python 3.12
    # Point to USD-bundled Python to satisfy pxrConfig's Python3 find_dependency
    # and avoid linking against system Python (which may be a different version)
    if(EXISTS "${USD_ROOT}/python/python.exe")
        set(Python3_EXECUTABLE "${USD_ROOT}/python/python.exe" CACHE FILEPATH "USD-bundled Python" FORCE)
        set(Python3_INCLUDE_DIR "${USD_ROOT}/python/include" CACHE PATH "USD-bundled Python include" FORCE)
        set(Python3_LIBRARY "${USD_ROOT}/python/libs/python312.lib" CACHE FILEPATH "USD-bundled Python lib" FORCE)
    endif()
endif()

# ---- OpenUSD ----
if(USD_ROOT)
    set(pxr_DIR "${USD_ROOT}" CACHE PATH "Path to pxrConfig.cmake" FORCE)
endif()

find_package(pxr CONFIG QUIET)

if(pxr_FOUND)
    message(STATUS "Found OpenUSD ${PXR_VERSION} at ${pxr_DIR}")
else()
    message(WARNING
        "OpenUSD not found. Set USD_ROOT to your USD installation.\n"
        "  Download NVIDIA pre-built from https://developer.nvidia.com/usd"
    )
endif()

# ---- FBX SDK (optional, for usdFBX plugin) ----
if(USDCLEANER_BUILD_FBX_PLUGIN)
    include(FindFBXSDK)
endif()

# ---- vcpkg dependencies ----
find_package(meshoptimizer CONFIG QUIET)
if(meshoptimizer_FOUND)
    message(STATUS "Found meshoptimizer")
else()
    message(STATUS "meshoptimizer not found -- vertex cache optimization will be disabled")
endif()

find_package(GTest CONFIG QUIET)
find_package(CLI11 CONFIG QUIET)
