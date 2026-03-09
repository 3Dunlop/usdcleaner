#pragma once

#include "core/pipeline/OptimizationPass.h"

namespace usdcleaner {

// Removes interior (occluded) faces via ray casting.
// Phase 2 feature -- requires Embree.
class USDCLEANER_API InteriorCuller : public OptimizationPass {
public:
    std::string GetName() const override { return "InteriorFaceCulling"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

private:
    PassMetrics metrics_;
};

} // namespace usdcleaner
