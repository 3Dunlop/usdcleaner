#include <gtest/gtest.h>
#include "core/welding/VertexWelder.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>

using namespace usdcleaner;
PXR_NAMESPACE_USING_DIRECTIVE

class VertexWelderTest : public ::testing::Test {
protected:
    std::string TestDataPath(const std::string& filename) {
        return std::string(TEST_DATA_DIR) + "/" + filename;
    }
};

TEST_F(VertexWelderTest, CubeWeldingReducesTo8Vertices) {
    auto stage = UsdStage::Open(TestDataPath("cube_welding.usda"));
    ASSERT_TRUE(stage);

    VertexWelder welder;
    welder.SetAutoEpsilon(false);
    welder.SetEpsilon(1e-5f);
    welder.Execute(stage);

    UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/Cube")));
    ASSERT_TRUE(mesh);

    VtVec3fArray points;
    mesh.GetPointsAttr().Get(&points);
    EXPECT_EQ(points.size(), 8u);
}

TEST_F(VertexWelderTest, FaceCountPreservedAfterWelding) {
    auto stage = UsdStage::Open(TestDataPath("cube_welding.usda"));
    ASSERT_TRUE(stage);

    VertexWelder welder;
    welder.SetAutoEpsilon(false);
    welder.SetEpsilon(1e-5f);
    welder.Execute(stage);

    UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/Cube")));
    VtIntArray faceVertexCounts;
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);

    // 6 faces should still exist
    EXPECT_EQ(faceVertexCounts.size(), 6u);
}

TEST_F(VertexWelderTest, IndicesValidAfterWelding) {
    auto stage = UsdStage::Open(TestDataPath("cube_welding.usda"));
    ASSERT_TRUE(stage);

    VertexWelder welder;
    welder.SetAutoEpsilon(false);
    welder.SetEpsilon(1e-5f);
    welder.Execute(stage);

    UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/Cube")));
    VtVec3fArray points;
    VtIntArray indices;
    mesh.GetPointsAttr().Get(&points);
    mesh.GetFaceVertexIndicesAttr().Get(&indices);

    // All indices should be in valid range
    for (int idx : indices) {
        EXPECT_GE(idx, 0);
        EXPECT_LT(static_cast<size_t>(idx), points.size());
    }
}
