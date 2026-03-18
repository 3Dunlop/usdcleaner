# Changelog

## v0.6.0 — 2026-03-17: Fix Critical FBX Geometry Loss

Fixed a critical bug where Navisworks FBX exports with duplicate sibling node names caused massive silent geometry loss during import.

### Critical Bug Fix

- **usdFBX duplicate sibling name handling** — Navisworks FBX exports create sibling FBX nodes with identical names (e.g., multiple "Floor" Xforms, multiple "Site___Earth" Meshes under the same parent). FBX format uses internal node IDs so this is valid, but USD requires unique prim paths. The usdFBX plugin's `cleanName()` function only deduplicated names that needed sanitization; valid USD identifiers passed through unchecked, causing duplicate paths in the PrimMap (last-wins = geometry overwritten). Two patches fix this:
  - `src/plugin/patches/Helpers.h`: Modified `cleanName()` to ALWAYS check `usedNames` for duplicates, even for already-valid identifiers
  - `external/usdFBX/src/UsdFbxDataReader.cpp`: Added sibling deduplication in `collectFbxNodes()` — checks `context.GetPrim(candidatePath)` and appends `_1`, `_2`, etc. if path already exists

### Impact

- **ARC-3H00**: 356 meshes → 4,289 meshes recovered (12x more geometry preserved)
- **LSE-3H31**: 22 meshes → 55 meshes (all geometry preserved)
- Previous batch results showed 74.56% file size reduction but were **invalid** — geometry was being silently dropped
- Corrected batch: 2,510 MB FBX → 2,230 MB USDC (11.16% reduction) with all geometry preserved

### FBX Batch Results (65 Navisworks files, full 10-pass pipeline)

- **Total: 2,510 MB → 2,230 MB (11.16% reduction, 280 MB saved)**
- **65/65 files processed successfully**
- Best results: LSE (landscape) 187→35 MB (81%), STR (structural) 64→35 MB (45%)
- 27/65 files grew slightly due to `Flatten()` overhead — correct behavior with all geometry preserved

---

## v0.5.0 — 2026-03-17: Direct FBX Import

Added direct FBX file support via Remedy Entertainment's usdFBX SdfFileFormat plugin. USDCleaner can now accept `.fbx` files directly as input, eliminating the need for an external FBX-to-USD converter in the BIM workflow.

### New Features

- **Direct FBX input**: `usdcleaner model.fbx -o output.usdc` works out of the box when built with `-DUSDCLEANER_BUILD_FBX_PLUGIN=ON`
- **usdFBX plugin integration**: Remedy's open-source USD file format plugin compiled as a MODULE library (`usdFbx.dll`), auto-discovered at runtime from exe-relative paths
- **FbxImportFixup pass**: BIM-specific post-import corrections automatically inserted when processing FBX files:
  - Up-axis correction (FBX default Y-up → Z-up for BIM/CAD)
  - Unit scale configuration
  - Empty group pruning (removes leaf Xforms with no mesh descendants)
- **CLI options**: `--fbx-up-axis {y,z}` and `--fbx-unit-scale <float>` for FBX import control
- **Batch FBX support**: `.fbx` files are now included in batch directory scanning
- **Pipeline::InsertPass()**: New method to insert passes at specific pipeline positions

### Build Changes

- New CMake option: `USDCLEANER_BUILD_FBX_PLUGIN` (OFF by default)
- New CMake module: `cmake/FindFBXSDK.cmake` for Autodesk FBX SDK discovery
- New git submodule: `external/usdFBX` (Remedy Entertainment's usdFBX)
- Patched `UsdFbxFileformat.h` to avoid Windows case-insensitive `VERSION`/`<version>` header conflict
- FBX SDK statically linked (`libfbxsdk-md.lib` + companion libs)

### Technical Details

- FBX stages are flattened to a writable anonymous `SdfLayer` before optimization (usdFBX provides a read-only `SdfAbstractData`)
- Plugin uses `PlugRegistry::RegisterPlugins()` with recursive `plugInfo.json` discovery
- CLI11 used in header-only mode to work around vcpkg/MSVC toolchain ABI mismatch

### FBX Test Results (Navisworks BIM export)

Tested on `QC00912021-JCB-MOD-3DM-ARC-3H-3H00-00001.fbx` (~40 MB):
- FbxImportFixup: up axis Y → Z
- MetadataStripper: redundant subdiv attrs removed
- IdentityXformStripper: identity xformOps cleared
- MaterialDeduplication: 53 → 25 materials (28 duplicates)
- Output: 25.49 MB optimized USDC (4,289 meshes preserved)

> **Note**: Initial v0.5.0 results reported 356 meshes / 4.01 MB output. This was caused by a duplicate sibling name bug (fixed in v0.6.0) that silently dropped ~92% of geometry.

---

## v0.4.0 — 2026-03-14: Centroid-Normalized Instancing (v5)

- Centroid normalization for instancing: meshes with identical shape but different offsets now match
- Multi-material prototype support in PointInstancer
- minInstanceCount lowered to 2 (default)
- Descriptive prototype naming from source mesh names
- 47 tests across 11 suites

---

## v0.3.0 — 2026-03-13: Correctness Fixes (Phase 1 + Phase 2)

Deep review of all 9 optimization passes, fixing 4 critical bugs, 5 high-severity issues, and 2 medium-severity issues. All fixes include regression tests. Verified on 12 real Navisworks BIM files (1.6 GB).

### Critical Bug Fixes

- **GpuCacheOptimizer: concave polygon safety** — Fan triangulation from vertex 0 produces self-intersecting triangles for concave polygons (common in CAD/BIM exports). Added n-gon detection; meshes with faces >4 vertices are now skipped with a warning instead of silently corrupted.

- **GpuCacheOptimizer: primvar remapping after vertex reorder** — `meshopt_optimizeVertexFetch` reorders the vertex buffer, but vertex-interpolated primvars (normals, UVs, colors) were not remapped. All vertex/varying primvars are now remapped using the same remap table, supporting 7 array types.

- **VertexWelder: index bounds validation** — Out-of-bounds indices in `faceVertexIndices` were silently left unchanged, potentially pointing beyond the new (smaller) points array. Added bounds checking with error logging.

- **MetadataStripper: customData scoping** — `ClearInfo(SdfFieldKeys->CustomData)` was removing the entire customData dictionary, not just `userDocBrief`. Now erases only the `userDocBrief` key, preserving BIM properties, user annotations, and other authored customData entries.

### High-Severity Fixes

- **Time-sampled data preservation** — Multiple passes (IdentityXformStripper, HierarchyFlattener, MetadataStripper, PointInstancerAuthor) could silently destroy animated data by evaluating only at `UsdTimeCode::Default()`. All passes now check `GetNumTimeSamples() > 0` and skip time-sampled attributes/transforms.

- **PointInstancerAuthor: material binding preservation** — When original meshes were deleted via `RemovePrim()`, their material bindings were lost. Material bindings are now extracted from the first mesh via `ComputeBoundMaterial()` before deletion and reapplied to prototypes.

- **MaterialDeduplicator: direct and subset bindings** — Was using `ComputeBoundMaterial()` (which includes inherited bindings) for rebinding, causing incorrect results. Now uses `GetDirectBinding().GetMaterial()` for direct bindings. Also added iteration over `UsdGeomSubset` children for per-face material binding deduplication.

- **LaminaFaceRemover: hash collision prevention** — Used a 64-bit multiplicative hash for face deduplication; birthday paradox guarantees collisions on large meshes. Added secondary comparison: when hashes match, actual index sequences are compared to confirm duplication before removal.

- **Face removal: faceVarying and uniform primvar compaction** — `RemoveFaces()` in UsdUtils.cpp removed face entries from topology arrays but left faceVarying primvars (per-face-vertex UVs, normals, colors) and uniform primvars (per-face data) misaligned. Both are now compacted in lockstep with face removal, supporting 9 primvar value types.

### Medium-Severity Fixes

- **PrimvarRemapper: expanded type coverage** — Only handled 4 types (Vec3f, Vec2f, float, Vec4f). Added VtIntArray, VtVec3dArray, VtVec2dArray, VtDoubleArray, VtVec4dArray. Unhandled types now log warnings instead of being silently skipped.

- **GeometryHasher: quantization overflow** — `invEps = 1.0f / positionEpsilon` multiplied by large coordinates could overflow int32. Clamped `invEps` to 1e6f maximum and switched to int64_t with double intermediate math.

### Precision Improvements

- **PointInstancerAuthor: quaternion precision** — Upgraded internal quaternion computation from GfQuath (16-bit half) to GfQuatf (32-bit float) for better precision on large-scale BIM models. Converts to GfQuath only at the final write step (required by USD schema).

### Test Additions

- Added `test_regression_fixes` suite with 7 tests covering all critical and high-severity bug fixes
- Added 3 new test data files: `customdata_mixed.usda`, `time_sampled_xforms.usda`, `facevarying_primvars.usda`
- **Total: 39 tests across 11 suites (was 32 tests, 10 suites)**

### BIM File Verification

Tested on 12 Navisworks USD files (1.6 GB total input):
- Default 7-pass: 1596.30 MB output (0.62% reduction) — 0.53 MB smaller than v2
- Full 9-pass: 1601.15 MB output (0.32% reduction) — 0.53 MB smaller than v2
- Size changes are modest (correctness fixes, not compression); some files are slightly larger because false-positive face removal is now prevented

---

## v0.2.0 — 2026-03-12: Phase 2 Passes + Metadata

- Added MetadataStripper and IdentityXformStripper passes
- Added PointInstancerAuthor (geometric instancing via UsdGeomPointInstancer)
- Added HierarchyFlattener (single-child Xform chain collapsing)
- 32 tests across 10 suites

## v0.1.0 — 2026-03-10: Initial Implementation

- Core pipeline with 5 passes: VertexWelder, DegenerateFaceRemover, LaminaFaceRemover, MaterialDeduplicator, GpuCacheOptimizer
- CLI with single-file and batch processing
- 17 tests across 6 suites
