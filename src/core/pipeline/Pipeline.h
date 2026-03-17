#pragma once

#include "core/common/Types.h"
#include "core/common/Metrics.h"
#include "core/pipeline/OptimizationPass.h"

#include <pxr/usd/usd/stage.h>

#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

// Orchestrates the ordered execution of optimization passes on a USD stage.
// Collects before/after metrics for each pass.
class USDCLEANER_API Pipeline {
public:
    Pipeline();

    // Add a pass to the end of the pipeline
    void AddPass(OptimizationPassPtr pass);

    // Insert a pass at a specific position (0 = beginning)
    void InsertPass(size_t index, OptimizationPassPtr pass);

    // Execute all enabled passes on the stage
    void Execute(const UsdStageRefPtr& stage);

    // Get the metrics collector with results from the last execution
    const MetricsCollector& GetMetrics() const { return metrics_; }

    // Get the list of registered passes
    const std::vector<OptimizationPassPtr>& GetPasses() const { return passes_; }

private:
    std::vector<OptimizationPassPtr> passes_;
    MetricsCollector metrics_;
};

} // namespace usdcleaner
