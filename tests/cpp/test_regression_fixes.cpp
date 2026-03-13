#include <gtest/gtest.h>

#include "core/metadata/MetadataStripper.h"
#include "core/metadata/IdentityXformStripper.h"
#include "core/hierarchy/HierarchyFlattener.h"
#include "core/topology/LaminaFaceRemover.h"
#include "core/topology/DegenerateFaceRemover.h"
#include "core/welding/VertexWelder.h"
#include "core/materials/MaterialHasher.h"
#include "core/materials/MaterialDeduplicator.h"
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
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
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

// =============================================================================
// Bug #13 Regression: MetadataStripper must preserve UsdShade material properties
// =============================================================================

TEST_F(RegressionTest, MetadataStripper_PreservesShadeProperties) {
    // Create a stage with a material that has interface inputs and shader
    // connections — the pattern used by FBX-exported USD files
    auto stage = UsdStage::CreateInMemory();

    // Create material with interface inputs
    auto material = UsdShadeMaterial::Define(stage,
        SdfPath("/Root/Materials/TestMaterial"));

    // Add interface inputs on the material prim (like FBX exporter does)
    auto baseColorInput = material.CreateInput(TfToken("baseColor"),
        SdfValueTypeNames->Color3f);
    baseColorInput.Set(GfVec3f(0.8f, 0.2f, 0.1f));

    // Add surface output connection
    auto surfaceOutput = material.CreateSurfaceOutput();

    // Create shader under material
    auto shader = UsdShadeShader::Define(stage,
        SdfPath("/Root/Materials/TestMaterial/Shader"));
    shader.CreateIdAttr(VtValue(TfToken("UsdPreviewSurface")));

    // Connect shader input to material interface input
    auto shaderDiffuse = shader.CreateInput(TfToken("diffuseColor"),
        SdfValueTypeNames->Color3f);
    shaderDiffuse.ConnectToSource(baseColorInput);

    // Create shader outputs
    auto shaderSurfaceOut = shader.CreateOutput(TfToken("surface"),
        SdfValueTypeNames->Token);
    surfaceOutput.ConnectToSource(shaderSurfaceOut);

    // Create a mesh bound to this material
    auto mesh = UsdGeomMesh::Define(stage, SdfPath("/Root/Mesh"));
    VtVec3fArray points = {GfVec3f(0,0,0), GfVec3f(1,0,0), GfVec3f(1,1,0)};
    VtIntArray counts = {3};
    VtIntArray indices = {0, 1, 2};
    mesh.GetPointsAttr().Set(points);
    mesh.GetFaceVertexCountsAttr().Set(counts);
    mesh.GetFaceVertexIndicesAttr().Set(indices);

    UsdShadeMaterialBindingAPI::Apply(mesh.GetPrim());
    UsdShadeMaterialBindingAPI(mesh.GetPrim()).Bind(material);

    // Run MetadataStripper
    MetadataStripper stripper;
    stripper.Execute(stage);

    // Verify material interface input preserved
    auto matPrim = stage->GetPrimAtPath(SdfPath("/Root/Materials/TestMaterial"));
    ASSERT_TRUE(matPrim.IsValid());
    UsdShadeMaterial matAfter(matPrim);
    auto baseColorAfter = matAfter.GetInput(TfToken("baseColor"));
    ASSERT_TRUE(baseColorAfter);
    GfVec3f colorVal;
    EXPECT_TRUE(baseColorAfter.Get(&colorVal))
        << "Material interface input 'baseColor' should be preserved";
    EXPECT_EQ(colorVal, GfVec3f(0.8f, 0.2f, 0.1f));

    // Verify surface output connection preserved
    auto surfOutAfter = matAfter.GetSurfaceOutput();
    EXPECT_TRUE(surfOutAfter.HasConnectedSource())
        << "Material surface output connection should be preserved";

    // Verify shader output preserved
    auto shaderPrim = stage->GetPrimAtPath(
        SdfPath("/Root/Materials/TestMaterial/Shader"));
    ASSERT_TRUE(shaderPrim.IsValid());
    UsdShadeShader shaderAfter(shaderPrim);
    auto shaderSurfOutAfter = shaderAfter.GetOutput(TfToken("surface"));
    EXPECT_TRUE(shaderSurfOutAfter)
        << "Shader 'outputs:surface' should be preserved";

    // Verify shader input connection preserved
    auto diffuseAfter = shaderAfter.GetInput(TfToken("diffuseColor"));
    EXPECT_TRUE(diffuseAfter)
        << "Shader 'inputs:diffuseColor' should be preserved";
    EXPECT_TRUE(diffuseAfter.HasConnectedSource())
        << "Shader input connection should be preserved";
}

// =============================================================================
// Bug #14 Regression: MaterialHasher must differentiate interface input values
// =============================================================================

TEST_F(RegressionTest, MaterialHasher_DifferentiatesInterfaceInputs) {
    // Create a stage with two materials that have the same shader structure
    // but different interface input values (like FBX-exported colors)
    auto stage = UsdStage::CreateInMemory();

    // Material A: red
    auto matA = UsdShadeMaterial::Define(stage, SdfPath("/Root/Materials/MatA"));
    matA.CreateInput(TfToken("baseColor"), SdfValueTypeNames->Color3f)
        .Set(GfVec3f(1.0f, 0.0f, 0.0f));
    auto shaderA = UsdShadeShader::Define(stage,
        SdfPath("/Root/Materials/MatA/Shader"));
    shaderA.CreateIdAttr(VtValue(TfToken("UsdPreviewSurface")));
    shaderA.CreateInput(TfToken("diffuseColor"), SdfValueTypeNames->Color3f)
        .ConnectToSource(matA.GetInput(TfToken("baseColor")));

    // Material B: blue (different color, same shader topology)
    auto matB = UsdShadeMaterial::Define(stage, SdfPath("/Root/Materials/MatB"));
    matB.CreateInput(TfToken("baseColor"), SdfValueTypeNames->Color3f)
        .Set(GfVec3f(0.0f, 0.0f, 1.0f));
    auto shaderB = UsdShadeShader::Define(stage,
        SdfPath("/Root/Materials/MatB/Shader"));
    shaderB.CreateIdAttr(VtValue(TfToken("UsdPreviewSurface")));
    shaderB.CreateInput(TfToken("diffuseColor"), SdfValueTypeNames->Color3f)
        .ConnectToSource(matB.GetInput(TfToken("baseColor")));

    // Material C: also red (duplicate of A)
    auto matC = UsdShadeMaterial::Define(stage, SdfPath("/Root/Materials/MatC"));
    matC.CreateInput(TfToken("baseColor"), SdfValueTypeNames->Color3f)
        .Set(GfVec3f(1.0f, 0.0f, 0.0f));
    auto shaderC = UsdShadeShader::Define(stage,
        SdfPath("/Root/Materials/MatC/Shader"));
    shaderC.CreateIdAttr(VtValue(TfToken("UsdPreviewSurface")));
    shaderC.CreateInput(TfToken("diffuseColor"), SdfValueTypeNames->Color3f)
        .ConnectToSource(matC.GetInput(TfToken("baseColor")));

    // Hash all three materials
    MaterialHasher hasher;
    HashDigest hashA = hasher.HashMaterial(matA);
    HashDigest hashB = hasher.HashMaterial(matB);
    HashDigest hashC = hasher.HashMaterial(matC);

    // A and C should hash identically (same color, same structure)
    EXPECT_EQ(hashA, hashC)
        << "Materials with identical interface inputs should hash the same";

    // A and B should hash DIFFERENTLY (different colors)
    EXPECT_NE(hashA, hashB)
        << "Materials with different interface input values must hash differently";
}

// =============================================================================
// Bug #13+14 Combined: MaterialDeduplicator preserves distinct materials
// =============================================================================

TEST_F(RegressionTest, MaterialDeduplicator_PreservesDistinctMaterials) {
    auto stage = UsdStage::CreateInMemory();

    // Create two materials with different colors
    auto matRed = UsdShadeMaterial::Define(stage, SdfPath("/Root/Materials/Red"));
    matRed.CreateInput(TfToken("baseColor"), SdfValueTypeNames->Color3f)
        .Set(GfVec3f(1.0f, 0.0f, 0.0f));
    auto shaderRed = UsdShadeShader::Define(stage,
        SdfPath("/Root/Materials/Red/Shader"));
    shaderRed.CreateIdAttr(VtValue(TfToken("UsdPreviewSurface")));
    shaderRed.CreateInput(TfToken("diffuseColor"), SdfValueTypeNames->Color3f)
        .ConnectToSource(matRed.GetInput(TfToken("baseColor")));

    auto matBlue = UsdShadeMaterial::Define(stage, SdfPath("/Root/Materials/Blue"));
    matBlue.CreateInput(TfToken("baseColor"), SdfValueTypeNames->Color3f)
        .Set(GfVec3f(0.0f, 0.0f, 1.0f));
    auto shaderBlue = UsdShadeShader::Define(stage,
        SdfPath("/Root/Materials/Blue/Shader"));
    shaderBlue.CreateIdAttr(VtValue(TfToken("UsdPreviewSurface")));
    shaderBlue.CreateInput(TfToken("diffuseColor"), SdfValueTypeNames->Color3f)
        .ConnectToSource(matBlue.GetInput(TfToken("baseColor")));

    // Create two meshes, each bound to a different material
    auto meshA = UsdGeomMesh::Define(stage, SdfPath("/Root/MeshA"));
    meshA.GetPointsAttr().Set(VtVec3fArray{GfVec3f(0,0,0), GfVec3f(1,0,0), GfVec3f(1,1,0)});
    meshA.GetFaceVertexCountsAttr().Set(VtIntArray{3});
    meshA.GetFaceVertexIndicesAttr().Set(VtIntArray{0, 1, 2});
    UsdShadeMaterialBindingAPI::Apply(meshA.GetPrim());
    UsdShadeMaterialBindingAPI(meshA.GetPrim()).Bind(matRed);

    auto meshB = UsdGeomMesh::Define(stage, SdfPath("/Root/MeshB"));
    meshB.GetPointsAttr().Set(VtVec3fArray{GfVec3f(2,0,0), GfVec3f(3,0,0), GfVec3f(3,1,0)});
    meshB.GetFaceVertexCountsAttr().Set(VtIntArray{3});
    meshB.GetFaceVertexIndicesAttr().Set(VtIntArray{0, 1, 2});
    UsdShadeMaterialBindingAPI::Apply(meshB.GetPrim());
    UsdShadeMaterialBindingAPI(meshB.GetPrim()).Bind(matBlue);

    // Run MaterialDeduplicator
    MaterialDeduplicator dedup;
    dedup.Execute(stage);

    // Both materials should still exist (different colors!)
    EXPECT_TRUE(stage->GetPrimAtPath(SdfPath("/Root/Materials/Red")).IsValid())
        << "Red material should not be pruned";
    EXPECT_TRUE(stage->GetPrimAtPath(SdfPath("/Root/Materials/Blue")).IsValid())
        << "Blue material should not be pruned";

    // Mesh bindings should be unchanged
    UsdShadeMaterialBindingAPI bindA(stage->GetPrimAtPath(SdfPath("/Root/MeshA")));
    auto boundA = bindA.ComputeBoundMaterial();
    EXPECT_EQ(boundA.GetPrim().GetPath(), SdfPath("/Root/Materials/Red"));

    UsdShadeMaterialBindingAPI bindB(stage->GetPrimAtPath(SdfPath("/Root/MeshB")));
    auto boundB = bindB.ComputeBoundMaterial();
    EXPECT_EQ(boundB.GetPrim().GetPath(), SdfPath("/Root/Materials/Blue"));
}
