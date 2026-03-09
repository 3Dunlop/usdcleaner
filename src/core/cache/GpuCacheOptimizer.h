#pragma once

#include "core/pipeline/OptimizationPass.h"

namespace usdcleaner {

// Optimizes mesh index and vertex buffers for GPU vertex cache and fetch
// performance using meshoptimizer. Must be the LAST geometry pass in the pipeline.
class USDCLEANER_API GpuCacheOptimizer : public OptimizationPass {
public:
    std::string GetName() const override { return "GpuCacheOptimization"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

    // If true, convert quads/n-gons to triangles before optimization
    void SetTriangulate(bool triangulate) { triangulate_ = triangulate; }
    bool GetTriangulate() const { return triangulate_; }

private:
    PassMetrics metrics_;
    bool triangulate_ = false;
};

} // namespace usdcleaner
