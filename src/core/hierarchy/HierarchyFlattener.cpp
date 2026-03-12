#include "core/hierarchy/HierarchyFlattener.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/sdf/namespaceEdit.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/token.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <regex>
#include <cmath>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

namespace {

bool MatchesWildcard(const std::string& name, const std::string& pattern) {
    std::string regexStr;
    for (char c : pattern) {
        if (c == '*') {
            regexStr += ".*";
        } else if (c == '?') {
            regexStr += ".";
        } else if (c == '.' || c == '(' || c == ')' || c == '[' || c == ']'
                   || c == '{' || c == '}' || c == '+' || c == '^' || c == '$'
                   || c == '|' || c == '\\') {
            regexStr += '\\';
            regexStr += c;
        } else {
            regexStr += c;
        }
    }
    try {
        std::regex re(regexStr);
        return std::regex_match(name, re);
    } catch (...) {
        return false;
    }
}

bool IsXformProperty(const UsdProperty& prop) {
    std::string name = prop.GetName().GetString();
    return name == "xformOpOrder" ||
           name.find("xformOp:") == 0;
}

int CountChildren(const UsdPrim& prim) {
    int count = 0;
    for (auto child : prim.GetFilteredChildren(UsdPrimDefaultPredicate)) {
        (void)child;
        count++;
    }
    return count;
}

UsdPrim GetOnlyChild(const UsdPrim& prim) {
    for (auto child : prim.GetFilteredChildren(UsdPrimDefaultPredicate)) {
        return child;
    }
    return UsdPrim();
}

bool IsNearIdentity(const GfMatrix4d& mat, double eps = 1e-10) {
    static const GfMatrix4d identity(1.0);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (std::abs(mat[i][j] - identity[i][j]) > eps) {
                return false;
            }
        }
    }
    return true;
}

} // anonymous namespace

bool HierarchyFlattener::IsSafeToFlatten(const UsdPrim& prim) const {
    // Must be an Xform type (not a Mesh, PointInstancer, etc.)
    if (!prim.IsA<UsdGeomXform>()) {
        return false;
    }

    // Don't flatten the pseudo-root or direct children of root if they have meaning
    if (!prim.GetParent().IsValid() || prim.GetParent() == prim.GetStage()->GetPseudoRoot()) {
        return false;
    }

    // Must have exactly one child
    if (CountChildren(prim) != 1) {
        return false;
    }

    // Has applied API schemas -> preserve
    if (!prim.GetAppliedSchemas().empty()) {
        return false;
    }

    // Has non-xformOp authored properties -> preserve
    for (const UsdProperty& prop : prim.GetAuthoredProperties()) {
        if (!IsXformProperty(prop)) {
            return false;
        }
    }

    // Has authored composition arcs -> preserve
    if (prim.HasAuthoredReferences() ||
        prim.HasAuthoredInherits() ||
        prim.HasAuthoredPayloads() ||
        prim.HasAuthoredSpecializes()) {
        return false;
    }

    // Has visibility/purpose overrides -> preserve
    UsdGeomImageable imageable(prim);
    if (imageable) {
        if (imageable.GetVisibilityAttr().IsAuthored()) {
            return false;
        }
        if (imageable.GetPurposeAttr().IsAuthored()) {
            return false;
        }
    }

    // Name matches protected pattern -> preserve
    std::string primName = prim.GetName().GetString();
    for (const auto& pattern : preservePatterns_) {
        if (MatchesWildcard(primName, pattern)) {
            return false;
        }
    }

    // Has direct material binding -> preserve
    UsdShadeMaterialBindingAPI bindingAPI(prim);
    if (bindingAPI.GetDirectBinding().GetMaterial()) {
        return false;
    }

    return true;
}

void HierarchyFlattener::Execute(const UsdStageRefPtr& stage) {
    // Collect candidates
    std::vector<SdfPath> candidates;

    for (UsdPrim prim : stage->Traverse()) {
        if (IsSafeToFlatten(prim)) {
            candidates.push_back(prim.GetPath());
        }
    }

    // Sort by path depth descending (deepest first = bottom-up)
    std::sort(candidates.begin(), candidates.end(),
              [](const SdfPath& a, const SdfPath& b) {
                  // More path elements = deeper
                  std::string sa = a.GetString();
                  std::string sb = b.GetString();
                  size_t depthA = std::count(sa.begin(), sa.end(), '/');
                  size_t depthB = std::count(sb.begin(), sb.end(), '/');
                  return depthA > depthB;
              });

    size_t primsRemoved = 0;
    SdfLayerHandle rootLayer = stage->GetRootLayer();

    for (const SdfPath& path : candidates) {
        UsdPrim prim = stage->GetPrimAtPath(path);
        if (!prim.IsValid()) {
            continue;
        }

        // Re-verify safety (state may have changed)
        if (CountChildren(prim) != 1) {
            continue;
        }

        UsdPrim child = GetOnlyChild(prim);
        if (!child.IsValid()) {
            continue;
        }

        UsdPrim grandparent = prim.GetParent();
        if (!grandparent.IsValid()) {
            continue;
        }

        // Compose transforms if the intermediate Xform has a non-identity transform
        UsdGeomXformable xformable(prim);
        bool resetsXformStack = false;
        auto ops = xformable.GetOrderedXformOps(&resetsXformStack);

        if (!ops.empty()) {
            GfMatrix4d parentXform(1.0);
            xformable.GetLocalTransformation(&parentXform, &resetsXformStack,
                                              UsdTimeCode::Default());

            if (!IsNearIdentity(parentXform) && child.IsA<UsdGeomXformable>()) {
                UsdGeomXformable childXformable(child);
                bool childResetsStack = false;
                GfMatrix4d childXform(1.0);
                childXformable.GetLocalTransformation(&childXform, &childResetsStack,
                                                       UsdTimeCode::Default());

                // Combined = child * parent (row-major, right-multiply convention)
                GfMatrix4d combined = childXform * parentXform;

                // Clear existing child xform ops
                auto childOps = childXformable.GetOrderedXformOps(&childResetsStack);
                childXformable.ClearXformOpOrder();
                for (const auto& op : childOps) {
                    child.RemoveProperty(op.GetAttr().GetName());
                }

                // Write combined as single matrix xform
                childXformable.MakeMatrixXform().Set(combined);
            }
        }

        // Reparent child under grandparent using SdfBatchNamespaceEdit
        SdfPath childOldPath = child.GetPath();
        TfToken childName = child.GetName();
        SdfPath targetPath = grandparent.GetPath().AppendChild(childName);

        // Check for name collision
        if (stage->GetPrimAtPath(targetPath).IsValid() && targetPath != childOldPath) {
            continue;
        }

        SdfBatchNamespaceEdit edit;
        edit.Add(SdfNamespaceEdit(childOldPath, targetPath));

        if (rootLayer->Apply(edit)) {
            // Remove the now-empty intermediate Xform
            stage->RemovePrim(path);
            primsRemoved++;
        }
    }

    std::cout << "[HierarchyFlattening] Removed " << primsRemoved
              << " intermediate Xform prims\n";
}

} // namespace usdcleaner
