#include "core/metadata/MetadataStripper.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/usd/sdf/schema.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/nodeGraph.h>
#include <pxr/base/vt/value.h>
#include <pxr/base/vt/dictionary.h>
#include <pxr/base/tf/token.h>

#include <iostream>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

void MetadataStripper::Execute(const UsdStageRefPtr& stage) {
    customDataEntriesRemoved_ = 0;
    propertiesRemoved_ = 0;
    subdivAttrsRemoved_ = 0;

    // We must use the Sdf layer API for customData detection and clearing.
    // UsdPrim::GetCustomData()/HasCustomDataKey() return COMPOSED values that
    // include USD schema defaults (e.g., UsdGeomMesh schema provides a default
    // userDocBrief). We only want to detect and clear AUTHORED customData
    // from the actual file layer, not schema defaults.
    SdfLayerHandle rootLayer = stage->GetRootLayer();

    // Collect all paths and their modifications before applying
    struct PrimMods {
        SdfPath path;
        bool clearCustomData = false;
        std::vector<TfToken> propsToRemove;
        bool isMesh = false;
    };
    std::vector<PrimMods> allMods;

    for (UsdPrim prim : stage->Traverse()) {
        PrimMods mods;
        mods.path = prim.GetPath();

        // 1. Check for AUTHORED customData containing 'userDocBrief' at the Sdf level
        SdfPrimSpecHandle primSpec = rootLayer->GetPrimAtPath(mods.path);
        if (primSpec && primSpec->HasInfo(SdfFieldKeys->CustomData)) {
            SdfDictionaryProxy cd = primSpec->GetCustomData();
            if (cd.count("userDocBrief") > 0) {
                mods.clearCustomData = true;
            }
        }

        // 2. Check None-valued and empty-array authored properties
        // Determine if this prim is a UsdShade type — its properties need
        // special handling since inputs/outputs are connection endpoints
        // where Get() returns false but the property is essential.
        bool isShadeType = prim.IsA<UsdShadeShader>() ||
                           prim.IsA<UsdShadeMaterial>() ||
                           prim.IsA<UsdShadeNodeGraph>();

        for (const UsdProperty& prop : prim.GetAuthoredProperties()) {
            UsdAttribute attr = prim.GetAttribute(prop.GetName());
            if (!attr || !attr.IsAuthored()) {
                continue;
            }

            // Never strip UsdShade inputs/outputs — these are connection
            // endpoints (e.g., outputs:surface, inputs:diffuseColor.connect)
            // where Get() returns false but the property is essential for
            // material rendering. Also skip any property with connections.
            if (isShadeType) {
                std::string propName = prop.GetName().GetString();
                if (propName.find("inputs:") == 0 ||
                    propName.find("outputs:") == 0) {
                    continue;
                }
            }

            VtValue val;
            if (!attr.Get(&val)) {
                // Get() returned false — attribute has no default value.
                // But it might have time samples! Don't remove time-sampled attrs.
                if (attr.GetNumTimeSamples() > 0) {
                    continue;
                }
                // Skip attributes that have connections (UsdShade connection
                // targets where the value comes from the connected source)
                if (attr.HasAuthoredConnections()) {
                    continue;
                }
                // Truly empty — no default value, no time samples, no connections
                mods.propsToRemove.push_back(prop.GetName());
                continue;
            }

            if (val.IsEmpty()) {
                mods.propsToRemove.push_back(prop.GetName());
                continue;
            }

            // Empty arrays (e.g., empty cornerIndices[], creaseIndices[])
            if (val.IsArrayValued() && val.GetArraySize() == 0) {
                mods.propsToRemove.push_back(prop.GetName());
                continue;
            }
        }

        mods.isMesh = prim.IsA<UsdGeomMesh>();
        allMods.push_back(std::move(mods));
    }

    // Apply all modifications (outside traversal to avoid iterator invalidation)
    for (const auto& mods : allMods) {
        UsdPrim prim = stage->GetPrimAtPath(mods.path);
        if (!prim.IsValid()) continue;

        if (mods.clearCustomData) {
            SdfPrimSpecHandle primSpec = rootLayer->GetPrimAtPath(mods.path);
            if (primSpec && primSpec->HasInfo(SdfFieldKeys->CustomData)) {
                // Only erase the 'userDocBrief' key from customData, preserving
                // any other entries (BIM properties, user annotations, etc.).
                VtDictionary cd = primSpec->GetCustomData();
                cd.erase("userDocBrief");
                if (cd.empty()) {
                    // All entries removed — clear the entire customData field
                    primSpec->ClearInfo(SdfFieldKeys->CustomData);
                } else {
                    // Other entries remain — write back the modified dictionary
                    primSpec->SetInfo(SdfFieldKeys->CustomData, VtValue(cd));
                }
                customDataEntriesRemoved_++;
            }
        }

        for (const TfToken& name : mods.propsToRemove) {
            prim.RemoveProperty(name);
            propertiesRemoved_++;
        }

        if (mods.isMesh) {
            StripRedundantSubdivAttrs(prim);
        }
    }

    std::cout << "[MetadataStripper] Removed " << customDataEntriesRemoved_
              << " customData entries, " << propertiesRemoved_
              << " properties, " << subdivAttrsRemoved_
              << " redundant subdiv attrs\n";
}

void MetadataStripper::StripRedundantSubdivAttrs(UsdPrim& prim) {
    UsdGeomMesh mesh(prim);

    // subdivisionScheme = "none" is the default for non-subdivision meshes
    {
        UsdAttribute attr = mesh.GetSubdivisionSchemeAttr();
        if (attr.IsAuthored()) {
            TfToken val;
            if (attr.Get(&val) && val == UsdGeomTokens->none) {
                prim.RemoveProperty(attr.GetName());
                subdivAttrsRemoved_++;
            }
        }
    }

    // interpolateBoundary = "edgeAndCorner" is the default
    {
        UsdAttribute attr = mesh.GetInterpolateBoundaryAttr();
        if (attr.IsAuthored()) {
            TfToken val;
            if (attr.Get(&val) && val == UsdGeomTokens->edgeAndCorner) {
                prim.RemoveProperty(attr.GetName());
                subdivAttrsRemoved_++;
            }
        }
    }

    // faceVaryingLinearInterpolation = "cornersPlus1" is the default
    {
        UsdAttribute attr = mesh.GetFaceVaryingLinearInterpolationAttr();
        if (attr.IsAuthored()) {
            TfToken val;
            if (attr.Get(&val) && val == UsdGeomTokens->cornersPlus1) {
                prim.RemoveProperty(attr.GetName());
                subdivAttrsRemoved_++;
            }
        }
    }

    // trianglesSubdivisionRule = "catmullClark" is the default
    {
        UsdAttribute attr = mesh.GetTriangleSubdivisionRuleAttr();
        if (attr.IsAuthored()) {
            TfToken val;
            if (attr.Get(&val) && val == UsdGeomTokens->catmullClark) {
                prim.RemoveProperty(attr.GetName());
                subdivAttrsRemoved_++;
            }
        }
    }
}

} // namespace usdcleaner
