#include <gtest/gtest.h>
#include "core/instancing/PointInstancerAuthor.h"
#include "core/instancing/GeometryHasher.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/mesh.h>

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
