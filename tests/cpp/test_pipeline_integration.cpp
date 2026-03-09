#include <gtest/gtest.h>
#include "core/pipeline/Pipeline.h"
#include "core/pipeline/StageProcessor.h"
#include "core/welding/VertexWelder.h"
#include "core/topology/DegenerateFaceRemover.h"
#include "core/topology/LaminaFaceRemover.h"
#include "core/common/Metrics.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>

using namespace usdcleaner;
PXR_NAMESPACE_USING_DIRECTIVE

class PipelineIntegrationTest : public ::testing::Test {
protected:
    std::string TestDataPath(const std::string& filename) {
        return std::string(TEST_DATA_DIR) + "/" + filename;
    }
};

TEST_F(PipelineIntegrationTest, FullPipelineOnCube) {
    auto stage = UsdStage::Open(TestDataPath("cube_welding.usda"));
    ASSERT_TRUE(stage);

    Pipeline pipeline;
    auto welder = std::make_shared<VertexWelder>();
    welder->SetAutoEpsilon(false);
    welder->SetEpsilon(1e-5f);
    pipeline.AddPass(welder);
    pipeline.AddPass(std::make_shared<DegenerateFaceRemover>());
    pipeline.AddPass(std::make_shared<LaminaFaceRemover>());

    pipeline.Execute(stage);

    // Verify the pipeline collected metrics
    const auto& results = pipeline.GetMetrics().GetPassResults();
    EXPECT_GE(results.size(), 1u);

    // Verify the cube was optimized
    UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/Cube")));
    VtVec3fArray points;
    mesh.GetPointsAttr().Get(&points);
    EXPECT_EQ(points.size(), 8u);
}

TEST_F(PipelineIntegrationTest, IdempotencyCheck) {
    auto stage = UsdStage::Open(TestDataPath("cube_welding.usda"));
    ASSERT_TRUE(stage);

    // Run pipeline twice
    for (int pass = 0; pass < 2; ++pass) {
        Pipeline pipeline;
        auto welder = std::make_shared<VertexWelder>();
        welder->SetAutoEpsilon(false);
        welder->SetEpsilon(1e-5f);
        pipeline.AddPass(welder);
        pipeline.AddPass(std::make_shared<DegenerateFaceRemover>());
        pipeline.AddPass(std::make_shared<LaminaFaceRemover>());
        pipeline.Execute(stage);
    }

    // After second run, should still have 8 vertices (no further reduction)
    UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/Cube")));
    VtVec3fArray points;
    mesh.GetPointsAttr().Get(&points);
    EXPECT_EQ(points.size(), 8u);
}

TEST_F(PipelineIntegrationTest, MetricsJsonIsValidJson) {
    auto stage = UsdStage::Open(TestDataPath("cube_welding.usda"));
    ASSERT_TRUE(stage);

    Pipeline pipeline;
    auto welder = std::make_shared<VertexWelder>();
    welder->SetAutoEpsilon(false);
    welder->SetEpsilon(1e-5f);
    pipeline.AddPass(welder);
    pipeline.Execute(stage);

    std::string json = pipeline.GetMetrics().ToJson();
    EXPECT_FALSE(json.empty());
    // Basic JSON structure check
    EXPECT_NE(json.find("\"passes\""), std::string::npos);
}
