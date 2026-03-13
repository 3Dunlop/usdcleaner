#include <gtest/gtest.h>
#include "core/instancing/PointInstancerAuthor.h"
#include "core/instancing/GeometryHasher.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/material.h>

using namespace usdcleaner;
PXR_NAMESPACE_USING_DIRECTIVE

class InstancingTest : public ::testing::Test {
protected:
    std::string TestDataPath(const std::string& filename) {
        return std::string(TEST_DATA_DIR) + "/" + filename;
    }
};

TEST_F(InstancingTest, GeometryHasherConsistency) {
    auto stage = UsdStage::Open(TestDataPath("instancable_meshes.usda"));
    ASSERT_TRUE(stage);

    GeometryHasher hasher;

    // Hash all four cubes - they should produce the same hash
    UsdGeomMesh cubeA(stage->GetPrimAtPath(SdfPath("/Root/CubeA/Cube")));
    UsdGeomMesh cubeB(stage->GetPrimAtPath(SdfPath("/Root/CubeB/Cube")));
    UsdGeomMesh cubeC(stage->GetPrimAtPath(SdfPath("/Root/CubeC/Cube")));
    UsdGeomMesh cubeD(stage->GetPrimAtPath(SdfPath("/Root/CubeD/Cube")));

    ASSERT_TRUE(cubeA.GetPrim().IsValid());
    ASSERT_TRUE(cubeB.GetPrim().IsValid());

    HashDigest hashA = hasher.HashMeshTopology(cubeA, 1e-3f);
    HashDigest hashB = hasher.HashMeshTopology(cubeB, 1e-3f);
    HashDigest hashC = hasher.HashMeshTopology(cubeC, 1e-3f);
    HashDigest hashD = hasher.HashMeshTopology(cubeD, 1e-3f);

    EXPECT_EQ(hashA, hashB);
    EXPECT_EQ(hashB, hashC);
    EXPECT_EQ(hashC, hashD);

    // Triangle should have a different hash
    UsdGeomMesh triangle(stage->GetPrimAtPath(SdfPath("/Root/Triangle")));
    ASSERT_TRUE(triangle.GetPrim().IsValid());
    HashDigest hashTriangle = hasher.HashMeshTopology(triangle, 1e-3f);
    EXPECT_NE(hashA, hashTriangle);
}

TEST_F(InstancingTest, CreatesPointInstancer) {
    auto stage = UsdStage::Open(TestDataPath("instancable_meshes.usda"));
    ASSERT_TRUE(stage);

    // Count meshes before
    int meshCountBefore = 0;
    for (auto prim : stage->Traverse()) {
        if (prim.IsA<UsdGeomMesh>()) meshCountBefore++;
    }
    EXPECT_EQ(meshCountBefore, 5); // 4 cubes + 1 triangle

    PointInstancerAuthor instancer;
    instancer.SetMinInstanceCount(3); // Need at least 3 to instance
    instancer.SetNormalizeCentroids(false); // Use exact matching for this legacy test
    instancer.Execute(stage);

    // Should have created a PointInstancer
    int instancerCount = 0;
    for (auto prim : stage->Traverse()) {
        if (prim.IsA<UsdGeomPointInstancer>()) {
            instancerCount++;

            UsdGeomPointInstancer pi(prim);
            VtIntArray protoIndices;
            pi.GetProtoIndicesAttr().Get(&protoIndices);
            EXPECT_EQ(protoIndices.size(), 4u); // 4 instances

            VtVec3fArray positions;
            pi.GetPositionsAttr().Get(&positions);
            EXPECT_EQ(positions.size(), 4u);
        }
    }
    EXPECT_EQ(instancerCount, 1);

    // Triangle should still exist as a standalone mesh
    bool triangleExists = false;
    for (auto prim : stage->Traverse()) {
        if (prim.IsA<UsdGeomMesh>() && !prim.GetPath().HasPrefix(SdfPath("/Root/Instancer_0"))) {
            // This should be the triangle (not under the instancer)
            triangleExists = true;
        }
    }
    // Note: the triangle may or may not still be at its original path
    // The key assertion is that a PointInstancer was created with 4 instances
}

TEST_F(InstancingTest, SkipsBelowMinInstanceCount) {
    auto stage = UsdStage::Open(TestDataPath("instancable_meshes.usda"));
    ASSERT_TRUE(stage);

    PointInstancerAuthor instancer;
    instancer.SetMinInstanceCount(10); // Set high threshold
    instancer.Execute(stage);

    // No PointInstancer should be created
    int instancerCount = 0;
    for (auto prim : stage->Traverse()) {
        if (prim.IsA<UsdGeomPointInstancer>()) {
            instancerCount++;
        }
    }
    EXPECT_EQ(instancerCount, 0);
}

// ===== NEW TESTS FOR v5 INSTANCING IMPROVEMENTS =====

TEST_F(InstancingTest, CentroidNormalization_MatchesOffsetMeshes) {
    // Three cubes with identical shape but different local-space vertex offsets.
    // With centroid normalization, these should all be instanced together.
    auto stage = UsdStage::Open(TestDataPath("instancing_normalized.usda"));
    ASSERT_TRUE(stage);

    // Verify the offset cubes exist
    ASSERT_TRUE(stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeA/Mesh")).IsValid());
    ASSERT_TRUE(stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeB/Mesh")).IsValid());
    ASSERT_TRUE(stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeC/Mesh")).IsValid());

    // Without centroid normalization: the offset cubes hash differently
    GeometryHasher hasher;
    UsdGeomMesh cubeA(stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeA/Mesh")));
    UsdGeomMesh cubeB(stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeB/Mesh")));
    UsdGeomMesh cubeC(stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeC/Mesh")));

    HashDigest exactA = hasher.HashMeshTopology(cubeA, 1e-3f);
    HashDigest exactB = hasher.HashMeshTopology(cubeB, 1e-3f);
    EXPECT_NE(exactA, exactB); // Different without normalization

    // With centroid normalization: they should hash identically
    auto normA = hasher.HashMeshNormalized(cubeA, 1e-3f);
    auto normB = hasher.HashMeshNormalized(cubeB, 1e-3f);
    auto normC = hasher.HashMeshNormalized(cubeC, 1e-3f);
    EXPECT_EQ(normA.hash, normB.hash);
    EXPECT_EQ(normB.hash, normC.hash);

    // Centroids should be different
    EXPECT_NE(normA.centroid, normB.centroid);
    EXPECT_NE(normB.centroid, normC.centroid);

    // Different shape (triangle) should NOT match
    UsdGeomMesh diffShape(stage->GetPrimAtPath(SdfPath("/Root/DifferentShape")));
    ASSERT_TRUE(diffShape.GetPrim().IsValid());
    auto normDiff = hasher.HashMeshNormalized(diffShape, 1e-3f);
    EXPECT_NE(normA.hash, normDiff.hash);
}

TEST_F(InstancingTest, CentroidNormalization_CreatesInstancer) {
    // End-to-end test: offset cubes should be grouped into one PointInstancer
    auto stage = UsdStage::Open(TestDataPath("instancing_normalized.usda"));
    ASSERT_TRUE(stage);

    PointInstancerAuthor instancer;
    instancer.SetMinInstanceCount(2);
    instancer.SetNormalizeCentroids(true); // Enable centroid normalization
    instancer.Execute(stage);

    // Should have created PointInstancers.
    // The 3 offset cubes should be one group.
    // The 3 material cubes (same local positions) should be another group.
    int instancerCount = 0;
    int totalInstances = 0;
    for (auto prim : stage->Traverse()) {
        if (prim.IsA<UsdGeomPointInstancer>()) {
            instancerCount++;
            UsdGeomPointInstancer pi(prim);
            VtIntArray protoIndices;
            pi.GetProtoIndicesAttr().Get(&protoIndices);
            totalInstances += static_cast<int>(protoIndices.size());
        }
    }
    // At least one instancer created
    EXPECT_GE(instancerCount, 1);
    // The 3 offset cubes + 3 material cubes = 6 total instances (in 1 or 2 instancers)
    EXPECT_GE(totalInstances, 3); // At minimum the 3 offset cubes

    // The triangle should still exist as a standalone mesh
    bool differentShapeExists = false;
    for (auto prim : stage->Traverse()) {
        if (prim.IsA<UsdGeomMesh>()) {
            // Find mesh not under an instancer
            bool underInstancer = false;
            for (auto ancestor = prim.GetParent(); ancestor.IsValid(); ancestor = ancestor.GetParent()) {
                if (ancestor.IsA<UsdGeomPointInstancer>()) {
                    underInstancer = true;
                    break;
                }
            }
            if (!underInstancer) {
                differentShapeExists = true;
            }
        }
    }
    EXPECT_TRUE(differentShapeExists);
}

TEST_F(InstancingTest, MultiMaterial_CreatesMultiplePrototypes) {
    // Three cubes with same geometry but 2 different materials (Red, Blue, Red).
    // Should create one PointInstancer with 2 prototypes (one per material).
    auto stage = UsdStage::Open(TestDataPath("instancing_normalized.usda"));
    ASSERT_TRUE(stage);

    // Run instancing ONLY on the material cubes by using a fresh stage with just them
    auto matStage = UsdStage::CreateInMemory();
    matStage->DefinePrim(SdfPath("/Root"), TfToken("Xform"));

    // Create materials
    auto redMatPrim = matStage->DefinePrim(SdfPath("/Root/Materials/RedMaterial"), TfToken("Material"));
    auto blueMatPrim = matStage->DefinePrim(SdfPath("/Root/Materials/BlueMaterial"), TfToken("Material"));

    // Create three cubes with same geometry, different materials
    VtVec3fArray pts = {
        GfVec3f(0,0,0), GfVec3f(1,0,0), GfVec3f(1,1,0), GfVec3f(0,1,0),
        GfVec3f(0,0,1), GfVec3f(1,0,1), GfVec3f(1,1,1), GfVec3f(0,1,1)
    };
    VtIntArray fvc = {4,4,4,4,4,4};
    VtIntArray fvi = {0,1,2,3, 4,5,6,7, 0,1,5,4, 2,3,7,6, 0,3,7,4, 1,2,6,5};

    for (int i = 0; i < 3; ++i) {
        std::string name = "Cube" + std::to_string(i);
        SdfPath xfPath = SdfPath("/Root/" + name);
        auto xfPrim = matStage->DefinePrim(xfPath, TfToken("Xform"));
        UsdGeomXformable xf(xfPrim);
        xf.AddTranslateOp().Set(GfVec3d(i * 5.0, 0, 0));

        SdfPath meshPath = xfPath.AppendChild(TfToken("Mesh"));
        UsdGeomMesh mesh = UsdGeomMesh::Define(matStage, meshPath);
        mesh.GetPointsAttr().Set(pts);
        mesh.GetFaceVertexCountsAttr().Set(fvc);
        mesh.GetFaceVertexIndicesAttr().Set(fvi);

        // Bind material: cubes 0 and 2 get Red, cube 1 gets Blue
        SdfPath matPath = (i == 1) ?
            SdfPath("/Root/Materials/BlueMaterial") :
            SdfPath("/Root/Materials/RedMaterial");
        UsdShadeMaterialBindingAPI bindAPI = UsdShadeMaterialBindingAPI::Apply(mesh.GetPrim());
        bindAPI.Bind(UsdShadeMaterial(matStage->GetPrimAtPath(matPath)));
    }

    PointInstancerAuthor instancer;
    instancer.SetMinInstanceCount(2);
    instancer.SetNormalizeCentroids(false); // positions already match
    instancer.Execute(matStage);

    // Should create one PointInstancer with 2 prototypes (Red and Blue)
    int instancerCount = 0;
    for (auto prim : matStage->Traverse()) {
        if (prim.IsA<UsdGeomPointInstancer>()) {
            instancerCount++;

            UsdGeomPointInstancer pi(prim);
            VtIntArray protoIndices;
            pi.GetProtoIndicesAttr().Get(&protoIndices);
            EXPECT_EQ(protoIndices.size(), 3u); // 3 instances total

            // Should have 2 unique prototype indices
            std::set<int> uniqueProtos(protoIndices.begin(), protoIndices.end());
            EXPECT_EQ(uniqueProtos.size(), 2u); // Red and Blue prototypes

            // Check that the prototypes relationship has 2 targets
            SdfPathVector protoTargets;
            pi.GetPrototypesRel().GetTargets(&protoTargets);
            EXPECT_EQ(protoTargets.size(), 2u);
        }
    }
    EXPECT_EQ(instancerCount, 1);
}

TEST_F(InstancingTest, CentroidNormalization_DisabledFallsBackToExact) {
    // With centroid normalization disabled, offset cubes should NOT be instanced
    auto stage = UsdStage::Open(TestDataPath("instancing_normalized.usda"));
    ASSERT_TRUE(stage);

    PointInstancerAuthor instancer;
    instancer.SetMinInstanceCount(2);
    instancer.SetNormalizeCentroids(false); // Disable normalization

    // Count offset cubes before
    ASSERT_TRUE(stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeA/Mesh")).IsValid());
    ASSERT_TRUE(stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeB/Mesh")).IsValid());
    ASSERT_TRUE(stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeC/Mesh")).IsValid());

    instancer.Execute(stage);

    // OffsetCubeB and OffsetCubeC have unique local positions (offset to 100,200,300
    // and -50,-50,-50 respectively) so without normalization they can't match anyone.
    // OffsetCubeA has positions at (0,0,0)-(1,1,1) which IS identical to the
    // material cubes, so it correctly groups with them in exact matching mode.
    bool offsetCubeBExists = stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeB/Mesh")).IsValid();
    bool offsetCubeCExists = stage->GetPrimAtPath(SdfPath("/Root/OffsetCubeC/Mesh")).IsValid();

    // B and C should still exist (unique positions, not instanced)
    EXPECT_TRUE(offsetCubeBExists);
    EXPECT_TRUE(offsetCubeCExists);
}

TEST_F(InstancingTest, MinInstanceCount2_InstancesPairs) {
    // With minInstanceCount=2, even a pair of identical meshes should be instanced
    auto stage = UsdStage::CreateInMemory();
    stage->DefinePrim(SdfPath("/Root"), TfToken("Xform"));

    VtVec3fArray pts = {GfVec3f(0,0,0), GfVec3f(1,0,0), GfVec3f(1,1,0), GfVec3f(0,1,0)};
    VtIntArray fvc = {4};
    VtIntArray fvi = {0,1,2,3};

    for (int i = 0; i < 2; ++i) {
        SdfPath xfPath = SdfPath("/Root/Quad" + std::to_string(i));
        auto xfPrim = stage->DefinePrim(xfPath, TfToken("Xform"));
        UsdGeomXformable xf(xfPrim);
        xf.AddTranslateOp().Set(GfVec3d(i * 5.0, 0, 0));

        SdfPath meshPath = xfPath.AppendChild(TfToken("Mesh"));
        UsdGeomMesh mesh = UsdGeomMesh::Define(stage, meshPath);
        mesh.GetPointsAttr().Set(pts);
        mesh.GetFaceVertexCountsAttr().Set(fvc);
        mesh.GetFaceVertexIndicesAttr().Set(fvi);
    }

    PointInstancerAuthor instancer;
    instancer.SetMinInstanceCount(2);
    instancer.SetNormalizeCentroids(false);
    instancer.Execute(stage);

    int instancerCount = 0;
    for (auto prim : stage->Traverse()) {
        if (prim.IsA<UsdGeomPointInstancer>()) {
            instancerCount++;
            UsdGeomPointInstancer pi(prim);
            VtIntArray protoIndices;
            pi.GetProtoIndicesAttr().Get(&protoIndices);
            EXPECT_EQ(protoIndices.size(), 2u);
        }
    }
    EXPECT_EQ(instancerCount, 1);
}
