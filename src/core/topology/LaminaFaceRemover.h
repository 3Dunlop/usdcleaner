#pragma once

#include "core/pipeline/OptimizationPass.h"

namespace usdcleaner {

// Removes lamina (duplicate) faces -- faces sharing the exact same vertex set.
// Uses deterministic index sorting + hashing for efficient detection.
class USDCLEANER_API LaminaFaceRemover : public OptimizationPass {
public:
    std::string GetName() const override { return "LaminaFaceRemoval"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

    // If true, faces with identical vertices but opposite winding order
    // are treated as separate faces (kept). If false, they are duplicates.
    void SetKeepOppositeWinding(bool keep) { keepOppositeWinding_ = keep; }
    bool GetKeepOppositeWinding() const { return keepOppositeWinding_; }

private:
    PassMetrics metrics_;
    bool keepOppositeWinding_ = false;
};

} // namespace usdcleaner
