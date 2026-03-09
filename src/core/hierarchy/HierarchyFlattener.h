#pragma once

#include "core/pipeline/OptimizationPass.h"

#include <string>
#include <vector>

namespace usdcleaner {

// Collapses single-child Xform chains to reduce hierarchy depth.
// Phase 2 feature.
class USDCLEANER_API HierarchyFlattener : public OptimizationPass {
public:
    std::string GetName() const override { return "HierarchyFlattening"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

    // Regex patterns for prim names that should never be flattened
    void SetPreservePatterns(const std::vector<std::string>& patterns) {
        preservePatterns_ = patterns;
    }

private:
    PassMetrics metrics_;
    std::vector<std::string> preservePatterns_;

    bool IsSafeToFlatten(const UsdPrim& prim) const;
};

} // namespace usdcleaner
