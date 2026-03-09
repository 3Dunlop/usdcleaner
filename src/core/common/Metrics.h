#pragma once

#include "core/common/Types.h"

#include <pxr/usd/usd/stage.h>

#include <string>
#include <unordered_map>

namespace usdcleaner {

// Per-pass metrics snapshot
struct USDCLEANER_API PassMetrics {
    std::string passName;
    size_t meshCount = 0;
    size_t totalVertices = 0;
    size_t totalFaces = 0;
    size_t totalFaceVertices = 0;
    size_t materialCount = 0;
    size_t primCount = 0;
    size_t xformCount = 0;

    // Delta fields (computed by comparing before/after)
    int64_t verticesRemoved = 0;
    int64_t facesRemoved = 0;
    int64_t materialsRemoved = 0;
    int64_t primsRemoved = 0;
};

// Collects and aggregates metrics across the pipeline
class USDCLEANER_API MetricsCollector {
public:
    // Snapshot the current state of the stage
    PassMetrics Snapshot(const PXR_NS::UsdStageRefPtr& stage,
                         const std::string& label) const;

    // Record a pass result (before and after snapshots)
    void RecordPass(const std::string& passName,
                    const PassMetrics& before,
                    const PassMetrics& after);

    // Get all recorded pass metrics
    const std::vector<PassMetrics>& GetPassResults() const { return passResults_; }

    // Serialize to JSON string
    std::string ToJson() const;

private:
    std::vector<PassMetrics> passResults_;
};

} // namespace usdcleaner
