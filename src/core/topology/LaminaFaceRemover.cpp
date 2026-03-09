#include "core/topology/LaminaFaceRemover.h"
#include "core/common/UsdUtils.h"
#include "core/common/HashUtils.h"

#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>

#include <algorithm>
#include <unordered_set>
#include <iostream>

namespace usdcleaner {

void LaminaFaceRemover::Execute(const UsdStageRefPtr& stage) {
    SdfChangeBlock block;

    ForEachMesh(stage, [&](UsdGeomMesh& mesh) {
        VtIntArray faceVertexCounts, faceVertexIndices;
        mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
        mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);

        if (faceVertexCounts.empty()) return;

        std::vector<bool> facesToKeep(faceVertexCounts.size(), true);
        std::unordered_set<size_t> seenHashes;
        size_t offset = 0;
        size_t duplicateCount = 0;

        for (size_t faceIdx = 0; faceIdx < faceVertexCounts.size(); ++faceIdx) {
            int count = faceVertexCounts[faceIdx];

            // Extract and sort this face's indices for canonical comparison
            std::vector<int> sortedIndices(count);
            for (int j = 0; j < count; ++j) {
                sortedIndices[j] = faceVertexIndices[offset + j];
            }
            std::sort(sortedIndices.begin(), sortedIndices.end());

            // Hash the canonical (sorted) index sequence
            size_t faceHash = HashIndexSequence(sortedIndices.data(),
                                                 sortedIndices.size());

            if (seenHashes.count(faceHash)) {
                // Potential duplicate -- mark for removal
                facesToKeep[faceIdx] = false;
                duplicateCount++;
            } else {
                seenHashes.insert(faceHash);
            }

            offset += count;
        }

        if (duplicateCount == 0) return;

        std::cout << "[LaminaFaceRemoval] " << mesh.GetPrim().GetPath()
                  << ": removing " << duplicateCount << " duplicate faces\n";

        RemoveFaces(mesh, facesToKeep);
    });
}

} // namespace usdcleaner
