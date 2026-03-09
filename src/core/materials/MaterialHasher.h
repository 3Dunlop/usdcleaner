#pragma once

#include "core/common/Types.h"
#include "core/common/HashUtils.h"

#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>

#include <unordered_map>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

// Computes deep SHA-256 hashes of UsdShade material networks.
// Hashes include: shader IDs, input values, texture paths, connection topology.
class USDCLEANER_API MaterialHasher {
public:
    // Compute the hash of a material's entire shader network
    HashDigest HashMaterial(const UsdShadeMaterial& material);

    // Check if a material has animated (time-sampled) inputs
    bool HasAnimatedInputs(const UsdShadeMaterial& material) const;

private:
    // Recursively hash a shader node and its inputs
    void HashShader(const UsdShadeShader& shader, SHA256Hasher& hasher);

    // Hash a single input value
    void HashInputValue(const VtValue& value, SHA256Hasher& hasher);
};

} // namespace usdcleaner
