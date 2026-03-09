# FindUSD.cmake -- Fallback find module for pre-built USD installations
#
# This module looks for OpenUSD in common installation locations and via
# the USD_ROOT environment variable. It is used when find_package(pxr CONFIG)
# fails (e.g., NVIDIA pre-built binaries that don't ship a pxrConfig.cmake).
#
# Sets:
#   USD_FOUND
#   USD_INCLUDE_DIRS
#   USD_LIBRARY_DIR
#   USD_LIBRARIES (core libraries: usd, sdf, tf, gf, vt, usdGeom, usdShade, ar)

set(_USD_SEARCH_PATHS
    "$ENV{USD_ROOT}"
    "$ENV{PXR_USD_LOCATION}"
    "C:/USD"
    "C:/Program Files/USD"
)

find_path(USD_INCLUDE_DIR
    NAMES pxr/usd/usd/stage.h
    PATHS ${_USD_SEARCH_PATHS}
    PATH_SUFFIXES include
)

find_library(USD_CORE_LIBRARY
    NAMES usd_usd
    PATHS ${_USD_SEARCH_PATHS}
    PATH_SUFFIXES lib
)

if(USD_INCLUDE_DIR AND USD_CORE_LIBRARY)
    set(USD_FOUND TRUE)
    get_filename_component(USD_LIBRARY_DIR "${USD_CORE_LIBRARY}" DIRECTORY)
    set(USD_INCLUDE_DIRS "${USD_INCLUDE_DIR}")

    # Find all required USD libraries
    set(_USD_REQUIRED_LIBS
        usd_usd usd_sdf usd_tf usd_gf usd_vt usd_ar
        usd_usdGeom usd_usdShade usd_usdUtils
    )

    set(USD_LIBRARIES "")
    foreach(_lib ${_USD_REQUIRED_LIBS})
        find_library(_USD_LIB_${_lib}
            NAMES ${_lib}
            PATHS ${USD_LIBRARY_DIR}
            NO_DEFAULT_PATH
        )
        if(_USD_LIB_${_lib})
            list(APPEND USD_LIBRARIES "${_USD_LIB_${_lib}}")
        endif()
    endforeach()

    message(STATUS "Found USD: ${USD_INCLUDE_DIR}")
    message(STATUS "USD libraries: ${USD_LIBRARY_DIR}")
else()
    set(USD_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(USD
    REQUIRED_VARS USD_INCLUDE_DIR USD_CORE_LIBRARY
)
