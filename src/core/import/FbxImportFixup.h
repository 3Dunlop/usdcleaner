#pragma once

#include "core/pipeline/OptimizationPass.h"

#include <string>

namespace usdcleaner {

// Post-import fixup pass for FBX files (especially Navisworks exports).
//
// When USDCleaner loads an FBX file via the usdFBX plugin, the resulting
// USD stage may need adjustments for BIM workflows:
//   - Axis correction (Z-up is standard for BIM, but FBX default is Y-up)
//   - Unit scale normalization (Navisworks exports in cm; USD convention is m)
//   - Pruning deeply nested empty Xform groups from Navisworks hierarchy
class USDCLEANER_API FbxImportFixup : public OptimizationPass {
public:
    explicit FbxImportFixup(const std::string& upAxis = "z",
                            float unitScale = 1.0f);

    std::string GetName() const override { return "FbxImportFixup"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

private:
    std::string upAxis_;    // "y" or "z"
    float unitScale_;       // scale factor to apply
    PassMetrics metrics_;

    void FixUpAxis(const UsdStageRefPtr& stage);
    void ApplyUnitScale(const UsdStageRefPtr& stage);
    void PruneEmptyGroups(const UsdStageRefPtr& stage);
};

} // namespace usdcleaner
