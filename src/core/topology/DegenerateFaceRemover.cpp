#include "core/topology/DegenerateFaceRemover.h"
#include "core/common/UsdUtils.h"

#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>

#include <unordered_set>
#include <iostream>

namespace usdcleaner {

void DegenerateFaceRemover::Execute(const UsdStageRefPtr& stage) {
    SdfChangeBlock block;

    ForEachMesh(stage, [&](UsdGeomMesh& mesh) {
        VtIntArray faceVertexCounts, faceVertexIndices;
        mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
        mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);

        if (faceVertexCounts.empty()) return;

        // Determine which faces to keep
        std::vector<bool> facesToKeep(faceVertexCounts.size(), true);
        size_t offset = 0;
        size_t degenerateCount = 0;

        for (size_t faceIdx = 0; faceIdx < faceVertexCounts.size(); ++faceIdx) {
            int count = faceVertexCounts[faceIdx];

            // Collect unique indices for this face
            std::unordered_set<int> uniqueIndices;
            for (int j = 0; j < count; ++j) {
                uniqueIndices.insert(faceVertexIndices[offset + j]);
            }

            // A valid face needs at least 3 unique vertices
            if (uniqueIndices.size() < 3) {
                facesToKeep[faceIdx] = false;
                degenerateCount++;
            }

            offset += count;
        }

        if (degenerateCount == 0) return;

        std::cout << "[DegenerateFaceRemoval] " << mesh.GetPrim().GetPath()
                  << ": removing " << degenerateCount << " degenerate faces\n";

        RemoveFaces(mesh, facesToKeep);
    });
}

} // namespace usdcleaner
