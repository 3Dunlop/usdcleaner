#include "core/instancing/GeometryHasher.h"
#include "core/common/HashUtils.h"

#include <cmath>
#include <algorithm>
#include <limits>

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

MeshNormalizationResult GeometryHasher::HashMeshNormalized(
    const UsdGeomMesh& mesh,
    float positionEpsilon,
    bool normalizeScale) {

    MeshNormalizationResult result;

    VtVec3fArray points;
    VtIntArray faceVertexCounts, faceVertexIndices;
    mesh.GetPointsAttr().Get(&points);
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
    mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);

    if (points.empty()) {
        SHA256Hasher hasher;
        hasher.Update(0);
        result.hash = hasher.Finalize();
        return result;
    }

    // Compute centroid
    double cx = 0.0, cy = 0.0, cz = 0.0;
    for (const auto& p : points) {
        cx += p[0];
        cy += p[1];
        cz += p[2];
    }
    double n = static_cast<double>(points.size());
    cx /= n;
    cy /= n;
    cz /= n;
    result.centroid = GfVec3f(static_cast<float>(cx),
                               static_cast<float>(cy),
                               static_cast<float>(cz));

    // Compute bounding box diagonal (for optional scale normalization)
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();

    for (const auto& p : points) {
        float px = p[0] - result.centroid[0];
        float py = p[1] - result.centroid[1];
        float pz = p[2] - result.centroid[2];
        minX = std::min(minX, px);
        minY = std::min(minY, py);
        minZ = std::min(minZ, pz);
        maxX = std::max(maxX, px);
        maxY = std::max(maxY, py);
        maxZ = std::max(maxZ, pz);
    }

    float dx = maxX - minX;
    float dy = maxY - minY;
    float dz = maxZ - minZ;
    result.boundsDiagonal = std::sqrt(dx * dx + dy * dy + dz * dz);

    // Avoid division by zero for degenerate meshes
    float scaleFactor = 1.0f;
    if (normalizeScale && result.boundsDiagonal > 1e-10f) {
        scaleFactor = 1.0f / result.boundsDiagonal;
    }

    // Hash topology (same as HashMeshTopology)
    SHA256Hasher hasher;
    hasher.Update(static_cast<int>(points.size()));

    for (int count : faceVertexCounts) {
        hasher.Update(count);
    }
    for (int idx : faceVertexIndices) {
        hasher.Update(idx);
    }

    // Hash centroid-normalized (and optionally scale-normalized) vertex positions
    float invEps = 1.0f / positionEpsilon;
    const float maxInvEps = 1e6f;
    invEps = std::min(invEps, maxInvEps);

    for (const auto& p : points) {
        float px = (p[0] - result.centroid[0]) * scaleFactor;
        float py = (p[1] - result.centroid[1]) * scaleFactor;
        float pz = (p[2] - result.centroid[2]) * scaleFactor;

        int64_t qx = static_cast<int64_t>(std::round(static_cast<double>(px) * invEps));
        int64_t qy = static_cast<int64_t>(std::round(static_cast<double>(py) * invEps));
        int64_t qz = static_cast<int64_t>(std::round(static_cast<double>(pz) * invEps));
        hasher.Update(&qx, sizeof(qx));
        hasher.Update(&qy, sizeof(qy));
        hasher.Update(&qz, sizeof(qz));
    }

    result.hash = hasher.Finalize();
    return result;
}

} // namespace usdcleaner
