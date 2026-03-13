#include <gtest/gtest.h>

#include "core/metadata/MetadataStripper.h"
#include "core/metadata/IdentityXformStripper.h"
#include "core/hierarchy/HierarchyFlattener.h"
#include "core/topology/LaminaFaceRemover.h"
#include "core/topology/DegenerateFaceRemover.h"
#include "core/welding/VertexWelder.h"
#include "core/common/UsdUtils.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/usd/sdf/schema.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/base/vt/dictionary.h>

using namespace usdcleaner;
PXR_NAMESPACE_USING_DIRECTIVE

class RegressionTest : public ::testing::Test {
protected:
    std::string TestDataPath(const std::string& filename) {
        return std::string(TEST_DATA_DIR) + "/" + filename;
    }

    bool HasAuthoredCustomData(const UsdStageRefPtr& stage, const SdfPath& path) {
        SdfLayerHandle layer = stage->GetRootLayer();
        SdfPrimSpecHandle primSpec = layer->GetPrimAtPath(path);
        if (!primSpec) return false;
        return primSpec->HasInfo(SdfFieldKeys->CustomData);
    }

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

    VtDictionary GetAuthoredCustomData(const UsdStageRefPtr& stage,
                                        const SdfPath& path) {
        SdfLayerHandle layer = stage->GetRootLayer();
        SdfPrimSpecHandle primSpec = layer->GetPrimAtPath(path);
        if (!primSpec || !primSpec->HasInfo(SdfFieldKeys->CustomData)) {
            return VtDictionary();
        }
        return primSpec->GetCustomData();
    }
};

// =============================================================================
// Bug #4 Regression: MetadataStripper preserves non-userDocBrief customData
// =============================================================================

TEST_F(RegressionTest, MetadataStripper_PreservesOtherCustomDataKeys) {
    auto stage = UsdStage::Open(TestDataPath("customdata_mixed.usda"));
    ASSERT_TRUE(stage);

    SdfPath mixedPath("/Root/MixedCustomData");
    SdfPath onlyDocPath("/Root/OnlyUserDocBrief");
    SdfPath noDocPath("/Root/NoUserDocBrief");

    // Verify initial state
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, mixedPath, "userDocBrief"));
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, mixedPath, "bimCategory"));
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, mixedPath, "bimElementId"));
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, mixedPath, "author"));
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, onlyDocPath, "userDocBrief"));
    EXPECT_FALSE(HasAuthoredCustomDataKey(stage, noDocPath, "userDocBrief"));
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, noDocPath, "bimCategory"));

    MetadataStripper stripper;
    stripper.Execute(stage);

    // MixedCustomData: userDocBrief gone, other keys preserved
    EXPECT_FALSE(HasAuthoredCustomDataKey(stage, mixedPath, "userDocBrief"));
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, mixedPath, "bimCategory"));
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, mixedPath, "bimElementId"));
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, mixedPath, "author"));

    // Verify the values are correct
    VtDictionary cd = GetAuthoredCustomData(stage, mixedPath);
    EXPECT_EQ(cd["bimCategory"], VtValue(std::string("Walls")));
    EXPECT_EQ(cd["bimElementId"], VtValue(12345));

    // OnlyUserDocBrief: entire customData field should be cleared
    EXPECT_FALSE(HasAuthoredCustomData(stage, onlyDocPath));

    // NoUserDocBrief: should be completely untouched
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, noDocPath, "bimCategory"));
    EXPECT_TRUE(HasAuthoredCustomDataKey(stage, noDocPath, "bimElementId"));
}

// =============================================================================
// Bug #5 Regression: Time-sampled xforms preserved by IdentityXformStripper
// =============================================================================

TEST_F(RegressionTest, IdentityXformStripper_PreservesTimeSampledXforms) {
    auto stage = UsdStage::Open(TestDataPath("time_sampled_xforms.usda"));
    ASSERT_TRUE(stage);

    SdfPath rootPath("/Root");
    SdfPath staticPath("/Root/StaticIdentity");

    // Root has time-sampled xformOps
    UsdPrim rootPrim = stage->GetPrimAtPath(rootPath);
    UsdGeomXformable rootXformable(rootPrim);
    bool resetsStack = false;
    auto rootOps = rootXformable.GetOrderedXformOps(&resetsStack);
    ASSERT_FALSE(rootOps.empty());
    EXPECT_GT(rootOps[0].GetAttr().GetNumTimeSamples(), 0u);

    // StaticIdentity has static identity xformOps
    UsdPrim staticPrim = stage->GetPrimAtPath(staticPath);
    UsdGeomXformable staticXformable(staticPrim);
    auto staticOps = staticXformable.GetOrderedXformOps(&resetsStack);
    ASSERT_FALSE(staticOps.empty());

    IdentityXformStripper stripper;
    stripper.Execute(stage);

    // Root's time-sampled xform should be PRESERVED
    rootPrim = stage->GetPrimAtPath(rootPath);
    UsdGeomXformable rootXformableAfter(rootPrim);
    auto rootOpsAfter = rootXformableAfter.GetOrderedXformOps(&resetsStack);
    EXPECT_FALSE(rootOpsAfter.empty())
        << "Time-sampled xformOps should NOT be stripped";

    // StaticIdentity's static identity xform should be REMOVED
    staticPrim = stage->GetPrimAtPath(staticPath);
    UsdGeomXformable staticXformableAfter(staticPrim);
    auto staticOpsAfter = staticXformableAfter.GetOrderedXformOps(&resetsStack);
    EXPECT_TRUE(staticOpsAfter.empty())
        << "Static identity xformOps SHOULD be stripped";
}

// =============================================================================
// Bug #5 Regression: HierarchyFlattener preserves animated intermediate Xforms
// =============================================================================

TEST_F(RegressionTest, HierarchyFlattener_PreservesTimeSampledXforms) {
    auto stage = UsdStage::Open(TestDataPath("time_sampled_xforms.usda"));
    ASSERT_TRUE(stage);

    SdfPath animatedParent("/Root/AnimatedParent");

    // AnimatedParent has time-sampled xform and one child — normally a
    // flattening candidate, but should be preserved because of animation
    UsdPrim animPrim = stage->GetPrimAtPath(animatedParent);
    ASSERT_TRUE(animPrim.IsValid());

    HierarchyFlattener flattener;
    flattener.Execute(stage);

    // AnimatedParent should still exist (not flattened)
    animPrim = stage->GetPrimAtPath(animatedParent);
    EXPECT_TRUE(animPrim.IsValid())
        << "Animated intermediate Xform should NOT be flattened";

    // Its child should still be under it
    UsdPrim child = stage->GetPrimAtPath(SdfPath("/Root/AnimatedParent/Child"));
    EXPECT_TRUE(child.IsValid());
}

// =============================================================================
// Bug #8 Regression: LaminaFaceRemover with secondary comparison
// =============================================================================

TEST_F(RegressionTest, LaminaFaceRemover_NoFalsePositivesFromHashCollision) {
    // Create an in-memory stage with faces that are all unique
    // (no actual duplicates). The old hash-only approach might have
    // false positives, but the new secondary comparison should not.
    auto stage = UsdStage::CreateInMemory();
    auto mesh = UsdGeomMesh::Define(stage, SdfPath("/TestMesh"));

    // Create 100 unique triangular faces
    VtVec3fArray points;
    VtIntArray counts;
    VtIntArray indices;
    for (int i = 0; i < 300; ++i) {
        points.push_back(GfVec3f(static_cast<float>(i), 0.0f, 0.0f));
    }
    for (int i = 0; i < 100; ++i) {
        counts.push_back(3);
        indices.push_back(i * 3);
        indices.push_back(i * 3 + 1);
        indices.push_back(i * 3 + 2);
    }

    mesh.GetPointsAttr().Set(points);
    mesh.GetFaceVertexCountsAttr().Set(counts);
    mesh.GetFaceVertexIndicesAttr().Set(indices);

    LaminaFaceRemover remover;
    remover.Execute(stage);

    // All 100 faces should be preserved (none are actual duplicates)
    VtIntArray resultCounts;
    mesh.GetFaceVertexCountsAttr().Get(&resultCounts);
    EXPECT_EQ(resultCounts.size(), 100u)
        << "No faces should be removed — all are unique";
}

// =============================================================================
// Bug #9 Regression: faceVarying primvars compacted during face removal
// =============================================================================

TEST_F(RegressionTest, RemoveFaces_CompactsFaceVaryingPrimvars) {
    auto stage = UsdStage::Open(TestDataPath("facevarying_primvars.usda"));
    ASSERT_TRUE(stage);

    UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/Root/MeshWithFVPrimvars")));
    ASSERT_TRUE(mesh);

    // Check initial state: 4 faces, 12 faceVarying UV entries
    VtIntArray counts;
    mesh.GetFaceVertexCountsAttr().Get(&counts);
    ASSERT_EQ(counts.size(), 4u);

    UsdGeomPrimvarsAPI primvarsAPI(mesh);
    UsdGeomPrimvar stPrimvar = primvarsAPI.GetPrimvar(TfToken("st"));
    ASSERT_TRUE(stPrimvar);
    ASSERT_EQ(stPrimvar.GetInterpolation(), UsdGeomTokens->faceVarying);

    VtVec2fArray stBefore;
    stPrimvar.Get(&stBefore);
    ASSERT_EQ(stBefore.size(), 12u); // 4 faces * 3 verts each

    // The last face (indices [0,1,2]) is a duplicate of the first face.
    // LaminaFaceRemover should remove it.
    LaminaFaceRemover remover;
    remover.Execute(stage);

    // After removal: should have 3 faces
    mesh.GetFaceVertexCountsAttr().Get(&counts);
    EXPECT_EQ(counts.size(), 3u);

    // faceVarying UVs should be compacted to 9 entries (3 faces * 3 verts)
    VtVec2fArray stAfter;
    stPrimvar.Get(&stAfter);
    EXPECT_EQ(stAfter.size(), 9u)
        << "faceVarying primvar should be compacted when faces are removed";

    // Verify the faceVarying normals are also compacted
    UsdGeomPrimvar normalsPrimvar = primvarsAPI.GetPrimvar(TfToken("normals"));
    if (normalsPrimvar) {
        VtVec3fArray normalsAfter;
        normalsPrimvar.Get(&normalsAfter);
        EXPECT_EQ(normalsAfter.size(), 9u)
            << "faceVarying normals should also be compacted";
    }
}

// =============================================================================
// Bug #3 Regression: VertexWelder bounds validation (in-memory test)
// =============================================================================

TEST_F(RegressionTest, VertexWelder_HandlesOutOfBoundsIndices) {
    // Create an in-memory stage with out-of-bounds indices
    auto stage = UsdStage::CreateInMemory();
    auto mesh = UsdGeomMesh::Define(stage, SdfPath("/TestMesh"));

    VtVec3fArray points = {
        GfVec3f(0, 0, 0), GfVec3f(1, 0, 0), GfVec3f(1, 1, 0)
    };
    VtIntArray counts = {3};
    // Index 5 is out of bounds for a 3-element points array
    VtIntArray indices = {0, 1, 5};

    mesh.GetPointsAttr().Set(points);
    mesh.GetFaceVertexCountsAttr().Set(counts);
    mesh.GetFaceVertexIndicesAttr().Set(indices);

    // VertexWelder should not crash on out-of-bounds indices
    VertexWelder welder;
    welder.SetEpsilon(0.01f);
    EXPECT_NO_THROW(welder.Execute(stage));

    // Mesh should still be valid
    UsdGeomMesh resultMesh(stage->GetPrimAtPath(SdfPath("/TestMesh")));
    EXPECT_TRUE(resultMesh.GetPrim().IsValid());
}

// =============================================================================
// MetadataStripper: Time-sampled attributes not removed as "None-valued"
// =============================================================================

TEST_F(RegressionTest, MetadataStripper_PreservesTimeSampledAttributes) {
    // Create a stage with an attribute that has no default value but has time samples
    auto stage = UsdStage::CreateInMemory();
    auto mesh = UsdGeomMesh::Define(stage, SdfPath("/TestMesh"));

    VtVec3fArray points = {GfVec3f(0, 0, 0), GfVec3f(1, 0, 0), GfVec3f(1, 1, 0)};
    VtIntArray counts = {3};
    VtIntArray indices = {0, 1, 2};
    mesh.GetPointsAttr().Set(points);
    mesh.GetFaceVertexCountsAttr().Set(counts);
    mesh.GetFaceVertexIndicesAttr().Set(indices);

    // Create a custom attribute with time samples but no default value
    UsdAttribute customAttr = mesh.GetPrim().CreateAttribute(
        TfToken("testAttr"), SdfValueTypeNames->Float);
    customAttr.Set(1.0f, UsdTimeCode(0));
    customAttr.Set(2.0f, UsdTimeCode(10));
    customAttr.ClearDefault();

    // Verify: no default value, but has time samples
    VtValue val;
    EXPECT_FALSE(customAttr.Get(&val, UsdTimeCode::Default()));
    EXPECT_GT(customAttr.GetNumTimeSamples(), 0u);

    MetadataStripper stripper;
    stripper.Execute(stage);

    // The time-sampled attribute should still exist
    UsdAttribute attrAfter = stage->GetPrimAtPath(SdfPath("/TestMesh"))
                                  .GetAttribute(TfToken("testAttr"));
    EXPECT_TRUE(attrAfter.IsAuthored())
        << "Time-sampled attribute should NOT be removed as 'None-valued'";
    EXPECT_GT(attrAfter.GetNumTimeSamples(), 0u);
}
