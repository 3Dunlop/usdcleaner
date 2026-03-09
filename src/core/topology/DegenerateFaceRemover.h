#pragma once

#include "core/pipeline/OptimizationPass.h"

namespace usdcleaner {

// Removes degenerate faces (faces with fewer than 3 unique vertex indices).
// Should run AFTER vertex welding, as welding can create degeneracies.
class USDCLEANER_API DegenerateFaceRemover : public OptimizationPass {
public:
    std::string GetName() const override { return "DegenerateFaceRemoval"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

private:
    PassMetrics metrics_;
};

} // namespace usdcleaner
