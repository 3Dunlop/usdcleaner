#pragma once

#include "core/common/Types.h"

#include <pxr/usd/usdGeom/mesh.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

// Result of normalizing a mesh's positions before hashing.
// Stores the centroid offset needed to reconstruct instance transforms.
struct MeshNormalizationResult {
    HashDigest hash;
    GfVec3f centroid{0.0f, 0.0f, 0.0f};  // local-space centroid that was subtracted
    float boundsDiagonal = 1.0f;           // bounding box diagonal (for scale normalization)
};

// Computes topology fingerprints for mesh deduplication.
// Two meshes with identical hashes have the same local-space geometry.
class USDCLEANER_API GeometryHasher {
public:
    // Hash a mesh's topology (vertex count, face structure, local-space positions)
    // Exact match — positions must be identical (within epsilon)
    HashDigest HashMeshTopology(const UsdGeomMesh& mesh, float positionEpsilon);

    // Hash a mesh's topology with centroid normalization.
    // Positions are translated so the centroid is at the origin before hashing.
    // This allows matching meshes that have the same shape but different local-space offsets.
    // Returns the centroid and bounds diagonal for transform reconstruction.
    MeshNormalizationResult HashMeshNormalized(const UsdGeomMesh& mesh,
                                                float positionEpsilon,
                                                bool normalizeScale = false);
};

} // namespace usdcleaner
