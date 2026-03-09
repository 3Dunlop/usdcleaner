#pragma once

#include "core/common/Types.h"

namespace usdcleaner {

// Builds a BVH from all mesh triangles for occlusion queries.
// Phase 2 feature -- will wrap Intel Embree.
class USDCLEANER_API BvhBuilder {
public:
    // TODO: Embree-based BVH construction
};

} // namespace usdcleaner
