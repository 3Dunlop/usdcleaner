#include "core/cache/GpuCacheOptimizer.h"
#include "core/common/UsdUtils.h"

#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/usdGeom/mesh.h>

#ifdef USDCLEANER_HAS_MESHOPTIMIZER
#include <meshoptimizer.h>
#endif

#include <iostream>
#include <vector>

namespace usdcleaner {

void GpuCacheOptimizer::Execute(const UsdStageRefPtr& stage) {
#ifndef USDCLEANER_HAS_MESHOPTIMIZER
    std::cout << "[GpuCacheOptimization] meshoptimizer not available, skipping\n";
    return;
#else
    SdfChangeBlock block;

    ForEachMesh(stage, [&](UsdGeomMesh& mesh) {
        VtVec3fArray points;
        VtIntArray faceVertexIndices, faceVertexCounts;
        mesh.GetPointsAttr().Get(&points);
        mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);
        mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);

        if (points.empty() || faceVertexIndices.empty()) return;

        // Triangulate: convert faceVertexCounts + faceVertexIndices to triangle list
        std::vector<unsigned int> triangleIndices;
        size_t offset = 0;
        for (int count : faceVertexCounts) {
            if (count < 3) {
                offset += count;
                continue;
            }
            // Fan triangulation from first vertex
            for (int j = 1; j < count - 1; ++j) {
                triangleIndices.push_back(
                    static_cast<unsigned int>(faceVertexIndices[offset]));
                triangleIndices.push_back(
                    static_cast<unsigned int>(faceVertexIndices[offset + j]));
                triangleIndices.push_back(
                    static_cast<unsigned int>(faceVertexIndices[offset + j + 1]));
            }
            offset += count;
        }

        if (triangleIndices.empty()) return;

        // Vertex cache optimization
        std::vector<unsigned int> optimizedIndices(triangleIndices.size());
        meshopt_optimizeVertexCache(
            optimizedIndices.data(),
            triangleIndices.data(),
            triangleIndices.size(),
            points.size());

        // Vertex fetch optimization (reorders vertices for better locality)
        std::vector<unsigned int> remapTable(points.size());
        size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(
            remapTable.data(),
            optimizedIndices.data(),
            optimizedIndices.size(),
            points.size());

        // Apply vertex remap
        std::vector<float> remappedVerts(uniqueVertices * 3);
        meshopt_remapVertexBuffer(
            remappedVerts.data(),
            points.data(),
            points.size(),
            sizeof(float) * 3,
            remapTable.data());

        // Remap indices
        meshopt_remapIndexBuffer(
            optimizedIndices.data(),
            optimizedIndices.data(),
            optimizedIndices.size(),
            remapTable.data());

        // Write back as triangulated mesh if requested, or preserve original topology
        if (triangulate_) {
            // Store as triangles
            VtVec3fArray newPoints(uniqueVertices);
            std::memcpy(newPoints.data(), remappedVerts.data(),
                        uniqueVertices * sizeof(float) * 3);

            VtIntArray newIndices(optimizedIndices.size());
            for (size_t i = 0; i < optimizedIndices.size(); ++i) {
                newIndices[i] = static_cast<int>(optimizedIndices[i]);
            }

            VtIntArray newCounts(optimizedIndices.size() / 3, 3);

            mesh.GetPointsAttr().Set(newPoints);
            mesh.GetFaceVertexIndicesAttr().Set(newIndices);
            mesh.GetFaceVertexCountsAttr().Set(newCounts);
        }
        // If not triangulating, we still benefit from the vertex fetch
        // optimization but keep original face topology.
    });
#endif
}

} // namespace usdcleaner
