#include "core/pipeline/StageProcessor.h"
#include "core/pipeline/BatchProcessor.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>

#ifdef USDCLEANER_HAS_CLI11
#include <CLI/CLI.hpp>
#endif

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
#ifdef USDCLEANER_HAS_CLI11
    CLI::App app{"USDCleaner - USD Geometry & Material Optimization Pipeline"};
    app.set_version_flag("--version", "0.1.0");

    std::string inputPath;
    std::string outputPath;
    std::string metricsPath;

    // Input/output
    app.add_option("input", inputPath, "Input USD file or directory")
        ->required()
        ->check(CLI::ExistingPath);
    app.add_option("-o,--output", outputPath, "Output USD file or directory");
    app.add_option("--metrics", metricsPath, "Output metrics JSON file");

    // Pass toggles (disable defaults)
    bool noMetadataStrip = false, noIdentityXformStrip = false;
    bool noWeld = false, noDegenerate = false, noLamina = false;
    bool noMaterialDedup = false, noCacheOpt = false;
    app.add_flag("--no-metadata-strip", noMetadataStrip, "Disable metadata/property stripping");
    app.add_flag("--no-identity-xform-strip", noIdentityXformStrip, "Disable identity xformOp removal");
    app.add_flag("--no-weld", noWeld, "Disable vertex welding");
    app.add_flag("--no-degenerate", noDegenerate, "Disable degenerate face removal");
    app.add_flag("--no-lamina", noLamina, "Disable lamina face removal");
    app.add_flag("--no-material-dedup", noMaterialDedup, "Disable material deduplication");
    app.add_flag("--no-cache-opt", noCacheOpt, "Disable GPU cache optimization");

    // Phase 2 toggles (opt-in)
    bool enableInstancing = false;
    int minInstanceCount = 2;
    bool enableHierarchyFlatten = false;
    bool noNormalizeCentroids = false;
    bool enableScaleNormalization = false;
    app.add_flag("--enable-instancing", enableInstancing, "Enable geometric instancing (PointInstancer)");
    app.add_option("--min-instance-count", minInstanceCount, "Minimum meshes to form an instance group (default: 2)");
    app.add_flag("--enable-hierarchy-flatten", enableHierarchyFlatten, "Enable hierarchy flattening");
    app.add_flag("--no-normalize-centroids", noNormalizeCentroids, "Disable centroid normalization for instancing");
    app.add_flag("--enable-scale-normalization", enableScaleNormalization, "Enable scale-invariant instancing");

    // Welding options
    float epsilon = 0.0f;
    bool manualEpsilon = false;
    app.add_option("--epsilon", epsilon, "Manual welding epsilon (0 = auto-detect)");

    // Output format
    std::string format = "usdc";
    app.add_option("--format", format, "Output format: usdc (default) or usda")
        ->check(CLI::IsMember({"usdc", "usda"}));

    // Triangulation
    bool triangulate = false;
    app.add_flag("--triangulate", triangulate, "Convert to triangles during cache optimization");

    // Batch mode
    int batchConcurrency = 4;
    app.add_option("--batch-concurrency", batchConcurrency, "Max concurrent files in batch mode");

    CLI11_PARSE(app, argc, argv);

    // Determine if processing single file or batch directory
    bool isBatch = fs::is_directory(inputPath);

    // Build processor config
    usdcleaner::ProcessorConfig config;
    config.enableMetadataStrip = !noMetadataStrip;
    config.enableIdentityXformStrip = !noIdentityXformStrip;
    config.enableWelding = !noWeld;
    config.enableDegenerateRemoval = !noDegenerate;
    config.enableLaminaRemoval = !noLamina;
    config.enableMaterialDedup = !noMaterialDedup;
    config.enableInstancing = enableInstancing;
    config.minInstanceCount = minInstanceCount;
    config.normalizeCentroids = !noNormalizeCentroids;
    config.normalizeScale = enableScaleNormalization;
    config.enableHierarchyFlattening = enableHierarchyFlatten;
    config.enableCacheOptimization = !noCacheOpt;
    config.triangulate = triangulate;
    config.outputFormat = format;

    if (epsilon > 0.0f) {
        config.autoEpsilon = false;
        config.weldingEpsilon = epsilon;
    }

    if (isBatch) {
        // Batch processing
        if (outputPath.empty()) {
            outputPath = (fs::path(inputPath) / "optimized").string();
        }

        usdcleaner::BatchConfig batchConfig;
        batchConfig.processorConfig = config;
        batchConfig.maxConcurrentFiles = batchConcurrency;
        batchConfig.outputDirectory = outputPath;

        usdcleaner::BatchProcessor batch(batchConfig);
        batch.ProcessDirectory(inputPath);
    } else {
        // Single file processing
        if (outputPath.empty()) {
            fs::path p(inputPath);
            outputPath = (p.parent_path() / (p.stem().string() + "_optimized")).string();
            outputPath += (format == "usdc") ? ".usdc" : ".usda";
        }

        usdcleaner::StageProcessor processor(config);
        bool success = processor.Process(inputPath, outputPath);

        if (success && !metricsPath.empty()) {
            std::ofstream metricsFile(metricsPath);
            metricsFile << processor.GetMetricsJson();
            std::cout << "[CLI] Metrics written to: " << metricsPath << "\n";
        }

        return success ? 0 : 1;
    }

#else
    // Minimal CLI without CLI11
    if (argc < 3) {
        std::cerr << "Usage: usdcleaner <input.usd> <output.usdc>\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];

    usdcleaner::StageProcessor processor;
    return processor.Process(inputPath, outputPath) ? 0 : 1;
#endif

    return 0;
}
