#pragma once

#include "core/pipeline/OptimizationPass.h"

namespace usdcleaner {

// Replaces groups of identical meshes with UsdGeomPointInstancer prims.
// Supports centroid-normalized matching (catches same shape at different local offsets)
// and multi-prototype instancers (meshes with same geometry but different materials).
class USDCLEANER_API PointInstancerAuthor : public OptimizationPass {
public:
    std::string GetName() const override { return "GeometricInstancing"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

    void SetMinInstanceCount(int count) { minInstanceCount_ = count; }
    void SetPositionEpsilon(float eps) { positionEpsilon_ = eps; }
    void SetNormalizeCentroids(bool enable) { normalizeCentroids_ = enable; }
    void SetNormalizeScale(bool enable) { normalizeScale_ = enable; }

private:
    PassMetrics metrics_;
    int minInstanceCount_ = 2;
    float positionEpsilon_ = 1e-3f;
    bool normalizeCentroids_ = true;   // on by default — huge win for BIM data
    bool normalizeScale_ = false;       // off by default — more aggressive
};

} // namespace usdcleaner
