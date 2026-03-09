#pragma once

#include "core/pipeline/OptimizationPass.h"

namespace usdcleaner {

// Replaces groups of identical meshes with UsdGeomPointInstancer prims.
// Phase 2 feature.
class USDCLEANER_API PointInstancerAuthor : public OptimizationPass {
public:
    std::string GetName() const override { return "GeometricInstancing"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

    void SetMinInstanceCount(int count) { minInstanceCount_ = count; }
    void SetPositionEpsilon(float eps) { positionEpsilon_ = eps; }

private:
    PassMetrics metrics_;
    int minInstanceCount_ = 3;
    float positionEpsilon_ = 1e-3f;
};

} // namespace usdcleaner
