#include "core/common/SpatialHash.h"

#include <cmath>
#include <limits>

namespace usdcleaner {

SpatialHash::SpatialHash(float cellSize)
    : cellSize_(cellSize) {
    // Ensure cell size is positive and reasonable
    if (cellSize_ <= 0.0f) {
        cellSize_ = 1e-4f;
    }
}

int64_t SpatialHash::HashPosition(const GfVec3f& p) const {
    // Map position to integer grid coordinates
    int32_t ix = static_cast<int32_t>(std::floor(p[0] / cellSize_));
    int32_t iy = static_cast<int32_t>(std::floor(p[1] / cellSize_));
    int32_t iz = static_cast<int32_t>(std::floor(p[2] / cellSize_));

    // Combine using a hash that distributes well
    // Using large primes to minimize collisions
    int64_t h = static_cast<int64_t>(ix) * 73856093LL
              ^ static_cast<int64_t>(iy) * 19349669LL
              ^ static_cast<int64_t>(iz) * 83492791LL;
    return h;
}

void SpatialHash::GetNeighborKeys(const GfVec3f& p,
                                   std::vector<int64_t>& outKeys) const {
    outKeys.clear();
    outKeys.reserve(27);

    int32_t cx = static_cast<int32_t>(std::floor(p[0] / cellSize_));
    int32_t cy = static_cast<int32_t>(std::floor(p[1] / cellSize_));
    int32_t cz = static_cast<int32_t>(std::floor(p[2] / cellSize_));

    for (int32_t dx = -1; dx <= 1; ++dx) {
        for (int32_t dy = -1; dy <= 1; ++dy) {
            for (int32_t dz = -1; dz <= 1; ++dz) {
                int32_t nx = cx + dx;
                int32_t ny = cy + dy;
                int32_t nz = cz + dz;
                int64_t h = static_cast<int64_t>(nx) * 73856093LL
                          ^ static_cast<int64_t>(ny) * 19349669LL
                          ^ static_cast<int64_t>(nz) * 83492791LL;
                outKeys.push_back(h);
            }
        }
    }
}

size_t SpatialHash::InsertOrFind(const GfVec3f& position, float epsilon) {
    float epsilonSq = epsilon * epsilon;

    // Check neighboring cells for an existing match
    std::vector<int64_t> neighborKeys;
    GetNeighborKeys(position, neighborKeys);

    for (int64_t key : neighborKeys) {
        auto it = grid_.find(key);
        if (it == grid_.end()) continue;

        for (const Entry& entry : it->second.entries) {
            GfVec3f diff = position - entry.position;
            float distSq = diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2];
            if (distSq <= epsilonSq) {
                return entry.canonicalIndex;
            }
        }
    }

    // No match found -- insert new entry
    size_t newIndex = nextCanonicalIndex_++;
    int64_t cellKey = HashPosition(position);
    grid_[cellKey].entries.push_back({newIndex, position});

    return newIndex;
}

RemapTable SpatialHash::GenerateRemapTable(const VtVec3fArray& positions,
                                            float epsilon) {
    Clear();
    RemapTable remap(positions.size());

    for (size_t i = 0; i < positions.size(); ++i) {
        remap[i] = static_cast<uint32_t>(InsertOrFind(positions[i], epsilon));
    }

    return remap;
}

VtVec3fArray SpatialHash::CompactPoints(const VtVec3fArray& originalPoints,
                                         const RemapTable& remap) {
    // Determine the number of unique vertices
    uint32_t maxIndex = 0;
    for (uint32_t idx : remap) {
        if (idx > maxIndex) maxIndex = idx;
    }

    VtVec3fArray compacted(maxIndex + 1);

    // For each unique index, take the first vertex that maps to it
    std::vector<bool> filled(maxIndex + 1, false);
    for (size_t i = 0; i < originalPoints.size(); ++i) {
        uint32_t newIdx = remap[i];
        if (!filled[newIdx]) {
            compacted[newIdx] = originalPoints[i];
            filled[newIdx] = true;
        }
    }

    return compacted;
}

void SpatialHash::Clear() {
    grid_.clear();
    nextCanonicalIndex_ = 0;
}

} // namespace usdcleaner
