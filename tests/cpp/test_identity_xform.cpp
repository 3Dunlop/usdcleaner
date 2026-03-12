#include <gtest/gtest.h>
#include "core/metadata/IdentityXformStripper.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/base/gf/matrix4d.h>

using namespace usdcleaner;
PXR_NAMESPACE_USING_DIRECTIVE

class IdentityXformTest : public ::testing::Test {
protected:
    std::string TestDataPath(const std::string& filename) {
        return std::string(TEST_DATA_DIR) + "/" + filename;
    }
};

TEST_F(IdentityXformTest, ClearsIdentityTransforms) {
    auto stage = UsdStage::Open(TestDataPath("identity_xforms.usda"));
    ASSERT_TRUE(stage);

    // Verify identity xform exists before
    UsdPrim identityPrim = stage->GetPrimAtPath(SdfPath("/Root/IdentityXform"));
    ASSERT_TRUE(identityPrim.IsValid());
    UsdGeomXformable xformable(identityPrim);
    bool resetsStack = false;
    auto ops = xformable.GetOrderedXformOps(&resetsStack);
    EXPECT_FALSE(ops.empty());

    IdentityXformStripper stripper;
    stripper.Execute(stage);

    // Identity xform should have ops cleared
    ops = xformable.GetOrderedXformOps(&resetsStack);
    EXPECT_TRUE(ops.empty());
}

TEST_F(IdentityXformTest, PreservesNonIdentityTransforms) {
    auto stage = UsdStage::Open(TestDataPath("identity_xforms.usda"));
    ASSERT_TRUE(stage);

    IdentityXformStripper stripper;
    stripper.Execute(stage);

    // Translated xform should still have its transform
    UsdPrim translatedPrim = stage->GetPrimAtPath(SdfPath("/Root/TranslatedXform"));
    ASSERT_TRUE(translatedPrim.IsValid());
    UsdGeomXformable xformable(translatedPrim);
    bool resetsStack = false;
    auto ops = xformable.GetOrderedXformOps(&resetsStack);
    EXPECT_FALSE(ops.empty());

    // Verify the translation value is preserved
    GfMatrix4d localXform(1.0);
    xformable.GetLocalTransformation(&localXform, &resetsStack, UsdTimeCode::Default());
    EXPECT_NEAR(localXform[3][0], 5.0, 1e-6);
    EXPECT_NEAR(localXform[3][1], 0.0, 1e-6);
    EXPECT_NEAR(localXform[3][2], 3.0, 1e-6);
}

TEST_F(IdentityXformTest, ClearsZeroTranslation) {
    auto stage = UsdStage::Open(TestDataPath("identity_xforms.usda"));
    ASSERT_TRUE(stage);

    IdentityXformStripper stripper;
    stripper.Execute(stage);

    // Zero-translate xform should be cleared (it's identity)
    UsdPrim zeroPrim = stage->GetPrimAtPath(SdfPath("/Root/ZeroTranslate"));
    ASSERT_TRUE(zeroPrim.IsValid());
    UsdGeomXformable xformable(zeroPrim);
    bool resetsStack = false;
    auto ops = xformable.GetOrderedXformOps(&resetsStack);
    EXPECT_TRUE(ops.empty());
}

TEST_F(IdentityXformTest, IgnoresXformsWithoutOps) {
    auto stage = UsdStage::Open(TestDataPath("identity_xforms.usda"));
    ASSERT_TRUE(stage);

    // Count prims before
    size_t primCountBefore = 0;
    for (auto prim : stage->Traverse()) {
        (void)prim;
        primCountBefore++;
    }

    IdentityXformStripper stripper;
    stripper.Execute(stage);

    // Prim count should not change (stripper only clears ops, doesn't remove prims)
    size_t primCountAfter = 0;
    for (auto prim : stage->Traverse()) {
        (void)prim;
        primCountAfter++;
    }
    EXPECT_EQ(primCountBefore, primCountAfter);
}
