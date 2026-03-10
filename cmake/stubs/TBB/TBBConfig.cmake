# Stub TBBConfig.cmake for NVIDIA pre-built USD
# Creates TBB::tbb target pointing to USD-bundled TBB libraries.

if(NOT TARGET TBB::tbb)
    add_library(TBB::tbb SHARED IMPORTED)
    set_target_properties(TBB::tbb PROPERTIES
        IMPORTED_IMPLIB "$ENV{USD_ROOT}/lib/tbb.lib"
        IMPORTED_LOCATION "$ENV{USD_ROOT}/bin/tbb.dll"
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{USD_ROOT}/include"
    )
endif()

set(TBB_FOUND TRUE)
set(TBB_VERSION "2020.3")
