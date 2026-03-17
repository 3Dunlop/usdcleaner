#include "core/pipeline/StageProcessor.h"
#include "core/import/FbxImportFixup.h"
#include "core/metadata/MetadataStripper.h"
#include "core/metadata/IdentityXformStripper.h"
#include "core/welding/VertexWelder.h"
#include "core/topology/DegenerateFaceRemover.h"
#include "core/topology/LaminaFaceRemover.h"
#include "core/materials/MaterialDeduplicator.h"
#include "core/instancing/PointInstancerAuthor.h"
#include "core/hierarchy/HierarchyFlattener.h"
#include "core/cache/GpuCacheOptimizer.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/flattenUtils.h>
#include <pxr/usd/sdf/layer.h>

#include <iostream>
#include <filesystem>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

StageProcessor::StageProcessor(const ProcessorConfig& config)
    : config_(config) {
    BuildDefaultPipeline();
}

void StageProcessor::BuildDefaultPipeline() {
    // Pass order: MetadataStrip -> IdentityXformStrip -> Weld -> Degenerate
    //          -> Lamina -> MaterialDedup -> Instancing -> HierarchyFlatten -> CacheOpt

    if (config_.enableMetadataStrip) {
        pipeline_.AddPass(std::make_shared<MetadataStripper>());
    }

    if (config_.enableIdentityXformStrip) {
        pipeline_.AddPass(std::make_shared<IdentityXformStripper>());
    }

    if (config_.enableWelding) {
        auto welder = std::make_shared<VertexWelder>();
        welder->SetEpsilon(config_.weldingEpsilon);
        welder->SetAutoEpsilon(config_.autoEpsilon);
        pipeline_.AddPass(welder);
    }

    if (config_.enableDegenerateRemoval) {
        pipeline_.AddPass(std::make_shared<DegenerateFaceRemover>());
    }

    if (config_.enableLaminaRemoval) {
        auto lamina = std::make_shared<LaminaFaceRemover>();
        lamina->SetKeepOppositeWinding(config_.keepOppositeWinding);
        pipeline_.AddPass(lamina);
    }

    if (config_.enableMaterialDedup) {
        auto matDedup = std::make_shared<MaterialDeduplicator>();
        matDedup->SetSkipAnimated(config_.skipAnimatedMaterials);
        pipeline_.AddPass(matDedup);
    }

    if (config_.enableInstancing) {
        auto instancer = std::make_shared<PointInstancerAuthor>();
        instancer->SetMinInstanceCount(config_.minInstanceCount);
        instancer->SetNormalizeCentroids(config_.normalizeCentroids);
        instancer->SetNormalizeScale(config_.normalizeScale);
        pipeline_.AddPass(instancer);
    }

    if (config_.enableHierarchyFlattening) {
        auto flattener = std::make_shared<HierarchyFlattener>();
        flattener->SetPreservePatterns(config_.preservePatterns);
        pipeline_.AddPass(flattener);
    }

    if (config_.enableCacheOptimization) {
        auto cacheOpt = std::make_shared<GpuCacheOptimizer>();
        cacheOpt->SetTriangulate(config_.triangulate);
        pipeline_.AddPass(cacheOpt);
    }
}

bool StageProcessor::Process(const std::string& inputPath,
                              const std::string& outputPath) {
    std::cout << "[StageProcessor] Loading: " << inputPath << "\n";

    // Detect FBX input
    std::string inputExt = std::filesystem::path(inputPath).extension().string();
    bool isFbx = (inputExt == ".fbx" || inputExt == ".FBX");

    UsdStageRefPtr stage;

    if (isFbx) {
        // FBX files are opened via the usdFBX SdfFileFormat plugin, which provides
        // a read-only layer.  We flatten the stage into a new anonymous writable
        // layer so that all optimization passes can mutate it freely.
        auto fbxStage = UsdStage::Open(inputPath);
        if (!fbxStage) {
            std::cerr << "[StageProcessor] Error: Cannot open FBX file '" << inputPath << "'.\n"
                      << "  Ensure USDCleaner was built with -DUSDCLEANER_BUILD_FBX_PLUGIN=ON\n"
                      << "  and that the FBX SDK is installed.\n";
            return false;
        }

        std::cout << "[StageProcessor] FBX stage loaded, flattening to writable layer...\n";
        auto flatLayer = fbxStage->Flatten();
        stage = UsdStage::Open(flatLayer);
        if (!stage) {
            std::cerr << "[StageProcessor] Error: failed to create writable stage from FBX\n";
            return false;
        }
        std::cout << "[StageProcessor] Writable stage ready\n";

        // Insert BIM fixup pass at the beginning of the pipeline
        auto fixup = std::make_shared<FbxImportFixup>(config_.fbxUpAxis, config_.fbxUnitScale);
        pipeline_.InsertPass(0, fixup);
    } else {
        stage = UsdStage::Open(inputPath);
        if (!stage) {
            std::cerr << "[StageProcessor] Error: failed to open " << inputPath << "\n";
            return false;
        }
        std::cout << "[StageProcessor] Stage loaded successfully\n";
    }

    // Run the pipeline
    pipeline_.Execute(stage);

    // Determine output format
    std::string outPath = outputPath;
    if (config_.outputFormat == "usdc" &&
        std::filesystem::path(outPath).extension() != ".usdc") {
        outPath = std::filesystem::path(outPath).replace_extension(".usdc").string();
    }

    // Export the optimized stage
    std::cout << "[StageProcessor] Saving to: " << outPath << "\n";

    if (config_.outputFormat == "usdc") {
        stage->Export(outPath);
    } else {
        // For USDA output, flatten and export
        stage->Export(outPath);
    }

    std::cout << "[StageProcessor] Done\n";
    return true;
}

std::string StageProcessor::GetMetricsJson() const {
    return pipeline_.GetMetrics().ToJson();
}

} // namespace usdcleaner
