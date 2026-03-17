# Build Remedy's usdFBX plugin against our USD + FBX SDK setup
#
# This compiles the usdFBX sources from external/usdFBX/src/ into a USD
# SdfFileFormat plugin that enables UsdStage::Open("file.fbx").

set(USDFBX_SRC_DIR "${CMAKE_SOURCE_DIR}/external/usdFBX/src")

set(USDFBX_SOURCES
    ${USDFBX_SRC_DIR}/DebugCodes.cpp
    ${USDFBX_SRC_DIR}/Error.cpp
    ${USDFBX_SRC_DIR}/FbxNodeReader.cpp
    ${USDFBX_SRC_DIR}/Tokens.cpp
    ${USDFBX_SRC_DIR}/UsdFbxAbstractData.cpp
    ${USDFBX_SRC_DIR}/UsdFbxDataReader.cpp
    ${USDFBX_SRC_DIR}/UsdFbxFileformat.cpp
)

# On Windows (case-insensitive FS), the #include "VERSION" in
# UsdFbxFileformat.h conflicts with C++17's <version> standard header.
# Solution: use our patched UsdFbxFileformat.h (in src/plugin/patches/)
# which hardcodes the version string instead of including VERSION.
set(USDFBX_PATCHES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/patches")

# Plugin output directory - follows USD plugin directory structure
set(USDFBX_PLUGIN_DIR "${CMAKE_BINARY_DIR}/plugin/usd/usdFbx")

add_library(usdFbx MODULE ${USDFBX_SOURCES})

set_target_properties(usdFbx PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${USDFBX_PLUGIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${USDFBX_PLUGIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${USDFBX_PLUGIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${USDFBX_PLUGIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${USDFBX_PLUGIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${USDFBX_PLUGIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${USDFBX_PLUGIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${USDFBX_PLUGIN_DIR}"
)

# Patches dir MUST come before source dir so our patched UsdFbxFileformat.h
# (without #include "VERSION") is found first, avoiding the C++17 <version>
# header conflict on Windows case-insensitive filesystem.
target_include_directories(usdFbx PRIVATE
    ${USDFBX_PATCHES_DIR}
    ${USDFBX_SRC_DIR}
)

target_compile_definitions(usdFbx PRIVATE
    USDFBX_EXPORTS
    # Do NOT define FBXSDK_SHARED for static linkage - the FBX SDK uses
    # #if defined(FBXSDK_SHARED) to control __declspec(dllimport)
)

if(WIN32)
    # /Zc:inline- prevents MSVC from optimizing away the SdfFileFormat
    # registration callback, which is critical for the plugin to load.
    # /permissive is needed for the old USD-bundled boost headers.
    target_compile_options(usdFbx PRIVATE /Zc:inline- /EHsc /permissive)
    # Suppress warnings from third-party headers
    target_compile_options(usdFbx PRIVATE /W0)
endif()

# FBX SDK (static)
target_link_libraries(usdFbx PRIVATE FBXSDK::FBXSDK)

# USD libraries
if(pxr_FOUND)
    target_link_libraries(usdFbx PRIVATE
        sdf
        tf
        gf
        vt
        ar
        usd
        usdGeom
        usdShade
        usdSkel
        usdUtils
        plug
    )
elseif(USD_FOUND)
    target_include_directories(usdFbx PRIVATE ${USD_INCLUDE_DIRS})
    target_link_libraries(usdFbx PRIVATE ${USD_LIBRARIES})
endif()

# Precompiled headers
target_precompile_headers(usdFbx PRIVATE
    ${USDFBX_SRC_DIR}/PrecompiledHeader.h
)

# Generate plugInfo.json from template
set(PLUG_INFO_LIBRARY_PATH "usdFbx${CMAKE_SHARED_MODULE_SUFFIX}")
set(PLUG_INFO_RESOURCE_PATH "resources")
set(PLUG_INFO_ROOT "..")

configure_file(
    "${USDFBX_SRC_DIR}/plugInfo.json"
    "${USDFBX_PLUGIN_DIR}/resources/plugInfo.json"
    @ONLY
)
