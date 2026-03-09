#pragma once

#include "core/common/Types.h"

#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdcleaner {

// Handles remapping of primvars (normals, UVs, colors, etc.) after vertex welding.
//
// Interpolation modes and behavior:
//   - vertex/varying: data is per-vertex, must be compacted using remap table
//   - faceVarying: data is per face-vertex, left unchanged (maps to faceVertexIndices)
//   - uniform: data is per-face, left unchanged
//   - constant: single value, left unchanged
class USDCLEANER_API PrimvarRemapper {
public:
    // Remap all primvars on a mesh according to the vertex remap table.
    // newVertexCount is the size of the compacted points array.
    static void RemapPrimvars(UsdGeomMesh& mesh,
                               const RemapTable& remap,
                               size_t newVertexCount);

private:
    // Compact a vertex-interpolated VtArray using the remap table.
    // For merged vertices, takes the value from the first mapped vertex.
    template<typename T>
    static VtArray<T> CompactVertexData(const VtArray<T>& data,
                                         const RemapTable& remap,
                                         size_t newCount);
};

} // namespace usdcleaner
