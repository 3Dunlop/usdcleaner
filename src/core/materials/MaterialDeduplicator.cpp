#include "core/materials/MaterialDeduplicator.h"
#include "core/common/UsdUtils.h"

#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include <iostream>
#include <map>

namespace usdcleaner {

void MaterialDeduplicator::Execute(const UsdStageRefPtr& stage) {
    duplicateToMaster_.clear();

    // Phase 1: Hash all materials and group by hash
    // Key: hex digest, Value: list of material paths
    std::map<std::string, std::vector<SdfPath>> hashGroups;

    for (auto prim : stage->Traverse()) {
        if (!prim.IsA<UsdShadeMaterial>()) continue;

        UsdShadeMaterial material(prim);

        // Skip animated materials if configured
        if (skipAnimated_ && hasher_.HasAnimatedInputs(material)) {
            continue;
        }

        HashDigest digest = hasher_.HashMaterial(material);
        std::string hexDigest = SHA256Hasher::DigestToHex(digest);

        hashGroups[hexDigest].push_back(prim.GetPath());
    }

    // Phase 2: Identify duplicates
    for (const auto& [hash, paths] : hashGroups) {
        if (paths.size() <= 1) continue;

        // First material is the master
        const SdfPath& masterPath = paths[0];
        for (size_t i = 1; i < paths.size(); ++i) {
            duplicateToMaster_[paths[i].GetString()] = masterPath.GetString();
        }
    }

    if (duplicateToMaster_.empty()) {
        std::cout << "[MaterialDeduplication] No duplicate materials found\n";
        return;
    }

    std::cout << "[MaterialDeduplication] Found " << duplicateToMaster_.size()
              << " duplicate materials\n";

    // Phase 3: Rebind all meshes from duplicates to masters
    {
        SdfChangeBlock block;

        for (auto prim : stage->Traverse()) {
            if (!prim.IsA<UsdGeomMesh>()) continue;

            UsdShadeMaterialBindingAPI bindingAPI(prim);
            UsdShadeMaterial boundMaterial = bindingAPI.ComputeBoundMaterial();

            if (!boundMaterial) continue;

            std::string boundPath = boundMaterial.GetPrim().GetPath().GetString();
            auto it = duplicateToMaster_.find(boundPath);
            if (it != duplicateToMaster_.end()) {
                // Rebind to the master material
                SdfPath masterPath(it->second);
                UsdPrim masterPrim = stage->GetPrimAtPath(masterPath);
                if (masterPrim) {
                    UsdShadeMaterial masterMaterial(masterPrim);
                    bindingAPI.Bind(masterMaterial);
                }
            }
        }
    }

    // Phase 4: Prune orphaned duplicate materials
    {
        SdfChangeBlock block;
        for (const auto& [dupPath, masterPath] : duplicateToMaster_) {
            stage->RemovePrim(SdfPath(dupPath));
        }
    }

    std::cout << "[MaterialDeduplication] Pruned " << duplicateToMaster_.size()
              << " duplicate material prims\n";
}

} // namespace usdcleaner
