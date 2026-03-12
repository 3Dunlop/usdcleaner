#include <gtest/gtest.h>
#include "core/hierarchy/HierarchyFlattener.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/gf/matrix4d.h>

using namespace usdcleaner;
PXR_NAMESPACE_USING_DIRECTIVE

class HierarchyTest : public ::testing::Test {
protected:
    std::string TestDataPath(const std::string& filename) {
        return std::string(TEST_DATA_DIR) + "/" + filename;
    }
};

TEST_F(HierarchyTest, FlattensSingleChildChains) {
    auto stage = UsdStage::Open(TestDataPath("deep_hierarchy.usda"));
    ASSERT_TRUE(stage);

    // Verify deep hierarchy exists before
    UsdPrim b = stage->GetPrimAtPath(SdfPath("/Root/A/B"));
    UsdPrim c = stage->GetPrimAtPath(SdfPath("/Root/A/B/C"));
    EXPECT_TRUE(b.IsValid());
    EXPECT_TRUE(c.IsValid());

    HierarchyFlattener flattener;
    flattener.Execute(stage);

    // After flattening, B and C should be removed
    // The Cube mesh should have been reparented up the chain
    b = stage->GetPrimAtPath(SdfPath("/Root/A/B"));
    c = stage->GetPrimAtPath(SdfPath("/Root/A/B/C"));
    EXPECT_FALSE(b.IsValid() && c.IsValid());
    // At least one intermediate should be gone
}

TEST_F(HierarchyTest, PreservesMultiChildXforms) {
    auto stage = UsdStage::Open(TestDataPath("deep_hierarchy.usda"));
    ASSERT_TRUE(stage);

    HierarchyFlattener flattener;
    flattener.Execute(stage);

    // MultiChild Xform should still exist (has 2 children)
    UsdPrim multiChild = stage->GetPrimAtPath(SdfPath("/Root/MultiChild"));
    EXPECT_TRUE(multiChild.IsValid());

    // Its children should still be there
    UsdPrim meshA = stage->GetPrimAtPath(SdfPath("/Root/MultiChild/MeshA"));
    UsdPrim meshB = stage->GetPrimAtPath(SdfPath("/Root/MultiChild/MeshB"));
    EXPECT_TRUE(meshA.IsValid());
    EXPECT_TRUE(meshB.IsValid());
}

TEST_F(HierarchyTest, PreservesXformsWithMaterialBinding) {
    auto stage = UsdStage::Open(TestDataPath("deep_hierarchy.usda"));
    ASSERT_TRUE(stage);

    HierarchyFlattener flattener;
    flattener.Execute(stage);

    // WithBinding Xform should still exist (has material binding)
    UsdPrim withBinding = stage->GetPrimAtPath(SdfPath("/Root/WithBinding"));
    EXPECT_TRUE(withBinding.IsValid());
}

TEST_F(HierarchyTest, PreservesProtectedPatterns) {
    auto stage = UsdStage::Open(TestDataPath("deep_hierarchy.usda"));
    ASSERT_TRUE(stage);

    HierarchyFlattener flattener;
    flattener.SetPreservePatterns({"A"});
    flattener.Execute(stage);

    // "A" matches the pattern, so it should be preserved
    UsdPrim a = stage->GetPrimAtPath(SdfPath("/Root/A"));
    EXPECT_TRUE(a.IsValid());
}
