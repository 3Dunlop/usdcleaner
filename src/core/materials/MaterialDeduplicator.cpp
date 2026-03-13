#include "core/materials/MaterialDeduplicator.h"
#include "core/common/UsdUtils.h"

#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/subset.h>
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

    // Phase 3: Rebind all prims from duplicates to masters.
    // Use GetDirectBinding() instead of ComputeBoundMaterial() to correctly
    // handle inherited bindings — ComputeBoundMaterial() returns the composed
    // material which may come from an ancestor, making the rebind lookup fail.
    {
        SdfChangeBlock block;

        for (auto prim : stage->Traverse()) {
            UsdShadeMaterialBindingAPI bindingAPI(prim);

            // 3a. Handle direct material binding on the prim itself
            UsdShadeMaterialBindingAPI::DirectBinding directBinding =
                bindingAPI.GetDirectBinding();
            UsdShadeMaterial directMat = directBinding.GetMaterial();
            if (directMat) {
                std::string boundPath = directMat.GetPrim().GetPath().GetString();
                auto it = duplicateToMaster_.find(boundPath);
                if (it != duplicateToMaster_.end()) {
                    SdfPath masterPath(it->second);
                    UsdPrim masterPrim = stage->GetPrimAtPath(masterPath);
                    if (masterPrim) {
                        UsdShadeMaterial masterMaterial(masterPrim);
                        bindingAPI.Bind(masterMaterial);
                    }
                }
            }

            // 3b. Handle per-face subset bindings (materialBinding:subset)
            // UsdGeomSubset children with material bindings
            if (prim.IsA<UsdGeomMesh>()) {
                for (auto child : prim.GetChildren()) {
                    if (!child.IsA<UsdGeomSubset>()) continue;

                    UsdShadeMaterialBindingAPI subsetBindingAPI(child);
                    UsdShadeMaterialBindingAPI::DirectBinding subsetBinding =
                        subsetBindingAPI.GetDirectBinding();
                    UsdShadeMaterial subsetMat = subsetBinding.GetMaterial();
                    if (!subsetMat) continue;

                    std::string subsetBoundPath =
                        subsetMat.GetPrim().GetPath().GetString();
                    auto it = duplicateToMaster_.find(subsetBoundPath);
                    if (it != duplicateToMaster_.end()) {
                        SdfPath masterPath(it->second);
                        UsdPrim masterPrim = stage->GetPrimAtPath(masterPath);
                        if (masterPrim) {
                            UsdShadeMaterial masterMaterial(masterPrim);
                            subsetBindingAPI.Bind(masterMaterial);
                        }
                    }
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
