#pragma once

#include "core/common/Types.h"

#include <pxr/usd/usdGeom/mesh.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

// Computes topology fingerprints for mesh deduplication.
// Two meshes with identical hashes have the same local-space geometry.
class USDCLEANER_API GeometryHasher {
public:
    // Hash a mesh's topology (vertex count, face structure, local-space positions)
    HashDigest HashMeshTopology(const UsdGeomMesh& mesh, float positionEpsilon);
};

} // namespace usdcleaner
