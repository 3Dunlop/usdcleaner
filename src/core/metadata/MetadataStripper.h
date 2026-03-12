#pragma once

#include "core/pipeline/OptimizationPass.h"

namespace usdcleaner {

// Strips bloated metadata from BIM-exported USD stages:
// - customData['userDocBrief'] (FBX converter artifact, ~14% of file size)
// - None-valued authored attributes
// - Empty-array attributes (cornerIndices, creaseIndices, etc.)
// - Redundant subdivision defaults on meshes
class USDCLEANER_API MetadataStripper : public OptimizationPass {
public:
    std::string GetName() const override { return "MetadataStripper"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

private:
    PassMetrics metrics_;
    size_t customDataEntriesRemoved_ = 0;
    size_t propertiesRemoved_ = 0;
    size_t subdivAttrsRemoved_ = 0;

    void StripRedundantSubdivAttrs(UsdPrim& prim);
};

} // namespace usdcleaner
