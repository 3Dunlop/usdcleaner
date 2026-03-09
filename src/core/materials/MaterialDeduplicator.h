#pragma once

#include "core/pipeline/OptimizationPass.h"
#include "core/materials/MaterialHasher.h"

#include <unordered_map>

namespace usdcleaner {

// Deduplicates identical materials by deep-hashing shader networks,
// rebinding meshes to master materials, and pruning orphaned material prims.
class USDCLEANER_API MaterialDeduplicator : public OptimizationPass {
public:
    std::string GetName() const override { return "MaterialDeduplication"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

    // Skip materials with time-sampled (animated) inputs
    void SetSkipAnimated(bool skip) { skipAnimated_ = skip; }
    bool GetSkipAnimated() const { return skipAnimated_; }

private:
    PassMetrics metrics_;
    bool skipAnimated_ = true;
    MaterialHasher hasher_;

    // Map from duplicate material path -> master material path
    std::unordered_map<std::string, std::string> duplicateToMaster_;
};

} // namespace usdcleaner
