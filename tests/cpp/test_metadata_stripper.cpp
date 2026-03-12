#include <gtest/gtest.h>
#include "core/metadata/MetadataStripper.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/usd/sdf/schema.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/tf/token.h>

using namespace usdcleaner;
PXR_NAMESPACE_USING_DIRECTIVE

class MetadataStripperTest : public ::testing::Test {
protected:
    std::string TestDataPath(const std::string& filename) {
        return std::string(TEST_DATA_DIR) + "/" + filename;
    }

    // Check if a prim has AUTHORED customData at the Sdf layer level.
    // We must check Sdf layer because UsdPrim::HasCustomDataKey() returns
    // composed values that include USD schema defaults (e.g., UsdGeomMesh
    // provides a default userDocBrief from its schema definition).
    bool HasAuthoredCustomDataKey(const UsdStageRefPtr& stage,
                                  const SdfPath& path,
                                  const std::string& key) {
        SdfLayerHandle layer = stage->GetRootLayer();
        SdfPrimSpecHandle primSpec = layer->GetPrimAtPath(path);
        if (!primSpec || !primSpec->HasInfo(SdfFieldKeys->CustomData)) {
            return false;
        }
        SdfDictionaryProxy cd = primSpec->GetCustomData();
        return cd.count(key) > 0;
    }
};

TEST_F(MetadataStripperTest, RemovesCustomData) {
    auto stage = UsdStage::Open(TestDataPath("metadata_bloat.usda"));
    ASSERT_TRUE(stage);

    // Verify authored customData exists before stripping (Sdf-level check)
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, SdfPath("/Root"), "userDocBrief"));
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, SdfPath("/Root/Building/Wall"), "userDocBrief"));

    // Prims without customData should not have it
    EXPECT_FALSE(HasAuthoredCustomDataKey(stage, SdfPath("/Root/NoCustomData"), "userDocBrief"));

    MetadataStripper stripper;
    stripper.Execute(stage);

    // Verify authored customData is gone (Sdf-level check)
    EXPECT_FALSE(HasAuthoredCustomDataKey(stage, SdfPath("/Root"), "userDocBrief"));
    EXPECT_FALSE(HasAuthoredCustomDataKey(stage, SdfPath("/Root/Building"), "userDocBrief"));
    EXPECT_FALSE(HasAuthoredCustomDataKey(stage, SdfPath("/Root/Building/Wall"), "userDocBrief"));
    EXPECT_FALSE(HasAuthoredCustomDataKey(stage, SdfPath("/Root/Building/Floor"), "userDocBrief"));

    // Verify prims without customData are unaffected
    UsdPrim noCustomData = stage->GetPrimAtPath(SdfPath("/Root/NoCustomData"));
    EXPECT_TRUE(noCustomData.IsValid());
}

TEST_F(MetadataStripperTest, RemovesEmptyArrays) {
    auto stage = UsdStage::Open(TestDataPath("metadata_bloat.usda"));
    ASSERT_TRUE(stage);

    UsdPrim wall = stage->GetPrimAtPath(SdfPath("/Root/Building/Wall"));
    ASSERT_TRUE(wall.IsValid());

    // Verify empty arrays exist before stripping
    UsdAttribute cornerIndices = wall.GetAttribute(TfToken("cornerIndices"));
    EXPECT_TRUE(cornerIndices.IsAuthored());

    MetadataStripper stripper;
    stripper.Execute(stage);

    // After stripping, empty arrays should be removed
    cornerIndices = wall.GetAttribute(TfToken("cornerIndices"));
    EXPECT_FALSE(cornerIndices.IsAuthored());

    // Non-empty arrays (points, faceVertexIndices) should still exist
    UsdGeomMesh mesh(wall);
    VtVec3fArray points;
    mesh.GetPointsAttr().Get(&points);
    EXPECT_EQ(points.size(), 4u);
}

TEST_F(MetadataStripperTest, RemovesRedundantSubdivDefaults) {
    auto stage = UsdStage::Open(TestDataPath("metadata_bloat.usda"));
    ASSERT_TRUE(stage);

    UsdPrim wall = stage->GetPrimAtPath(SdfPath("/Root/Building/Wall"));
    UsdGeomMesh mesh(wall);

    // Verify subdivisionScheme is authored before
    EXPECT_TRUE(mesh.GetSubdivisionSchemeAttr().IsAuthored());

    MetadataStripper stripper;
    stripper.Execute(stage);

    // subdivisionScheme = "none" should be removed (it's the default)
    EXPECT_FALSE(mesh.GetSubdivisionSchemeAttr().IsAuthored());
}

TEST_F(MetadataStripperTest, PreservesNonDefaultValues) {
    auto stage = UsdStage::Open(TestDataPath("metadata_bloat.usda"));
    ASSERT_TRUE(stage);

    MetadataStripper stripper;
    stripper.Execute(stage);

    // Mesh geometry should be preserved
    UsdPrim wall = stage->GetPrimAtPath(SdfPath("/Root/Building/Wall"));
    UsdGeomMesh mesh(wall);
    VtVec3fArray points;
    mesh.GetPointsAttr().Get(&points);
    EXPECT_EQ(points.size(), 4u);

    VtIntArray faceVertexCounts;
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
    EXPECT_EQ(faceVertexCounts.size(), 1u);
}
