#pragma once

#include "core/pipeline/OptimizationPass.h"

namespace usdcleaner {

// Removes identity xformOps from UsdGeomXformable prims.
// BIM exports often have identity transforms on 60-70% of prims.
// This pass clears the xformOpOrder and individual op attributes
// when the composed local transform is identity (within epsilon).
class USDCLEANER_API IdentityXformStripper : public OptimizationPass {
public:
    std::string GetName() const override { return "IdentityXformStripper"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

    void SetEpsilon(double eps) { epsilon_ = eps; }

private:
    PassMetrics metrics_;
    double epsilon_ = 1e-7;
    size_t xformOpsCleared_ = 0;

    bool IsNearIdentity(const GfMatrix4d& mat) const;
};

} // namespace usdcleaner
