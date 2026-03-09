#include <gtest/gtest.h>
#include "core/topology/LaminaFaceRemover.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>

using namespace usdcleaner;
PXR_NAMESPACE_USING_DIRECTIVE

class LaminaRemovalTest : public ::testing::Test {
protected:
    std::string TestDataPath(const std::string& filename) {
        return std::string(TEST_DATA_DIR) + "/" + filename;
    }
};

TEST_F(LaminaRemovalTest, RemovesDuplicateFaces) {
    auto stage = UsdStage::Open(TestDataPath("lamina_faces.usda"));
    ASSERT_TRUE(stage);

    LaminaFaceRemover remover;
    remover.Execute(stage);

    UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/MeshWithDuplicates")));
    ASSERT_TRUE(mesh);

    VtIntArray faceVertexCounts;
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);

    // Should have 2 unique faces (duplicates removed)
    EXPECT_EQ(faceVertexCounts.size(), 2u);
}
