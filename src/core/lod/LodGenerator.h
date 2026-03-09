#pragma once

#include "core/pipeline/OptimizationPass.h"

namespace usdcleaner {

// Generates LOD levels via mesh simplification and authors USD VariantSets.
// Phase 2 feature.
class USDCLEANER_API LodGenerator : public OptimizationPass {
public:
    std::string GetName() const override { return "LODGeneration"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

private:
    PassMetrics metrics_;
};

} // namespace usdcleaner
