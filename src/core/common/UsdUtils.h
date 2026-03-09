#pragma once

#include "core/common/Types.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

#include <functional>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

// Mesh data extracted for processing
struct USDCLEANER_API MeshData {
    SdfPath primPath;
    VtVec3fArray points;
    VtIntArray faceVertexCounts;
    VtIntArray faceVertexIndices;
};

// Traverse all UsdGeomMesh prims in a stage and invoke a callback
USDCLEANER_API void ForEachMesh(
    const UsdStageRefPtr& stage,
    const std::function<void(UsdGeomMesh&)>& fn);

// Extract mesh data arrays from a UsdGeomMesh
USDCLEANER_API MeshData ExtractMeshData(const UsdGeomMesh& mesh);

// Write mesh data arrays back to a UsdGeomMesh
USDCLEANER_API void WriteMeshData(UsdGeomMesh& mesh, const MeshData& data);

// Compute the axis-aligned bounding box diagonal of the entire stage
USDCLEANER_API float ComputeStageDiagonal(const UsdStageRefPtr& stage);

// Determine a reasonable welding epsilon based on stage metadata
USDCLEANER_API float ComputeAutoEpsilon(const UsdStageRefPtr& stage);

// Remove faces from mesh data, updating faceVertexCounts, faceVertexIndices,
// and all faceVarying primvars. facesToKeep is a boolean mask per face.
USDCLEANER_API void RemoveFaces(
    UsdGeomMesh& mesh,
    const std::vector<bool>& facesToKeep);

// Compact unreferenced vertices from points array and update indices
USDCLEANER_API void CompactUnreferencedVertices(
    VtVec3fArray& points,
    VtIntArray& faceVertexIndices);

} // namespace usdcleaner
