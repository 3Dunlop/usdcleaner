#pragma once

#include "core/common/Types.h"
#include "core/pipeline/StageProcessor.h"

#include <string>
#include <vector>

namespace usdcleaner {

// Batch processing configuration
struct USDCLEANER_API BatchConfig {
    ProcessorConfig processorConfig;
    int maxConcurrentFiles = 4;
    std::string inputGlob = "*.usd*";
    std::string outputDirectory = "./optimized/";
};

// Processes multiple USD files, optionally in parallel via TBB.
class USDCLEANER_API BatchProcessor {
public:
    explicit BatchProcessor(const BatchConfig& config = BatchConfig{});

    // Process all files matching the glob in the input directory
    void ProcessDirectory(const std::string& inputDir);

    // Process a specific list of files
    void ProcessFiles(const std::vector<std::string>& inputPaths);

    // Get aggregate metrics as JSON
    std::string GetAggregateMetricsJson() const;

private:
    BatchConfig config_;

    std::string MakeOutputPath(const std::string& inputPath) const;
};

} // namespace usdcleaner
