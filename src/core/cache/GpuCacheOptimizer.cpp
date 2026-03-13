#include "core/cache/GpuCacheOptimizer.h"
#include "core/common/UsdUtils.h"

#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/base/vt/value.h>

#ifdef USDCLEANER_HAS_MESHOPTIMIZER
#include <meshoptimizer.h>
#endif

#include <iostream>
#include <vector>

namespace usdcleaner {

namespace {

// Check if a mesh has any n-gon faces (vertex count > 4) which may be concave.
// Fan triangulation from vertex 0 is only safe for convex polygons (triangles, convex quads).
bool HasNgons(const VtIntArray& faceVertexCounts) {
    for (int count : faceVertexCounts) {
        if (count > 4) {
            return true;
        }
    }
    return false;
}

// Remap a VtArray<T> using a vertex remap table produced by meshoptimizer.
// remapTable maps old_index -> new_index for each original vertex.
template<typename T>
VtArray<T> RemapVertexArray(const VtArray<T>& data,
                             const unsigned int* remapTable,
                             size_t oldVertexCount,
                             size_t newVertexCount) {
    VtArray<T> result(newVertexCount);
    for (size_t i = 0; i < data.size() && i < oldVertexCount; ++i) {
        unsigned int newIdx = remapTable[i];
        if (newIdx < newVertexCount) {
            result[newIdx] = data[i];
        }
    }
    return result;
}

} // anonymous namespace

void GpuCacheOptimizer::Execute(const UsdStageRefPtr& stage) {
#ifndef USDCLEANER_HAS_MESHOPTIMIZER
    std::cout << "[GpuCacheOptimization] meshoptimizer not available, skipping\n";
    return;
#else
    SdfChangeBlock block;

    ForEachMesh(stage, [&](UsdGeomMesh& mesh) {
        VtVec3fArray points;
        VtIntArray faceVertexIndices, faceVertexCounts;
        mesh.GetPointsAttr().Get(&points);
        mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);
        mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);

        if (points.empty() || faceVertexIndices.empty()) return;

        // Check for n-gons: fan triangulation is unsafe for concave polygons
        bool hasNgonFaces = HasNgons(faceVertexCounts);
        if (hasNgonFaces && triangulate_) {
            std::cout << "[GpuCacheOptimization] " << mesh.GetPrim().GetPath()
                      << ": skipping — has n-gon faces (>4 verts) which may be concave; "
                      << "fan triangulation would corrupt geometry\n";
            return;
        }

        if (hasNgonFaces) {
            std::cout << "[GpuCacheOptimization] " << mesh.GetPrim().GetPath()
                      << ": warning — has n-gon faces; cache optimization uses fan "
                      << "triangulation internally (geometry output unchanged)\n";
        }

        // Triangulate: convert faceVertexCounts + faceVertexIndices to triangle list
        std::vector<unsigned int> triangleIndices;
        size_t offset = 0;
        for (int count : faceVertexCounts) {
            if (count < 3) {
                offset += count;
                continue;
            }
            // Fan triangulation from first vertex
            for (int j = 1; j < count - 1; ++j) {
                triangleIndices.push_back(
                    static_cast<unsigned int>(faceVertexIndices[offset]));
                triangleIndices.push_back(
                    static_cast<unsigned int>(faceVertexIndices[offset + j]));
                triangleIndices.push_back(
                    static_cast<unsigned int>(faceVertexIndices[offset + j + 1]));
            }
            offset += count;
        }

        if (triangleIndices.empty()) return;

        // Vertex cache optimization
        std::vector<unsigned int> optimizedIndices(triangleIndices.size());
        meshopt_optimizeVertexCache(
            optimizedIndices.data(),
            triangleIndices.data(),
            triangleIndices.size(),
            points.size());

        // Vertex fetch optimization (reorders vertices for better locality)
        std::vector<unsigned int> remapTable(points.size());
        size_t uniqueVertices = meshopt_optimizeVertexFetchRemap(
            remapTable.data(),
            optimizedIndices.data(),
            optimizedIndices.size(),
            points.size());

        // Apply vertex remap to positions
        std::vector<float> remappedVerts(uniqueVertices * 3);
        meshopt_remapVertexBuffer(
            remappedVerts.data(),
            points.data(),
            points.size(),
            sizeof(float) * 3,
            remapTable.data());

        // Remap indices
        meshopt_remapIndexBuffer(
            optimizedIndices.data(),
            optimizedIndices.data(),
            optimizedIndices.size(),
            remapTable.data());

        // Write back as triangulated mesh if requested
        if (triangulate_) {
            // Build new points array
            VtVec3fArray newPoints(uniqueVertices);
            std::memcpy(newPoints.data(), remappedVerts.data(),
                        uniqueVertices * sizeof(float) * 3);

            VtIntArray newIndices(optimizedIndices.size());
            for (size_t i = 0; i < optimizedIndices.size(); ++i) {
                newIndices[i] = static_cast<int>(optimizedIndices[i]);
            }

            VtIntArray newCounts(optimizedIndices.size() / 3, 3);

            mesh.GetPointsAttr().Set(newPoints);
            mesh.GetFaceVertexIndicesAttr().Set(newIndices);
            mesh.GetFaceVertexCountsAttr().Set(newCounts);

            // Remap all vertex-interpolated primvars to match the new vertex order.
            // Without this, normals, UVs, colors etc. become misaligned with positions.
            UsdGeomPrimvarsAPI primvarsAPI(mesh);
            for (auto& pv : primvarsAPI.GetPrimvars()) {
                TfToken interpolation = pv.GetInterpolation();
                if (interpolation != UsdGeomTokens->vertex &&
                    interpolation != UsdGeomTokens->varying) {
                    continue;
                }

                VtValue val;
                if (!pv.Get(&val)) continue;

                if (val.IsHolding<VtVec3fArray>()) {
                    auto data = val.UncheckedGet<VtVec3fArray>();
                    if (data.size() == points.size()) {
                        pv.Set(RemapVertexArray(data, remapTable.data(),
                                                points.size(), uniqueVertices));
                    }
                } else if (val.IsHolding<VtVec2fArray>()) {
                    auto data = val.UncheckedGet<VtVec2fArray>();
                    if (data.size() == points.size()) {
                        pv.Set(RemapVertexArray(data, remapTable.data(),
                                                points.size(), uniqueVertices));
                    }
                } else if (val.IsHolding<VtFloatArray>()) {
                    auto data = val.UncheckedGet<VtFloatArray>();
                    if (data.size() == points.size()) {
                        pv.Set(RemapVertexArray(data, remapTable.data(),
                                                points.size(), uniqueVertices));
                    }
                } else if (val.IsHolding<VtVec4fArray>()) {
                    auto data = val.UncheckedGet<VtVec4fArray>();
                    if (data.size() == points.size()) {
                        pv.Set(RemapVertexArray(data, remapTable.data(),
                                                points.size(), uniqueVertices));
                    }
                } else if (val.IsHolding<VtIntArray>()) {
                    auto data = val.UncheckedGet<VtIntArray>();
                    if (data.size() == points.size()) {
                        pv.Set(RemapVertexArray(data, remapTable.data(),
                                                points.size(), uniqueVertices));
                    }
                } else if (val.IsHolding<VtVec3dArray>()) {
                    auto data = val.UncheckedGet<VtVec3dArray>();
                    if (data.size() == points.size()) {
                        pv.Set(RemapVertexArray(data, remapTable.data(),
                                                points.size(), uniqueVertices));
                    }
                } else if (val.IsHolding<VtDoubleArray>()) {
                    auto data = val.UncheckedGet<VtDoubleArray>();
                    if (data.size() == points.size()) {
                        pv.Set(RemapVertexArray(data, remapTable.data(),
                                                points.size(), uniqueVertices));
                    }
                }
            }
        }
        // If not triangulating, we still benefit from the analysis but
        // preserve original topology and vertex order.
    });
#endif
}

} // namespace usdcleaner
