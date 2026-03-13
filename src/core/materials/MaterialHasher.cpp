#include "core/materials/MaterialHasher.h"

#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/base/tf/token.h>

#include <algorithm>
#include <sstream>

namespace usdcleaner {

HashDigest MaterialHasher::HashMaterial(const UsdShadeMaterial& material) {
    SHA256Hasher hasher;

    // Hash the material path name structure (but not the specific path,
    // since duplicates will have different paths)
    hasher.Update("MATERIAL");

    // Hash material interface inputs — these are values authored directly on
    // the Material prim (e.g., inputs:baseColor, inputs:specularEdgeColor).
    // In FBX-exported USD, the actual color values live here while shaders
    // only have connections back to them. Without hashing these, materials
    // with different colors but the same shader topology hash identically.
    {
        auto matInputs = material.GetInputs();
        std::sort(matInputs.begin(), matInputs.end(),
                  [](const UsdShadeInput& a, const UsdShadeInput& b) {
                      return a.GetBaseName() < b.GetBaseName();
                  });

        for (const auto& input : matInputs) {
            hasher.Update("MAT_INPUT:");
            hasher.Update(input.GetBaseName().GetString());
            VtValue val;
            if (input.Get(&val)) {
                HashInputValue(val, hasher);
            }
        }
    }

    // Iterate all shader prims under this material, sorted by relative path
    // for deterministic hashing
    std::vector<UsdShadeShader> shaders;
    for (auto prim : UsdPrimRange(material.GetPrim())) {
        if (prim.IsA<UsdShadeShader>()) {
            shaders.push_back(UsdShadeShader(prim));
        }
    }

    // Sort by name for deterministic order
    std::sort(shaders.begin(), shaders.end(),
              [&](const UsdShadeShader& a, const UsdShadeShader& b) {
                  SdfPath relA = a.GetPrim().GetPath().MakeRelativePath(
                      material.GetPrim().GetPath());
                  SdfPath relB = b.GetPrim().GetPath().MakeRelativePath(
                      material.GetPrim().GetPath());
                  return relA < relB;
              });

    for (const auto& shader : shaders) {
        HashShader(shader, hasher);
    }

    return hasher.Finalize();
}

void MaterialHasher::HashShader(const UsdShadeShader& shader,
                                 SHA256Hasher& hasher) {
    // Hash shader ID (e.g., "UsdPreviewSurface")
    TfToken shaderId;
    shader.GetIdAttr().Get(&shaderId);
    hasher.Update(shaderId.GetString());

    // Hash all inputs, sorted by name for determinism
    auto inputs = shader.GetInputs();
    std::sort(inputs.begin(), inputs.end(),
              [](const UsdShadeInput& a, const UsdShadeInput& b) {
                  return a.GetBaseName() < b.GetBaseName();
              });

    for (const auto& input : inputs) {
        hasher.Update(input.GetBaseName().GetString());

        if (input.HasConnectedSource()) {
            // Hash the connection target (relative path within material)
            UsdShadeConnectableAPI source;
            TfToken sourceName;
            UsdShadeAttributeType sourceType;
            if (input.GetConnectedSource(&source, &sourceName, &sourceType)) {
                // Use relative path so different material instances match
                hasher.Update("CONNECTED:");
                hasher.Update(sourceName.GetString());
            }
        } else {
            // Hash the static value
            VtValue val;
            if (input.Get(&val)) {
                HashInputValue(val, hasher);
            }
        }
    }
}

void MaterialHasher::HashInputValue(const VtValue& value,
                                     SHA256Hasher& hasher) {
    // Handle common value types
    if (value.IsHolding<float>()) {
        hasher.Update(value.UncheckedGet<float>());
    } else if (value.IsHolding<GfVec3f>()) {
        auto v = value.UncheckedGet<GfVec3f>();
        hasher.Update(v[0]); hasher.Update(v[1]); hasher.Update(v[2]);
    } else if (value.IsHolding<int>()) {
        hasher.Update(value.UncheckedGet<int>());
    } else if (value.IsHolding<TfToken>()) {
        hasher.Update(value.UncheckedGet<TfToken>().GetString());
    } else if (value.IsHolding<SdfAssetPath>()) {
        // Hash the resolved path (handles relative vs absolute differences)
        auto assetPath = value.UncheckedGet<SdfAssetPath>();
        std::string resolved = assetPath.GetResolvedPath();
        if (resolved.empty()) {
            resolved = assetPath.GetAssetPath();
        }
        hasher.Update(resolved);
    } else if (value.IsHolding<std::string>()) {
        hasher.Update(value.UncheckedGet<std::string>());
    } else {
        // Fallback: hash the type name and string representation
        hasher.Update(value.GetTypeName());
        std::ostringstream ss;
        ss << value;
        hasher.Update(ss.str());
    }
}

bool MaterialHasher::HasAnimatedInputs(const UsdShadeMaterial& material) const {
    for (auto prim : UsdPrimRange(material.GetPrim())) {
        if (!prim.IsA<UsdShadeShader>()) continue;
        UsdShadeShader shader(prim);
        for (const auto& input : shader.GetInputs()) {
            if (input.GetAttr().GetNumTimeSamples() > 0) {
                return true;
            }
        }
    }
    return false;
}

} // namespace usdcleaner
