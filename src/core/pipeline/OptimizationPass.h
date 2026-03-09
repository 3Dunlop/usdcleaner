#pragma once

#include "core/common/Types.h"
#include "core/common/Metrics.h"

#include <pxr/usd/usd/stage.h>

#include <string>
#include <memory>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

// Abstract base class for all optimization passes.
// Each pass follows a two-phase pattern:
//   1. Analyze (read-only, safe for parallel execution)
//   2. Apply (mutate stage, must be within SdfChangeBlock)
class USDCLEANER_API OptimizationPass {
public:
    virtual ~OptimizationPass() = default;

    // Human-readable name for logging and metrics
    virtual std::string GetName() const = 0;

    // Execute the optimization pass on the stage.
    // The pass is responsible for its own SdfChangeBlock scoping.
    virtual void Execute(const UsdStageRefPtr& stage) = 0;

    // Get metrics collected during the last Execute() call
    virtual PassMetrics GetMetrics() const = 0;

    // Enable/disable this pass
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

protected:
    bool enabled_ = true;
};

using OptimizationPassPtr = std::shared_ptr<OptimizationPass>;

} // namespace usdcleaner
