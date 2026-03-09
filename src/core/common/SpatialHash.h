#pragma once

#include "core/common/Types.h"

#include <unordered_map>
#include <vector>
#include <utility>

namespace usdcleaner {

// 3D spatial hash grid for fast nearest-neighbor vertex queries.
// Cell size should be >= 2 * epsilon for correct neighbor detection.
class USDCLEANER_API SpatialHash {
public:
    explicit SpatialHash(float cellSize);

    // Insert a point and return its canonical index.
    // If an existing point is found within epsilon, returns its index instead.
    size_t InsertOrFind(const GfVec3f& position, float epsilon);

    // Generate a full remap table: old_index -> new_index
    // for an entire points array.
    RemapTable GenerateRemapTable(const VtVec3fArray& positions, float epsilon);

    // Build the compacted points array from the remap table.
    // Returns the new (smaller) points array.
    static VtVec3fArray CompactPoints(const VtVec3fArray& originalPoints,
                                       const RemapTable& remap);

    // Reset internal state for reuse
    void Clear();

private:
    struct Entry {
        size_t canonicalIndex;
        GfVec3f position;
    };

    struct Cell {
        std::vector<Entry> entries;
    };

    float cellSize_;
    size_t nextCanonicalIndex_ = 0;
    std::unordered_map<int64_t, Cell> grid_;

    // Hash a 3D position to a cell key
    int64_t HashPosition(const GfVec3f& p) const;

    // Get the 27 neighboring cell keys (3x3x3 neighborhood)
    void GetNeighborKeys(const GfVec3f& p,
                         std::vector<int64_t>& outKeys) const;
};

} // namespace usdcleaner
