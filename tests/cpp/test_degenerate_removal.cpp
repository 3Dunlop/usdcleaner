#include <gtest/gtest.h>
#include "core/topology/DegenerateFaceRemover.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>

using namespace usdcleaner;
PXR_NAMESPACE_USING_DIRECTIVE

class DegenerateRemovalTest : public ::testing::Test {
protected:
    std::string TestDataPath(const std::string& filename) {
        return std::string(TEST_DATA_DIR) + "/" + filename;
    }
};

TEST_F(DegenerateRemovalTest, RemovesDegenerateFaces) {
    auto stage = UsdStage::Open(TestDataPath("degenerate_faces.usda"));
    ASSERT_TRUE(stage);

    DegenerateFaceRemover remover;
    remover.Execute(stage);

    UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/MeshWithDegenerates")));
    ASSERT_TRUE(mesh);

    VtIntArray faceVertexCounts;
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);

    // Should have 2 valid faces remaining (out of original 4)
    EXPECT_EQ(faceVertexCounts.size(), 2u);
}

TEST_F(DegenerateRemovalTest, ValidFacesPreserved) {
    auto stage = UsdStage::Open(TestDataPath("degenerate_faces.usda"));
    ASSERT_TRUE(stage);

    DegenerateFaceRemover remover;
    remover.Execute(stage);

    UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/MeshWithDegenerates")));
    VtIntArray faceVertexIndices;
    mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);

    // 2 triangles * 3 vertices = 6 indices
    EXPECT_EQ(faceVertexIndices.size(), 6u);
}
