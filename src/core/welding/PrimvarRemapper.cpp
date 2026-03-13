#include "core/welding/PrimvarRemapper.h"

#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/base/vt/value.h>
#include <pxr/base/gf/vec2d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec4d.h>

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

        bool handled = false;

        // Helper macro to reduce boilerplate for type dispatch
        #define TRY_REMAP_TYPE(VtType) \
            if (!handled && val.IsHolding<VtType>()) { \
                auto data = val.UncheckedGet<VtType>(); \
                if (data.size() == remap.size()) { \
                    auto compacted = CompactVertexData(data, remap, newVertexCount); \
                    pv.Set(compacted); \
                } \
                handled = true; \
            }

        // Float types (most common in BIM/rendering)
        TRY_REMAP_TYPE(VtVec3fArray)
        TRY_REMAP_TYPE(VtVec2fArray)
        TRY_REMAP_TYPE(VtFloatArray)
        TRY_REMAP_TYPE(VtArray<GfVec4f>)

        // Integer types (element IDs, flags, etc.)
        TRY_REMAP_TYPE(VtIntArray)

        // Double-precision types (high-precision coordinates)
        TRY_REMAP_TYPE(VtArray<GfVec3d>)
        TRY_REMAP_TYPE(VtArray<GfVec2d>)
        TRY_REMAP_TYPE(VtDoubleArray)
        TRY_REMAP_TYPE(VtArray<GfVec4d>)

        #undef TRY_REMAP_TYPE

        if (!handled) {
            std::cerr << "[PrimvarRemapper] Warning: unsupported vertex primvar type "
                      << "on " << pv.GetName() << ", skipping remap\n";
        }
    }
}

// Explicit template instantiations for all supported types
template VtVec3fArray PrimvarRemapper::CompactVertexData(
    const VtVec3fArray&, const RemapTable&, size_t);
template VtVec2fArray PrimvarRemapper::CompactVertexData(
    const VtVec2fArray&, const RemapTable&, size_t);
template VtFloatArray PrimvarRemapper::CompactVertexData(
    const VtFloatArray&, const RemapTable&, size_t);
template VtArray<GfVec4f> PrimvarRemapper::CompactVertexData(
    const VtArray<GfVec4f>&, const RemapTable&, size_t);
template VtIntArray PrimvarRemapper::CompactVertexData(
    const VtIntArray&, const RemapTable&, size_t);
template VtArray<GfVec3d> PrimvarRemapper::CompactVertexData(
    const VtArray<GfVec3d>&, const RemapTable&, size_t);
template VtArray<GfVec2d> PrimvarRemapper::CompactVertexData(
    const VtArray<GfVec2d>&, const RemapTable&, size_t);
template VtDoubleArray PrimvarRemapper::CompactVertexData(
    const VtDoubleArray&, const RemapTable&, size_t);
template VtArray<GfVec4d> PrimvarRemapper::CompactVertexData(
    const VtArray<GfVec4d>&, const RemapTable&, size_t);

} // namespace usdcleaner
