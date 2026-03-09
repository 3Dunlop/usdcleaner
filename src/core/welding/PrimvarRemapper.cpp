#include "core/welding/PrimvarRemapper.h"

#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/base/vt/value.h>

#include <iostream>

namespace usdcleaner {

template<typename T>
VtArray<T> PrimvarRemapper::CompactVertexData(const VtArray<T>& data,
                                                const RemapTable& remap,
                                                size_t newCount) {
    VtArray<T> compacted(newCount);
    std::vector<bool> filled(newCount, false);

    for (size_t i = 0; i < data.size() && i < remap.size(); ++i) {
        uint32_t newIdx = remap[i];
        if (newIdx < newCount && !filled[newIdx]) {
            compacted[newIdx] = data[i];
            filled[newIdx] = true;
        }
    }

    return compacted;
}

void PrimvarRemapper::RemapPrimvars(UsdGeomMesh& mesh,
                                     const RemapTable& remap,
                                     size_t newVertexCount) {
    UsdGeomPrimvarsAPI primvarsAPI(mesh);
    auto primvars = primvarsAPI.GetPrimvars();

    for (auto& pv : primvars) {
        TfToken interpolation = pv.GetInterpolation();

        // Only remap vertex/varying interpolated primvars
        if (interpolation != UsdGeomTokens->vertex &&
            interpolation != UsdGeomTokens->varying) {
            continue;
        }

        VtValue val;
        if (!pv.Get(&val)) continue;

        // Handle common primvar types
        if (val.IsHolding<VtVec3fArray>()) {
            auto data = val.UncheckedGet<VtVec3fArray>();
            if (data.size() == remap.size()) {
                auto compacted = CompactVertexData(data, remap, newVertexCount);
                pv.Set(compacted);
            }
        } else if (val.IsHolding<VtVec2fArray>()) {
            auto data = val.UncheckedGet<VtVec2fArray>();
            if (data.size() == remap.size()) {
                auto compacted = CompactVertexData(data, remap, newVertexCount);
                pv.Set(compacted);
            }
        } else if (val.IsHolding<VtFloatArray>()) {
            auto data = val.UncheckedGet<VtFloatArray>();
            if (data.size() == remap.size()) {
                auto compacted = CompactVertexData(data, remap, newVertexCount);
                pv.Set(compacted);
            }
        } else if (val.IsHolding<VtVec4fArray>()) {
            auto data = val.UncheckedGet<VtVec4fArray>();
            if (data.size() == remap.size()) {
                auto compacted = CompactVertexData(data, remap, newVertexCount);
                pv.Set(compacted);
            }
        }
        // faceVarying, uniform, constant primvars are left untouched
    }
}

// Explicit template instantiations
template VtVec3fArray PrimvarRemapper::CompactVertexData(
    const VtVec3fArray&, const RemapTable&, size_t);
template VtVec2fArray PrimvarRemapper::CompactVertexData(
    const VtVec2fArray&, const RemapTable&, size_t);
template VtFloatArray PrimvarRemapper::CompactVertexData(
    const VtFloatArray&, const RemapTable&, size_t);
template VtArray<GfVec4f> PrimvarRemapper::CompactVertexData(
    const VtArray<GfVec4f>&, const RemapTable&, size_t);

} // namespace usdcleaner
