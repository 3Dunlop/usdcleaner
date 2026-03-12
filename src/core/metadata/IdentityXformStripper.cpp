#include "core/metadata/IdentityXformStripper.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/base/gf/matrix4d.h>

#include <iostream>
#include <cmath>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

bool IdentityXformStripper::IsNearIdentity(const GfMatrix4d& mat) const {
    static const GfMatrix4d identity(1.0);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (std::abs(mat[i][j] - identity[i][j]) > epsilon_) {
                return false;
            }
        }
    }
    return true;
}

void IdentityXformStripper::Execute(const UsdStageRefPtr& stage) {
    xformOpsCleared_ = 0;

    // First pass: collect candidates (avoid modifying while traversing)
    struct Candidate {
        SdfPath path;
        std::vector<TfToken> opAttrNames;
    };
    std::vector<Candidate> candidates;

    for (UsdPrim prim : stage->Traverse()) {
        if (!prim.IsA<UsdGeomXformable>()) {
            continue;
        }

        UsdGeomXformable xformable(prim);
        bool resetsXformStack = false;
        std::vector<UsdGeomXformOp> ops = xformable.GetOrderedXformOps(&resetsXformStack);

        if (ops.empty()) {
            continue;
        }

        // Compose local transform
        GfMatrix4d localXform(1.0);
        bool success = xformable.GetLocalTransformation(&localXform, &resetsXformStack,
                                                         UsdTimeCode::Default());
        if (!success) {
            continue;
        }

        if (IsNearIdentity(localXform)) {
            Candidate c;
            c.path = prim.GetPath();
            for (const auto& op : ops) {
                c.opAttrNames.push_back(op.GetAttr().GetName());
            }
            candidates.push_back(std::move(c));
        }
    }

    // Second pass: apply changes inside SdfChangeBlock
    SdfChangeBlock changeBlock;
    for (const auto& c : candidates) {
        UsdPrim prim = stage->GetPrimAtPath(c.path);
        if (!prim.IsValid()) {
            continue;
        }

        UsdGeomXformable xformable(prim);
        xformable.ClearXformOpOrder();

        // Remove individual xformOp attributes
        for (const TfToken& attrName : c.opAttrNames) {
            prim.RemoveProperty(attrName);
        }

        xformOpsCleared_++;
    }

    std::cout << "[IdentityXformStripper] Cleared " << xformOpsCleared_
              << " identity xformOps\n";
}

} // namespace usdcleaner
