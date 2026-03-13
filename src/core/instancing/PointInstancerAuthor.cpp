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

} // anonymous namespace

void PointInstancerAuthor::Execute(const UsdStageRefPtr& stage) {
    GeometryHasher hasher;
    std::map<HashDigest, std::vector<SdfPath>> groups;

    // 1. Hash all mesh topologies and group by hash
    ForEachMesh(stage, [&](UsdGeomMesh& mesh) {
        // Skip meshes with time-sampled transforms — instancing would
        // lose per-mesh animation (PointInstancer only has one set of time samples)
        if (HasTimeSampledTransform(mesh.GetPrim())) {
            return;
        }

        HashDigest hash = hasher.HashMeshTopology(mesh, positionEpsilon_);
        groups[hash].push_back(mesh.GetPath());
    });

    // 2. Process groups with enough instances
    UsdGeomXformCache xfCache(UsdTimeCode::Default());
    int instancerIndex = 0;
    size_t totalInstanced = 0;
    size_t totalRemoved = 0;

    for (auto& [hash, paths] : groups) {
        if (static_cast<int>(paths.size()) < minInstanceCount_) {
            continue;
        }

        // Extract world transforms and decompose.
        // We compute quaternions at float32 precision (GfQuatf) for accuracy,
        // then convert to half-precision (GfQuath) for the USD schema.
        VtVec3fArray positions;
        VtQuathArray orientations;
        VtVec3fArray scales;
        bool allDecomposable = true;

        for (const SdfPath& path : paths) {
            UsdPrim prim = stage->GetPrimAtPath(path);
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

            positions.push_back(pos);
            // Convert float32 quaternion to half-precision for USD schema
            orientations.push_back(GfQuath(
                static_cast<GfHalf>(orientF.GetReal()),
                static_cast<GfHalf>(orientF.GetImaginary()[0]),
                static_cast<GfHalf>(orientF.GetImaginary()[1]),
                static_cast<GfHalf>(orientF.GetImaginary()[2])));
            scales.push_back(scale);
        }

        if (!allDecomposable) {
            std::cout << "[GeometricInstancing] Skipping group of " << paths.size()
                      << " meshes (non-decomposable transforms)\n";
            continue;
        }

        // 3. Extract material binding from the first mesh BEFORE deleting originals
        SdfPath materialPath;
        {
            UsdPrim firstMesh = stage->GetPrimAtPath(paths[0]);
            if (firstMesh.IsValid()) {
                UsdShadeMaterialBindingAPI bindingAPI(firstMesh);
                UsdShadeMaterial boundMaterial = bindingAPI.ComputeBoundMaterial();
                if (boundMaterial) {
                    materialPath = boundMaterial.GetPrim().GetPath();
                }
            }
        }

        // 4. Create PointInstancer
        SdfPath parentPath = FindCommonParent(paths);
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

        // Copy prototype mesh under instancer
        SdfPath protoPath = instancerPath.AppendChild(TfToken("proto_0"));
        SdfCopySpec(stage->GetRootLayer(), paths[0], stage->GetRootLayer(), protoPath);

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

        // Bind the original material to the prototype mesh
        if (!materialPath.IsEmpty() && protoPrim.IsValid()) {
            UsdPrim materialPrim = stage->GetPrimAtPath(materialPath);
            if (materialPrim.IsValid()) {
                UsdShadeMaterialBindingAPI protoBindingAPI =
                    UsdShadeMaterialBindingAPI::Apply(protoPrim);
                UsdShadeMaterial material(materialPrim);
                protoBindingAPI.Bind(material);
            }
        }

        // Set instancer attributes
        instancer.GetPrototypesRel().SetTargets({protoPath});

        VtIntArray protoIndices(paths.size(), 0);
        instancer.GetProtoIndicesAttr().Set(protoIndices);
        instancer.GetPositionsAttr().Set(positions);
        instancer.GetOrientationsAttr().Set(orientations);
        instancer.GetScalesAttr().Set(scales);

        // Delete original mesh prims
        for (const SdfPath& path : paths) {
            stage->RemovePrim(path);
        }

        totalInstanced += paths.size();
        totalRemoved += paths.size();
    }

    std::cout << "[GeometricInstancing] Created " << instancerIndex
              << " PointInstancers, instanced " << totalInstanced
              << " meshes, removed " << totalRemoved << " prims\n";
}

} // namespace usdcleaner
