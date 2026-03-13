#include "core/instancing/PointInstancerAuthor.h"
#include "core/instancing/GeometryHasher.h"
#include "core/common/UsdUtils.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/sdf/copyUtils.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/quatd.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

namespace {

// Find the common ancestor path for a set of prim paths
SdfPath FindCommonParent(const std::vector<SdfPath>& paths) {
    if (paths.empty()) return SdfPath::AbsoluteRootPath();

    SdfPath common = paths[0].GetParentPath();
    for (size_t i = 1; i < paths.size(); ++i) {
        SdfPath parent = paths[i].GetParentPath();
        // Walk up both paths until they match
        while (common != SdfPath::AbsoluteRootPath() &&
               !parent.HasPrefix(common)) {
            common = common.GetParentPath();
        }
    }
    return common;
}

// Decompose a world transform matrix into position, orientation, scale.
// Returns false if the transform contains shear or negative determinant.
// Uses GfQuatf (32-bit float) instead of GfQuath (16-bit half) for better
// precision, especially important for large-scale BIM (buildings 100m+).
bool DecomposeTransform(const GfMatrix4d& xform,
                        GfVec3f& outPosition,
                        GfQuatf& outOrientation,
                        GfVec3f& outScale) {
    // Check determinant - negative means mirroring which PointInstancer can't represent
    double det = xform.GetDeterminant();
    if (det < 0.0) {
        return false;
    }

    // Extract translation from last row
    outPosition = GfVec3f(
        static_cast<float>(xform[3][0]),
        static_cast<float>(xform[3][1]),
        static_cast<float>(xform[3][2]));

    // Extract scale as column magnitudes
    double sx = GfVec3d(xform[0][0], xform[0][1], xform[0][2]).GetLength();
    double sy = GfVec3d(xform[1][0], xform[1][1], xform[1][2]).GetLength();
    double sz = GfVec3d(xform[2][0], xform[2][1], xform[2][2]).GetLength();

    if (sx < 1e-10 || sy < 1e-10 || sz < 1e-10) {
        return false; // Degenerate scale
    }

    outScale = GfVec3f(
        static_cast<float>(sx),
        static_cast<float>(sy),
        static_cast<float>(sz));

    // Build rotation matrix by dividing out scale
    GfMatrix4d rotMat(1.0);
    rotMat[0][0] = xform[0][0] / sx; rotMat[0][1] = xform[0][1] / sx; rotMat[0][2] = xform[0][2] / sx;
    rotMat[1][0] = xform[1][0] / sy; rotMat[1][1] = xform[1][1] / sy; rotMat[1][2] = xform[1][2] / sy;
    rotMat[2][0] = xform[2][0] / sz; rotMat[2][1] = xform[2][1] / sz; rotMat[2][2] = xform[2][2] / sz;

    // Check for shear: rotation matrix columns should be orthogonal
    GfVec3d c0(rotMat[0][0], rotMat[0][1], rotMat[0][2]);
    GfVec3d c1(rotMat[1][0], rotMat[1][1], rotMat[1][2]);
    GfVec3d c2(rotMat[2][0], rotMat[2][1], rotMat[2][2]);
    double dot01 = std::abs(GfDot(c0, c1));
    double dot02 = std::abs(GfDot(c0, c2));
    double dot12 = std::abs(GfDot(c1, c2));
    if (dot01 > 1e-4 || dot02 > 1e-4 || dot12 > 1e-4) {
        return false; // Has shear
    }

    // Convert rotation matrix to quaternion via GfMatrix3d
    GfMatrix3d rotMat3d(
        rotMat[0][0], rotMat[0][1], rotMat[0][2],
        rotMat[1][0], rotMat[1][1], rotMat[1][2],
        rotMat[2][0], rotMat[2][1], rotMat[2][2]);
    GfRotation rotation = rotMat3d.ExtractRotation();
    GfQuatd quatd = rotation.GetQuat();
    // Use GfQuatf (32-bit float) for better precision than GfQuath (16-bit half)
    outOrientation = GfQuatf(
        static_cast<float>(quatd.GetReal()),
        static_cast<float>(quatd.GetImaginary()[0]),
        static_cast<float>(quatd.GetImaginary()[1]),
        static_cast<float>(quatd.GetImaginary()[2]));

    return true;
}

// Check if any xformOp on a prim has time samples
bool HasTimeSampledTransform(const UsdPrim& prim) {
    if (!prim.IsA<UsdGeomXformable>()) return false;
    UsdGeomXformable xformable(prim);
    bool resetsStack = false;
    auto ops = xformable.GetOrderedXformOps(&resetsStack);
    for (const auto& op : ops) {
        if (op.GetAttr().GetNumTimeSamples() > 0) {
            return true;
        }
    }
    return false;
}

// Derive a descriptive prototype name from a mesh prim
std::string DerivePrototypeName(const UsdPrim& meshPrim) {
    std::string protoName = "proto_0";
    if (meshPrim.IsValid()) {
        std::string meshName = meshPrim.GetName().GetString();
        // If the mesh is just called "Mesh", use the parent name instead
        if (meshName == "Mesh" || meshName == "mesh") {
            UsdPrim parent = meshPrim.GetParent();
            if (parent.IsValid() && parent != meshPrim.GetStage()->GetPseudoRoot()) {
                protoName = parent.GetName().GetString();
            }
        } else {
            protoName = meshName;
        }
        // Sanitize: USD prim names must start with letter/underscore
        if (!protoName.empty() && !std::isalpha(protoName[0]) && protoName[0] != '_') {
            protoName = "_" + protoName;
        }
    }
    return protoName;
}

// Get material binding path for a mesh
SdfPath GetMaterialPath(const UsdStageRefPtr& stage, const SdfPath& meshPath) {
    UsdPrim prim = stage->GetPrimAtPath(meshPath);
    if (!prim.IsValid()) return SdfPath();

    UsdShadeMaterialBindingAPI bindingAPI(prim);
    UsdShadeMaterial boundMaterial = bindingAPI.ComputeBoundMaterial();
    if (boundMaterial) {
        return boundMaterial.GetPrim().GetPath();
    }
    return SdfPath();
}

// Info about a mesh instance for grouping
struct MeshInstanceInfo {
    SdfPath meshPath;
    GfVec3f centroid{0.0f, 0.0f, 0.0f};  // local-space centroid (for normalized mode)
    float boundsDiagonal = 1.0f;           // for scale normalization
};

} // anonymous namespace

void PointInstancerAuthor::Execute(const UsdStageRefPtr& stage) {
    GeometryHasher hasher;

    // Map from geometry hash -> list of mesh instances
    std::map<HashDigest, std::vector<MeshInstanceInfo>> groups;
    size_t totalMeshes = 0;
    size_t skippedAnimated = 0;

    // 1. Hash all mesh topologies and group by hash
    ForEachMesh(stage, [&](UsdGeomMesh& mesh) {
        totalMeshes++;

        // Skip meshes with time-sampled transforms — instancing would
        // lose per-mesh animation (PointInstancer only has one set of time samples)
        if (HasTimeSampledTransform(mesh.GetPrim())) {
            skippedAnimated++;
            return;
        }

        MeshInstanceInfo info;
        info.meshPath = mesh.GetPath();

        if (normalizeCentroids_) {
            auto normResult = hasher.HashMeshNormalized(mesh, positionEpsilon_, normalizeScale_);
            info.centroid = normResult.centroid;
            info.boundsDiagonal = normResult.boundsDiagonal;
            groups[normResult.hash].push_back(info);
        } else {
            HashDigest hash = hasher.HashMeshTopology(mesh, positionEpsilon_);
            groups[hash].push_back(info);
        }
    });

    // 2. Process groups with enough instances
    UsdGeomXformCache xfCache(UsdTimeCode::Default());
    int instancerIndex = 0;
    size_t totalInstanced = 0;
    size_t totalRemoved = 0;
    size_t skippedNonDecomposable = 0;
    size_t skippedBelowThreshold = 0;
    size_t groupsBelowThreshold = 0;

    for (auto& [hash, instances] : groups) {
        if (static_cast<int>(instances.size()) < minInstanceCount_) {
            groupsBelowThreshold++;
            skippedBelowThreshold += instances.size();
            continue;
        }

        // Sub-group by material binding for multi-prototype support.
        // Each unique material gets its own prototype within the same PointInstancer.
        std::map<SdfPath, std::vector<size_t>> materialGroups;
        for (size_t i = 0; i < instances.size(); ++i) {
            SdfPath matPath = GetMaterialPath(stage, instances[i].meshPath);
            materialGroups[matPath].push_back(i);
        }

        // Extract world transforms and decompose for ALL instances.
        // We compute quaternions at float32 precision (GfQuatf) for accuracy,
        // then convert to half-precision (GfQuath) for the USD schema.
        VtVec3fArray positions;
        VtQuathArray orientations;
        VtVec3fArray scales;
        VtIntArray protoIndices;
        bool allDecomposable = true;

        // Build per-instance data in order, tracking which prototype index each uses
        struct InstanceData {
            GfVec3f position;
            GfQuath orientation;
            GfVec3f scale;
            int protoIndex;
        };
        std::vector<InstanceData> instanceData(instances.size());

        // Assign prototype indices based on material grouping
        int protoIdx = 0;
        std::vector<std::pair<SdfPath, int>> materialProtoMap; // materialPath -> protoIndex
        for (auto& [matPath, memberIndices] : materialGroups) {
            materialProtoMap.push_back({matPath, protoIdx});
            for (size_t idx : memberIndices) {
                instanceData[idx].protoIndex = protoIdx;
            }
            protoIdx++;
        }

        for (size_t i = 0; i < instances.size(); ++i) {
            UsdPrim prim = stage->GetPrimAtPath(instances[i].meshPath);
            if (!prim.IsValid()) {
                allDecomposable = false;
                break;
            }

            GfMatrix4d worldXform = xfCache.GetLocalToWorldTransform(prim);

            GfVec3f pos;
            GfQuatf orientF;
            GfVec3f scale;
            if (!DecomposeTransform(worldXform, pos, orientF, scale)) {
                allDecomposable = false;
                break;
            }

            // If centroid normalization is active, we need to fold the local centroid
            // offset into the world position. The prototype has positions relative to
            // the centroid, so each instance needs to be positioned at:
            //   worldPos = originalWorldPos + rotation * (scale * centroid)
            if (normalizeCentroids_) {
                GfVec3f centroid = instances[i].centroid;

                // If scale normalization is also active, fold the bounds diagonal
                // into the instance scale
                if (normalizeScale_) {
                    float localScale = instances[i].boundsDiagonal;
                    scale[0] *= localScale;
                    scale[1] *= localScale;
                    scale[2] *= localScale;
                    // Centroid is in pre-scale-normalization space, so scale it too
                    centroid[0] *= localScale;
                    centroid[1] *= localScale;
                    centroid[2] *= localScale;
                }

                // Transform centroid offset by the world rotation and scale
                // to get the world-space centroid position
                GfVec3f scaledCentroid(
                    centroid[0] * scale[0],
                    centroid[1] * scale[1],
                    centroid[2] * scale[2]);

                // Rotate the scaled centroid by the orientation quaternion
                GfRotation rot(GfQuatd(orientF.GetReal(),
                    orientF.GetImaginary()[0],
                    orientF.GetImaginary()[1],
                    orientF.GetImaginary()[2]));
                GfVec3d rotatedCentroid = rot.TransformDir(
                    GfVec3d(scaledCentroid[0], scaledCentroid[1], scaledCentroid[2]));

                pos[0] += static_cast<float>(rotatedCentroid[0]);
                pos[1] += static_cast<float>(rotatedCentroid[1]);
                pos[2] += static_cast<float>(rotatedCentroid[2]);
            }

            instanceData[i].position = pos;
            instanceData[i].orientation = GfQuath(
                static_cast<GfHalf>(orientF.GetReal()),
                static_cast<GfHalf>(orientF.GetImaginary()[0]),
                static_cast<GfHalf>(orientF.GetImaginary()[1]),
                static_cast<GfHalf>(orientF.GetImaginary()[2]));
            instanceData[i].scale = scale;
        }

        if (!allDecomposable) {
            skippedNonDecomposable += instances.size();
            std::cout << "[GeometricInstancing] Skipping group of " << instances.size()
                      << " meshes (non-decomposable transforms)\n";
            continue;
        }

        // Build final arrays
        for (size_t i = 0; i < instances.size(); ++i) {
            positions.push_back(instanceData[i].position);
            orientations.push_back(instanceData[i].orientation);
            scales.push_back(instanceData[i].scale);
            protoIndices.push_back(instanceData[i].protoIndex);
        }

        // Collect all mesh paths for common parent computation
        std::vector<SdfPath> allPaths;
        allPaths.reserve(instances.size());
        for (const auto& inst : instances) {
            allPaths.push_back(inst.meshPath);
        }

        // 3. Create PointInstancer
        SdfPath parentPath = FindCommonParent(allPaths);
        std::string instancerName = "Instancer_" + std::to_string(instancerIndex++);
        SdfPath instancerPath = parentPath.AppendChild(TfToken(instancerName));

        // Avoid name collision
        while (stage->GetPrimAtPath(instancerPath).IsValid()) {
            instancerName = "Instancer_" + std::to_string(instancerIndex++);
            instancerPath = parentPath.AppendChild(TfToken(instancerName));
        }

        // Define PointInstancer (must NOT be inside SdfChangeBlock — Define needs
        // immediate change notification to work correctly)
        UsdGeomPointInstancer instancer =
            UsdGeomPointInstancer::Define(stage, instancerPath);
        if (!instancer.GetPrim().IsValid()) {
            std::cerr << "[GeometricInstancing] Failed to define instancer at "
                      << instancerPath << "\n";
            continue;
        }

        // 4. Create prototypes — one per unique material
        std::vector<SdfPath> protoPaths;
        std::string baseProtoName = DerivePrototypeName(
            stage->GetPrimAtPath(instances[0].meshPath));

        for (auto& [matPath, matProtoIdx] : materialProtoMap) {
            std::string protoName = baseProtoName;
            if (materialProtoMap.size() > 1) {
                protoName += "_mat" + std::to_string(matProtoIdx);
            }

            SdfPath protoPath = instancerPath.AppendChild(TfToken(protoName));
            // Handle prototype name collision
            if (stage->GetPrimAtPath(protoPath).IsValid()) {
                int suffix = 1;
                while (stage->GetPrimAtPath(
                    instancerPath.AppendChild(TfToken(protoName + "_" + std::to_string(suffix)))).IsValid()) {
                    suffix++;
                }
                protoName = protoName + "_" + std::to_string(suffix);
                protoPath = instancerPath.AppendChild(TfToken(protoName));
            }

            // Copy first mesh of this material group as the prototype
            size_t firstMeshIdx = materialGroups[matPath][0];
            SdfCopySpec(stage->GetRootLayer(), instances[firstMeshIdx].meshPath,
                        stage->GetRootLayer(), protoPath);

            // Clear prototype's transform (positions are in the instancer)
            UsdPrim protoPrim = stage->GetPrimAtPath(protoPath);
            if (protoPrim.IsValid() && protoPrim.IsA<UsdGeomXformable>()) {
                UsdGeomXformable protoXformable(protoPrim);
                bool resetsStack = false;
                auto ops = protoXformable.GetOrderedXformOps(&resetsStack);
                protoXformable.ClearXformOpOrder();
                for (const auto& op : ops) {
                    protoPrim.RemoveProperty(op.GetAttr().GetName());
                }
            }

            // If centroid normalization is active, translate prototype vertices
            // so centroid is at origin
            if (normalizeCentroids_ && protoPrim.IsValid()) {
                UsdGeomMesh protoMesh(protoPrim);
                VtVec3fArray points;
                protoMesh.GetPointsAttr().Get(&points);
                if (!points.empty()) {
                    GfVec3f centroid = instances[firstMeshIdx].centroid;
                    float scaleFactor = 1.0f;
                    if (normalizeScale_ && instances[firstMeshIdx].boundsDiagonal > 1e-10f) {
                        scaleFactor = 1.0f / instances[firstMeshIdx].boundsDiagonal;
                    }
                    for (auto& p : points) {
                        p[0] = (p[0] - centroid[0]) * scaleFactor;
                        p[1] = (p[1] - centroid[1]) * scaleFactor;
                        p[2] = (p[2] - centroid[2]) * scaleFactor;
                    }
                    protoMesh.GetPointsAttr().Set(points);
                }
            }

            // Bind the material to this prototype
            if (!matPath.IsEmpty() && protoPrim.IsValid()) {
                UsdPrim materialPrim = stage->GetPrimAtPath(matPath);
                if (materialPrim.IsValid()) {
                    UsdShadeMaterialBindingAPI protoBindingAPI =
                        UsdShadeMaterialBindingAPI::Apply(protoPrim);
                    UsdShadeMaterial material(materialPrim);
                    protoBindingAPI.Bind(material);
                }
            }

            protoPaths.push_back(protoPath);
        }

        // Set instancer attributes
        instancer.GetPrototypesRel().SetTargets(protoPaths);
        instancer.GetProtoIndicesAttr().Set(protoIndices);
        instancer.GetPositionsAttr().Set(positions);
        instancer.GetOrientationsAttr().Set(orientations);
        instancer.GetScalesAttr().Set(scales);

        // Delete original mesh prims
        for (const auto& inst : instances) {
            stage->RemovePrim(inst.meshPath);
        }

        totalInstanced += instances.size();
        totalRemoved += instances.size();
    }

    // Diagnostics
    std::cout << "[GeometricInstancing] Scanned " << totalMeshes << " meshes, "
              << groups.size() << " unique geometries"
              << (normalizeCentroids_ ? " (centroid-normalized)" : "") << "\n";
    if (skippedAnimated > 0) {
        std::cout << "[GeometricInstancing] Skipped " << skippedAnimated
                  << " animated meshes\n";
    }
    if (skippedNonDecomposable > 0) {
        std::cout << "[GeometricInstancing] Skipped " << skippedNonDecomposable
                  << " meshes with non-decomposable transforms\n";
    }
    if (groupsBelowThreshold > 0) {
        std::cout << "[GeometricInstancing] " << groupsBelowThreshold
                  << " groups (" << skippedBelowThreshold << " meshes) below min-instance-count "
                  << minInstanceCount_ << "\n";
    }
    std::cout << "[GeometricInstancing] Created " << instancerIndex
              << " PointInstancers, instanced " << totalInstanced
              << " meshes, removed " << totalRemoved << " prims\n";
}

} // namespace usdcleaner
