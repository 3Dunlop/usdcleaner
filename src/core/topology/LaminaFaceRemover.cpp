#include "core/topology/LaminaFaceRemover.h"
#include "core/common/UsdUtils.h"
#include "core/common/HashUtils.h"

#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>

#include <algorithm>
#include <unordered_map>
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

        // Map from hash -> list of sorted index sequences seen with that hash.
        // This two-level approach prevents birthday-paradox hash collisions:
        // when two different faces hash to the same value, we do a secondary
        // comparison of the actual sorted indices before declaring them duplicates.
        std::unordered_map<size_t, std::vector<std::vector<int>>> seenFaces;

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

            auto it = seenFaces.find(faceHash);
            if (it != seenFaces.end()) {
                // Hash match — do secondary comparison against all faces with this hash
                bool isActualDuplicate = false;
                for (const auto& prevFace : it->second) {
                    if (prevFace == sortedIndices) {
                        isActualDuplicate = true;
                        break;
                    }
                }
                if (isActualDuplicate) {
                    facesToKeep[faceIdx] = false;
                    duplicateCount++;
                } else {
                    // Hash collision (different face, same hash) — keep both
                    it->second.push_back(std::move(sortedIndices));
                }
            } else {
                seenFaces[faceHash].push_back(std::move(sortedIndices));
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
