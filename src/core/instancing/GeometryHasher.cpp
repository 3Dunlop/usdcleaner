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

    // Hash quantized vertex positions (to handle floating-point noise).
    // Use int64_t to avoid overflow: position=1e6, epsilon=1e-7 -> quantized=1e13
    // which would overflow int32 range (~2e9). Clamp invEps to a safe maximum
    // to prevent extreme quantization values.
    float invEps = 1.0f / positionEpsilon;
    const float maxInvEps = 1e6f; // Prevents overflow for positions up to ~2000
    invEps = std::min(invEps, maxInvEps);
    for (const auto& p : points) {
        int64_t qx = static_cast<int64_t>(std::round(static_cast<double>(p[0]) * invEps));
        int64_t qy = static_cast<int64_t>(std::round(static_cast<double>(p[1]) * invEps));
        int64_t qz = static_cast<int64_t>(std::round(static_cast<double>(p[2]) * invEps));
        hasher.Update(&qx, sizeof(qx));
        hasher.Update(&qy, sizeof(qy));
        hasher.Update(&qz, sizeof(qz));
    }

    return hasher.Finalize();
}

} // namespace usdcleaner
