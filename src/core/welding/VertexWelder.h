#pragma once

#include "core/pipeline/OptimizationPass.h"
#include "core/common/SpatialHash.h"

namespace usdcleaner {

// Welds coincident vertices within a configurable epsilon tolerance.
// Uses a spatial hash grid for O(N) average-case performance.
// Must run FIRST in the pipeline (downstream passes depend on deduplicated indices).
class USDCLEANER_API VertexWelder : public OptimizationPass {
public:
    VertexWelder();

    std::string GetName() const override { return "VertexWelding"; }
    void Execute(const UsdStageRefPtr& stage) override;
    PassMetrics GetMetrics() const override { return metrics_; }

    // Set the welding epsilon (distance threshold)
    void SetEpsilon(float epsilon) { epsilon_ = epsilon; }
    float GetEpsilon() const { return epsilon_; }

    // Enable auto-epsilon detection from scene scale
    void SetAutoEpsilon(bool autoEpsilon) { autoEpsilon_ = autoEpsilon; }
    bool GetAutoEpsilon() const { return autoEpsilon_; }

private:
    float epsilon_ = 1e-5f;
    bool autoEpsilon_ = true;
    PassMetrics metrics_;

    // Weld a single mesh
    void WeldMesh(UsdGeomMesh& mesh, float epsilon);
};

} // namespace usdcleaner
