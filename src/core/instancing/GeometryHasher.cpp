#include "core/instancing/GeometryHasher.h"
#include "core/common/HashUtils.h"

namespace usdcleaner {

HashDigest GeometryHasher::HashMeshTopology(const UsdGeomMesh& mesh,
                                             float positionEpsilon) {
    SHA256Hasher hasher;

    VtVec3fArray points;
    VtIntArray faceVertexCounts, faceVertexIndices;
    mesh.GetPointsAttr().Get(&points);
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
    mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);

    // Hash vertex count
    hasher.Update(static_cast<int>(points.size()));

    // Hash face structure
    for (int count : faceVertexCounts) {
        hasher.Update(count);
    }
    for (int idx : faceVertexIndices) {
        hasher.Update(idx);
    }

    // Hash quantized vertex positions (to handle floating-point noise)
    float invEps = 1.0f / positionEpsilon;
    for (const auto& p : points) {
        int qx = static_cast<int>(std::round(p[0] * invEps));
        int qy = static_cast<int>(std::round(p[1] * invEps));
        int qz = static_cast<int>(std::round(p[2] * invEps));
        hasher.Update(qx);
        hasher.Update(qy);
        hasher.Update(qz);
    }

    return hasher.Finalize();
}

} // namespace usdcleaner
