#include "core/pipeline/BatchProcessor.h"

#include <filesystem>
#include <iostream>
#include <algorithm>

#ifdef USDCLEANER_HAS_TBB
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#endif

namespace fs = std::filesystem;

namespace usdcleaner {

BatchProcessor::BatchProcessor(const BatchConfig& config)
    : config_(config) {}

void BatchProcessor::ProcessDirectory(const std::string& inputDir) {
    std::vector<std::string> files;

    // Collect matching files
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        // Match common USD extensions
        if (ext == ".usd" || ext == ".usda" || ext == ".usdc") {
            files.push_back(entry.path().string());
        }
    }

    std::sort(files.begin(), files.end());

    std::cout << "[BatchProcessor] Found " << files.size()
              << " USD files in " << inputDir << "\n";

    ProcessFiles(files);
}

void BatchProcessor::ProcessFiles(const std::vector<std::string>& inputPaths) {
    // Ensure output directory exists
    fs::create_directories(config_.outputDirectory);

#ifdef USDCLEANER_HAS_TBB
    // Parallel processing: each file gets its own StageProcessor
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, inputPaths.size()),
        [&](const tbb::blocked_range<size_t>& range) {
            for (size_t i = range.begin(); i < range.end(); ++i) {
                StageProcessor processor(config_.processorConfig);
                std::string outPath = MakeOutputPath(inputPaths[i]);
                processor.Process(inputPaths[i], outPath);
            }
        },
        tbb::simple_partitioner()
    );
#else
    // Sequential fallback
    for (const auto& inputPath : inputPaths) {
        StageProcessor processor(config_.processorConfig);
        std::string outPath = MakeOutputPath(inputPath);
        processor.Process(inputPath, outPath);
    }
#endif
}

std::string BatchProcessor::MakeOutputPath(const std::string& inputPath) const {
    fs::path input(inputPath);
    fs::path output = fs::path(config_.outputDirectory) / input.filename();

    // Change extension based on config
    if (config_.processorConfig.outputFormat == "usdc") {
        output.replace_extension(".usdc");
    }

    return output.string();
}

std::string BatchProcessor::GetAggregateMetricsJson() const {
    // TODO: Aggregate metrics from all processed files
    return "{}";
}

} // namespace usdcleaner
