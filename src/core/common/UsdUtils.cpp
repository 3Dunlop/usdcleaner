#include "core/common/UsdUtils.h"

#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/boundable.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/vec2d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/vt/value.h>

#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <cmath>

namespace usdcleaner {

void ForEachMesh(const UsdStageRefPtr& stage,
                 const std::function<void(UsdGeomMesh&)>& fn) {
    for (auto prim : stage->Traverse()) {
        if (prim.IsA<UsdGeomMesh>()) {
            UsdGeomMesh mesh(prim);
            fn(mesh);
        }
    }
}

MeshData ExtractMeshData(const UsdGeomMesh& mesh) {
    MeshData data;
    data.primPath = mesh.GetPrim().GetPath();
    mesh.GetPointsAttr().Get(&data.points);
    mesh.GetFaceVertexCountsAttr().Get(&data.faceVertexCounts);
    mesh.GetFaceVertexIndicesAttr().Get(&data.faceVertexIndices);
    return data;
}

void WriteMeshData(UsdGeomMesh& mesh, const MeshData& data) {
    mesh.GetPointsAttr().Set(data.points);
    mesh.GetFaceVertexCountsAttr().Set(data.faceVertexCounts);
    mesh.GetFaceVertexIndicesAttr().Set(data.faceVertexIndices);
}

float ComputeStageDiagonal(const UsdStageRefPtr& stage) {
    UsdGeomBBoxCache bboxCache(
        UsdTimeCode::Default(),
        UsdGeomImageable::GetOrderedPurposeTokens());

    GfBBox3d worldBBox = bboxCache.ComputeWorldBound(stage->GetPseudoRoot());
    GfRange3d range = worldBBox.ComputeAlignedRange();

    if (range.IsEmpty()) {
        return 1.0f;
    }

    GfVec3d size = range.GetSize();
    double diagonal = std::sqrt(size[0] * size[0] +
                                 size[1] * size[1] +
                                 size[2] * size[2]);
    return static_cast<float>(diagonal);
}

float ComputeAutoEpsilon(const UsdStageRefPtr& stage) {
    float diagonal = ComputeStageDiagonal(stage);

    // Use 1e-7 of the scene diagonal as epsilon
    // This gives approximately:
    //   - 0.01mm for a 100m building (in meters)
    //   - 0.01mm for a 100,000mm building (in mm)
    float epsilon = diagonal * 1e-7f;

    // Clamp to a reasonable minimum
    const float minEpsilon = 1e-10f;
    return std::max(epsilon, minEpsilon);
}

namespace {

// Template helper to compact a faceVarying VtArray by removing entries
// corresponding to removed faces.
template<typename T>
bool CompactFaceVaryingArray(const VtValue& original,
                              const VtIntArray& faceVertexCounts,
                              const std::vector<bool>& facesToKeep,
                              VtValue& outCompacted) {
    if (!original.IsHolding<VtArray<T>>()) return false;

    const VtArray<T>& data = original.UncheckedGet<VtArray<T>>();
    VtArray<T> compacted;
    compacted.reserve(data.size());

    size_t fvOffset = 0;
    for (size_t faceIdx = 0; faceIdx < faceVertexCounts.size(); ++faceIdx) {
        int count = faceVertexCounts[faceIdx];
        if (facesToKeep[faceIdx]) {
            for (int j = 0; j < count; ++j) {
                if (fvOffset + j < data.size()) {
                    compacted.push_back(data[fvOffset + j]);
                }
            }
        }
        fvOffset += count;
    }

    outCompacted = VtValue(compacted);
    return true;
}

// Try to compact a faceVarying primvar value across all supported types.
VtValue CompactFaceVaryingValue(const VtValue& original,
                                 const VtIntArray& faceVertexCounts,
                                 const std::vector<bool>& facesToKeep) {
    VtValue result;

    if (CompactFaceVaryingArray<GfVec2f>(original, faceVertexCounts, facesToKeep, result)) return result;
    if (CompactFaceVaryingArray<GfVec3f>(original, faceVertexCounts, facesToKeep, result)) return result;
    if (CompactFaceVaryingArray<GfVec4f>(original, faceVertexCounts, facesToKeep, result)) return result;
    if (CompactFaceVaryingArray<float>(original, faceVertexCounts, facesToKeep, result)) return result;
    if (CompactFaceVaryingArray<int>(original, faceVertexCounts, facesToKeep, result)) return result;
    if (CompactFaceVaryingArray<GfVec2d>(original, faceVertexCounts, facesToKeep, result)) return result;
    if (CompactFaceVaryingArray<GfVec3d>(original, faceVertexCounts, facesToKeep, result)) return result;
    if (CompactFaceVaryingArray<double>(original, faceVertexCounts, facesToKeep, result)) return result;
    if (CompactFaceVaryingArray<GfVec4d>(original, faceVertexCounts, facesToKeep, result)) return result;

    // Unsupported type — log warning
    std::cerr << "[RemoveFaces] Warning: unsupported faceVarying primvar type, "
              << "skipping compaction\n";
    return original;
}

} // anonymous namespace

void RemoveFaces(UsdGeomMesh& mesh, const std::vector<bool>& facesToKeep) {
    VtIntArray faceVertexCounts, faceVertexIndices;
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
    mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);

    if (faceVertexCounts.empty()) return;

    // Collect faceVarying primvar data BEFORE modifying topology
    UsdGeomPrimvarsAPI primvarsAPI(mesh);
    auto primvars = primvarsAPI.GetPrimvars();

    struct FaceVaryingPrimvar {
        UsdGeomPrimvar primvar;
        VtValue originalData;
    };
    std::vector<FaceVaryingPrimvar> fvPrimvars;

    for (auto& pv : primvars) {
        if (pv.GetInterpolation() == UsdGeomTokens->faceVarying) {
            VtValue val;
            if (pv.Get(&val)) {
                fvPrimvars.push_back({pv, val});
            }
        }
    }

    // Also collect uniform (per-face) primvars
    struct UniformPrimvar {
        UsdGeomPrimvar primvar;
        VtValue originalData;
    };
    std::vector<UniformPrimvar> uniformPrimvars;

    for (auto& pv : primvars) {
        if (pv.GetInterpolation() == UsdGeomTokens->uniform) {
            VtValue val;
            if (pv.Get(&val)) {
                uniformPrimvars.push_back({pv, val});
            }
        }
    }

    // Build new counts and indices
    VtIntArray newCounts;
    VtIntArray newIndices;
    newCounts.reserve(faceVertexCounts.size());
    newIndices.reserve(faceVertexIndices.size());

    size_t fvOffset = 0;
    for (size_t faceIdx = 0; faceIdx < faceVertexCounts.size(); ++faceIdx) {
        int count = faceVertexCounts[faceIdx];

        if (facesToKeep[faceIdx]) {
            newCounts.push_back(count);
            for (int j = 0; j < count; ++j) {
                newIndices.push_back(faceVertexIndices[fvOffset + j]);
            }
        }

        fvOffset += count;
    }

    mesh.GetFaceVertexCountsAttr().Set(newCounts);
    mesh.GetFaceVertexIndicesAttr().Set(newIndices);

    // Compact faceVarying primvars to match new topology
    for (auto& fvp : fvPrimvars) {
        VtValue compacted = CompactFaceVaryingValue(
            fvp.originalData, faceVertexCounts, facesToKeep);
        fvp.primvar.Set(compacted);
    }

    // Compact uniform (per-face) primvars — one entry per face
    for (auto& up : uniformPrimvars) {
        VtValue val = up.originalData;

        auto compactUniform = [&](auto tag) -> bool {
            using T = decltype(tag);
            if (!val.IsHolding<VtArray<T>>()) return false;
            const auto& arr = val.UncheckedGet<VtArray<T>>();
            VtArray<T> compacted;
            compacted.reserve(arr.size());
            for (size_t i = 0; i < arr.size() && i < facesToKeep.size(); ++i) {
                if (facesToKeep[i]) {
                    compacted.push_back(arr[i]);
                }
            }
            up.primvar.Set(VtValue(compacted));
            return true;
        };

        bool handled = false;
        handled = handled || compactUniform(float{});
        handled = handled || compactUniform(int{});
        handled = handled || compactUniform(GfVec2f{});
        handled = handled || compactUniform(GfVec3f{});
        handled = handled || compactUniform(GfVec4f{});
        handled = handled || compactUniform(double{});
        handled = handled || compactUniform(GfVec2d{});
        handled = handled || compactUniform(GfVec3d{});

        if (!handled) {
            std::cerr << "[RemoveFaces] Warning: unsupported uniform primvar type, "
                      << "skipping compaction\n";
        }
    }
}

void CompactUnreferencedVertices(VtVec3fArray& points,
                                  VtIntArray& faceVertexIndices) {
    if (points.empty() || faceVertexIndices.empty()) return;

    // Find which vertices are actually referenced
    std::vector<bool> referenced(points.size(), false);
    for (int idx : faceVertexIndices) {
        if (idx >= 0 && static_cast<size_t>(idx) < points.size()) {
            referenced[idx] = true;
        }
    }

    // Build compaction map: old_index -> new_index
    std::vector<int> compactMap(points.size(), -1);
    int newIndex = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        if (referenced[i]) {
            compactMap[i] = newIndex++;
        }
    }

    // Compact points
    VtVec3fArray newPoints(newIndex);
    for (size_t i = 0; i < points.size(); ++i) {
        if (compactMap[i] >= 0) {
            newPoints[compactMap[i]] = points[i];
        }
    }

    // Update indices
    for (size_t i = 0; i < faceVertexIndices.size(); ++i) {
        int idx = faceVertexIndices[i];
        if (idx >= 0 && static_cast<size_t>(idx) < compactMap.size()) {
            faceVertexIndices[i] = compactMap[idx];
        }
    }

    points = std::move(newPoints);
}

} // namespace usdcleaner
