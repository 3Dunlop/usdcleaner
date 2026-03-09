set(USDCLEANER_CORE_SOURCES
    common/Metrics.cpp
    common/SpatialHash.cpp
    common/HashUtils.cpp
    common/UsdUtils.cpp
    welding/VertexWelder.cpp
    welding/PrimvarRemapper.cpp
    topology/DegenerateFaceRemover.cpp
    topology/LaminaFaceRemover.cpp
    materials/MaterialHasher.cpp
    materials/MaterialDeduplicator.cpp
    cache/GpuCacheOptimizer.cpp
    instancing/GeometryHasher.cpp
    instancing/PointInstancerAuthor.cpp
    hierarchy/HierarchyFlattener.cpp
    pipeline/Pipeline.cpp
    pipeline/StageProcessor.cpp
    pipeline/BatchProcessor.cpp
)

set(USDCLEANER_CORE_HEADERS
    common/Types.h
    common/Metrics.h
    common/SpatialHash.h
    common/HashUtils.h
    common/UsdUtils.h
    welding/VertexWelder.h
    welding/PrimvarRemapper.h
    topology/DegenerateFaceRemover.h
    topology/LaminaFaceRemover.h
    materials/MaterialHasher.h
    materials/MaterialDeduplicator.h
    cache/GpuCacheOptimizer.h
    instancing/GeometryHasher.h
    instancing/PointInstancerAuthor.h
    hierarchy/HierarchyFlattener.h
    pipeline/OptimizationPass.h
    pipeline/Pipeline.h
    pipeline/StageProcessor.h
    pipeline/BatchProcessor.h
)

add_library(usdcleaner_core SHARED
    ${USDCLEANER_CORE_SOURCES}
    ${USDCLEANER_CORE_HEADERS}
)

target_include_directories(usdcleaner_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

# USD linkage
if(pxr_FOUND)
    target_link_libraries(usdcleaner_core PUBLIC
        usd
        sdf
        tf
        gf
        vt
        ar
        usdGeom
        usdShade
        usdUtils
    )
elseif(USD_FOUND)
    target_include_directories(usdcleaner_core PUBLIC ${USD_INCLUDE_DIRS})
    target_link_libraries(usdcleaner_core PUBLIC ${USD_LIBRARIES})
endif()

# meshoptimizer (optional)
if(meshoptimizer_FOUND)
    target_link_libraries(usdcleaner_core PRIVATE meshoptimizer::meshoptimizer)
    target_compile_definitions(usdcleaner_core PRIVATE USDCLEANER_HAS_MESHOPTIMIZER)
endif()

# TBB (optional, for batch processing)
if(TBB_FOUND)
    target_link_libraries(usdcleaner_core PRIVATE TBB::tbb)
    target_compile_definitions(usdcleaner_core PRIVATE USDCLEANER_HAS_TBB)
endif()

# DLL export macros
target_compile_definitions(usdcleaner_core PRIVATE USDCLEANER_CORE_EXPORTS)

usdcleaner_set_warnings(usdcleaner_core)
