#pragma once

// DLL export/import macros
#ifdef _WIN32
    #ifdef USDCLEANER_CORE_EXPORTS
        #define USDCLEANER_API __declspec(dllexport)
    #else
        #define USDCLEANER_API __declspec(dllimport)
    #endif
#else
    #define USDCLEANER_API __attribute__((visibility("default")))
#endif

#include <pxr/pxr.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/path.h>

#include <cstdint>
#include <string>
#include <vector>
#include <array>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

// Common array types matching USD's VtArray types
using IndexArray = VtArray<int>;
using PointArray = VtArray<GfVec3f>;
using NormalArray = VtArray<GfVec3f>;
using UVArray = VtArray<GfVec2f>;

// SHA-256 digest: 32 bytes
using HashDigest = std::array<uint8_t, 32>;

// Vertex remap table: old_index -> new_index
using RemapTable = std::vector<uint32_t>;

} // namespace usdcleaner
