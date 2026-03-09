#include "core/welding/VertexWelder.h"
#include "core/welding/PrimvarRemapper.h"
#include "core/common/UsdUtils.h"

#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/usd/primRange.h>

#include <iostream>

namespace usdcleaner {

VertexWelder::VertexWelder() = default;

void VertexWelder::Execute(const UsdStageRefPtr& stage) {
    float epsilon = epsilon_;

    if (autoEpsilon_) {
        epsilon = ComputeAutoEpsilon(stage);
        std::cout << "[VertexWelding] Auto-detected epsilon: " << epsilon << "\n";
    }

    // Process all meshes
    SdfChangeBlock block;

    ForEachMesh(stage, [&](UsdGeomMesh& mesh) {
        WeldMesh(mesh, epsilon);
    });
}

void VertexWelder::WeldMesh(UsdGeomMesh& mesh, float epsilon) {
    VtVec3fArray points;
    VtIntArray faceVertexIndices;
    VtIntArray faceVertexCounts;

    mesh.GetPointsAttr().Get(&points);
    mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);

    if (points.empty() || faceVertexIndices.empty()) return;

    // Build remap table using spatial hashing
    float cellSize = epsilon * 2.0f;
    SpatialHash hash(cellSize);
    RemapTable remap = hash.GenerateRemapTable(points, epsilon);

    // Check if any vertices were actually welded
    bool anyWelded = false;
    for (size_t i = 0; i < remap.size(); ++i) {
        if (remap[i] != static_cast<uint32_t>(i)) {
            anyWelded = true;
            break;
        }
    }

    if (!anyWelded) return;

    // Remap face vertex indices
    for (size_t i = 0; i < faceVertexIndices.size(); ++i) {
        int oldIdx = faceVertexIndices[i];
        if (oldIdx >= 0 && static_cast<size_t>(oldIdx) < remap.size()) {
            faceVertexIndices[i] = static_cast<int>(remap[oldIdx]);
        }
    }

    // Compact the points array
    VtVec3fArray compactedPoints = SpatialHash::CompactPoints(points, remap);

    // Remap vertex-interpolated primvars
    PrimvarRemapper::RemapPrimvars(mesh, remap, compactedPoints.size());

    // Write updated data
    mesh.GetPointsAttr().Set(compactedPoints);
    mesh.GetFaceVertexIndicesAttr().Set(faceVertexIndices);

    // Compact unreferenced vertices (safety pass)
    // This handles edge cases where the remap may have gaps
    VtVec3fArray finalPoints = compactedPoints;
    VtIntArray finalIndices = faceVertexIndices;
    CompactUnreferencedVertices(finalPoints, finalIndices);

    if (finalPoints.size() != compactedPoints.size()) {
        mesh.GetPointsAttr().Set(finalPoints);
        mesh.GetFaceVertexIndicesAttr().Set(finalIndices);
    }
}

} // namespace usdcleaner
