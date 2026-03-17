#include "core/import/FbxImportFixup.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/rotation.h>

#include <iostream>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

FbxImportFixup::FbxImportFixup(const std::string& upAxis, float unitScale)
    : upAxis_(upAxis)
    , unitScale_(unitScale) {}

void FbxImportFixup::Execute(const UsdStageRefPtr& stage) {
    if (!stage) return;

    std::cout << "[FbxImportFixup] Applying BIM import fixups"
              << " (upAxis=" << upAxis_ << ", unitScale=" << unitScale_ << ")\n";

    FixUpAxis(stage);

    if (unitScale_ != 1.0f) {
        ApplyUnitScale(stage);
    }

    PruneEmptyGroups(stage);
}

void FbxImportFixup::FixUpAxis(const UsdStageRefPtr& stage) {
    // Check current stage up axis
    TfToken currentUpAxis = UsdGeomGetStageUpAxis(stage);

    if (upAxis_ == "z" && currentUpAxis == UsdGeomTokens->y) {
        // FBX default is Y-up, BIM convention is Z-up.
        // Set the stage metadata to Z-up. The usdFBX plugin should handle
        // the actual geometry transformation, but we ensure the metadata is correct.
        UsdGeomSetStageUpAxis(stage, UsdGeomTokens->z);
        std::cout << "[FbxImportFixup] Set stage up axis: Y -> Z\n";
    } else if (upAxis_ == "y" && currentUpAxis == UsdGeomTokens->z) {
        UsdGeomSetStageUpAxis(stage, UsdGeomTokens->y);
        std::cout << "[FbxImportFixup] Set stage up axis: Z -> Y\n";
    } else {
        std::cout << "[FbxImportFixup] Up axis already matches (" << currentUpAxis << ")\n";
    }
}

void FbxImportFixup::ApplyUnitScale(const UsdStageRefPtr& stage) {
    // Apply unit scale to the root prim via a scale xformOp.
    // This handles Navisworks exports that use cm or mm instead of meters.
    UsdPrim rootPrim = stage->GetDefaultPrim();
    if (!rootPrim) {
        // Try the pseudo-root's first child
        auto range = stage->GetPseudoRoot().GetChildren();
        for (auto it = range.begin(); it != range.end(); ++it) {
            rootPrim = *it;
            break;
        }
    }

    if (!rootPrim) {
        std::cout << "[FbxImportFixup] No root prim found, skipping unit scale\n";
        return;
    }

    UsdGeomXform rootXform(rootPrim);
    if (!rootXform) {
        std::cout << "[FbxImportFixup] Root prim is not an Xform, skipping unit scale\n";
        return;
    }

    // Set metersPerUnit on the stage
    float currentMPU = static_cast<float>(UsdGeomGetStageMetersPerUnit(stage));
    float newMPU = currentMPU * unitScale_;
    UsdGeomSetStageMetersPerUnit(stage, static_cast<double>(newMPU));

    std::cout << "[FbxImportFixup] Applied unit scale: metersPerUnit "
              << currentMPU << " -> " << newMPU << "\n";
}

void FbxImportFixup::PruneEmptyGroups(const UsdStageRefPtr& stage) {
    // Navisworks exports create deeply nested Xform hierarchies with many
    // empty groups. We deactivate leaf Xforms that have no mesh children
    // and no meaningful attributes, making them invisible to downstream passes.
    std::vector<SdfPath> toDeactivate;

    for (auto prim : stage->Traverse()) {
        if (!prim.IsA<UsdGeomXform>()) continue;
        if (prim.IsA<UsdGeomMesh>()) continue;

        // Check if this Xform has any non-Xform descendants (meshes, materials, etc.)
        bool hasContent = false;
        for (auto child : UsdPrimRange(prim)) {
            if (child == prim) continue;
            if (child.IsA<UsdGeomMesh>() || child.HasRelationship(TfToken("material:binding"))) {
                hasContent = true;
                break;
            }
        }

        if (!hasContent) {
            toDeactivate.push_back(prim.GetPath());
        }
    }

    if (!toDeactivate.empty()) {
        SdfChangeBlock block;
        for (const auto& path : toDeactivate) {
            UsdPrim prim = stage->GetPrimAtPath(path);
            if (prim) {
                prim.SetActive(false);
            }
        }
        std::cout << "[FbxImportFixup] Deactivated " << toDeactivate.size()
                  << " empty Xform groups\n";
    }
}

} // namespace usdcleaner
