# FindFBXSDK.cmake - Find Autodesk FBX SDK
#
# Searches for the FBX SDK and creates an imported target.
#
# Input variables:
#   FBXSDK_ROOT - Root directory of FBX SDK installation
#
# Output variables:
#   FBXSDK_FOUND        - True if FBX SDK was found
#   FBXSDK_INCLUDE_DIRS - Include directories
#   FBXSDK_LIBRARIES    - Libraries to link
#
# Imported targets:
#   FBXSDK::FBXSDK - The FBX SDK library

# Search paths
set(_FBXSDK_SEARCH_PATHS
    "${FBXSDK_ROOT}"
    "$ENV{FBXSDK_ROOT}"
    "C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.9"
    "C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.7"
    "C:/Program Files/Autodesk/FBX/FBX SDK/2020.3"
    "C:/Program Files/Autodesk/FBX/FBX SDK/2020.2"
    "D:/FbxSdk"
)

# Find include directory
find_path(FBXSDK_INCLUDE_DIR
    NAMES fbxsdk.h
    PATHS ${_FBXSDK_SEARCH_PATHS}
    PATH_SUFFIXES include
)

# Find library - prefer static /MD build (libfbxsdk-md.lib)
# This links FBX SDK statically but uses dynamic CRT, matching our build config.
find_library(FBXSDK_LIBRARY_RELEASE
    NAMES libfbxsdk-md libfbxsdk
    PATHS ${_FBXSDK_SEARCH_PATHS}
    PATH_SUFFIXES lib/x64/release
)

find_library(FBXSDK_LIBRARY_DEBUG
    NAMES libfbxsdk-md libfbxsdk
    PATHS ${_FBXSDK_SEARCH_PATHS}
    PATH_SUFFIXES lib/x64/debug
)

# Also find the companion static libs that libfbxsdk-md depends on
find_library(FBXSDK_XML2_RELEASE
    NAMES libxml2-md libxml2
    PATHS ${_FBXSDK_SEARCH_PATHS}
    PATH_SUFFIXES lib/x64/release
)

find_library(FBXSDK_ZLIB_RELEASE
    NAMES zlib-md zlib
    PATHS ${_FBXSDK_SEARCH_PATHS}
    PATH_SUFFIXES lib/x64/release
)

find_library(FBXSDK_XML2_DEBUG
    NAMES libxml2-md libxml2
    PATHS ${_FBXSDK_SEARCH_PATHS}
    PATH_SUFFIXES lib/x64/debug
)

find_library(FBXSDK_ZLIB_DEBUG
    NAMES zlib-md zlib
    PATHS ${_FBXSDK_SEARCH_PATHS}
    PATH_SUFFIXES lib/x64/debug
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FBXSDK
    REQUIRED_VARS FBXSDK_INCLUDE_DIR FBXSDK_LIBRARY_RELEASE
)

if(FBXSDK_FOUND)
    set(FBXSDK_INCLUDE_DIRS "${FBXSDK_INCLUDE_DIR}")

    if(NOT TARGET FBXSDK::FBXSDK)
        add_library(FBXSDK::FBXSDK STATIC IMPORTED)
        set_target_properties(FBXSDK::FBXSDK PROPERTIES
            IMPORTED_LOCATION "${FBXSDK_LIBRARY_RELEASE}"
            INTERFACE_INCLUDE_DIRECTORIES "${FBXSDK_INCLUDE_DIR}"
        )

        # Add debug config if available
        if(FBXSDK_LIBRARY_DEBUG)
            set_target_properties(FBXSDK::FBXSDK PROPERTIES
                IMPORTED_LOCATION_DEBUG "${FBXSDK_LIBRARY_DEBUG}"
            )
        endif()

        # Link companion static libs (libxml2, zlib) and Windows system libs
        set(_FBXSDK_EXTRA_LIBS "")
        if(FBXSDK_XML2_RELEASE)
            list(APPEND _FBXSDK_EXTRA_LIBS "${FBXSDK_XML2_RELEASE}")
        endif()
        if(FBXSDK_ZLIB_RELEASE)
            list(APPEND _FBXSDK_EXTRA_LIBS "${FBXSDK_ZLIB_RELEASE}")
        endif()

        # FBX SDK static link requires these Windows libs
        if(WIN32)
            list(APPEND _FBXSDK_EXTRA_LIBS advapi32 wininet)
        endif()

        set_target_properties(FBXSDK::FBXSDK PROPERTIES
            INTERFACE_LINK_LIBRARIES "${_FBXSDK_EXTRA_LIBS}"
        )

        # For static linkage, do NOT define FBXSDK_SHARED.
        # The FBX SDK uses #if defined(FBXSDK_SHARED) to add __declspec(dllimport),
        # so leaving it undefined ensures no dllimport decorations on symbols.
    endif()

    message(STATUS "Found FBX SDK: ${FBXSDK_INCLUDE_DIR}")
    message(STATUS "  Library: ${FBXSDK_LIBRARY_RELEASE}")
endif()
