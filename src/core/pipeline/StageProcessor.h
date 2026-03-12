#pragma once

#include "core/common/Types.h"
#include "core/pipeline/Pipeline.h"

#include <string>
#include <vector>

namespace usdcleaner {

// Configuration for stage processing
struct USDCLEANER_API ProcessorConfig {
    // Metadata stripping
    bool enableMetadataStrip = true;
    bool enableIdentityXformStrip = true;

    // Vertex welding
    bool enableWelding = true;
    float weldingEpsilon = 0.0f;  // 0 = auto-detect
    bool autoEpsilon = true;

    // Topology cleanup
    bool enableDegenerateRemoval = true;
    bool enableLaminaRemoval = true;
    bool keepOppositeWinding = false;

    // Material deduplication
    bool enableMaterialDedup = true;
    bool skipAnimatedMaterials = true;

    // Geometric instancing (Phase 2)
    bool enableInstancing = false;  // off by default
    int minInstanceCount = 3;

    // Hierarchy flattening (Phase 2)
    bool enableHierarchyFlattening = false;  // off by default
    std::vector<std::string> preservePatterns;

    // GPU cache optimization
    bool enableCacheOptimization = true;
    bool triangulate = false;

    // Output format
    std::string outputFormat = "usdc";  // "usdc" or "usda"
};

// Processes a single USD file through the optimization pipeline.
class USDCLEANER_API StageProcessor {
public:
    explicit StageProcessor(const ProcessorConfig& config = ProcessorConfig{});

    // Process an input file and write the optimized result
    // Returns true on success
    bool Process(const std::string& inputPath,
                 const std::string& outputPath);

    // Get the pipeline (for adding custom passes)
    Pipeline& GetPipeline() { return pipeline_; }

    // Get metrics from the last processing run
    std::string GetMetricsJson() const;

private:
    ProcessorConfig config_;
    Pipeline pipeline_;

    // Build the default pipeline from config
    void BuildDefaultPipeline();
};

} // namespace usdcleaner
