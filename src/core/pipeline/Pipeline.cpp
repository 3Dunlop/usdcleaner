#include "core/pipeline/Pipeline.h"

#include <pxr/usd/sdf/changeBlock.h>

#include <iostream>
#include <chrono>

namespace usdcleaner {

Pipeline::Pipeline() = default;

void Pipeline::AddPass(OptimizationPassPtr pass) {
    passes_.push_back(std::move(pass));
}

void Pipeline::InsertPass(size_t index, OptimizationPassPtr pass) {
    if (index >= passes_.size()) {
        passes_.push_back(std::move(pass));
    } else {
        passes_.insert(passes_.begin() + static_cast<ptrdiff_t>(index), std::move(pass));
    }
}

void Pipeline::Execute(const UsdStageRefPtr& stage) {
    if (!stage) {
        std::cerr << "[Pipeline] Error: null stage\n";
        return;
    }

    std::cout << "[Pipeline] Starting optimization with "
              << passes_.size() << " passes\n";

    for (auto& pass : passes_) {
        if (!pass || !pass->IsEnabled()) {
            continue;
        }

        const std::string& name = pass->GetName();
        std::cout << "[Pipeline] Running pass: " << name << "\n";

        // Snapshot metrics before the pass
        PassMetrics before = metrics_.Snapshot(stage, name + "_before");

        auto startTime = std::chrono::high_resolution_clock::now();

        // Execute the pass (pass manages its own SdfChangeBlock)
        pass->Execute(stage);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();

        // Snapshot metrics after the pass
        PassMetrics after = metrics_.Snapshot(stage, name + "_after");

        // Record delta
        metrics_.RecordPass(name, before, after);

        std::cout << "[Pipeline] " << name << " completed in "
                  << durationMs << "ms"
                  << " | vertices: " << before.totalVertices
                  << " -> " << after.totalVertices
                  << " | faces: " << before.totalFaces
                  << " -> " << after.totalFaces
                  << " | materials: " << before.materialCount
                  << " -> " << after.materialCount
                  << "\n";
    }

    std::cout << "[Pipeline] All passes complete\n";
}

} // namespace usdcleaner
