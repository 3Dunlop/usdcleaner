# USDCleaner

High-performance USD geometry and material optimization pipeline for BIM data exported from Autodesk Navisworks. Accepts both USD and FBX files directly -- no external converter needed.

## What It Does

BIM data exported from Navisworks produces heavily bloated USD stages: disconnected vertices at every face boundary, thousands of cloned materials with numerical suffixes, redundant metadata, deep single-child Xform chains, and zero geometric instancing. USDCleaner applies rigorous topological and semantic optimization to produce real-time-ready `.usdc` assets for NVIDIA Omniverse, Unreal Engine, and similar Hydra-based renderers.

With the built-in FBX import plugin, you can skip the separate FBX-to-USD conversion step entirely:

```bash
# Direct FBX to optimized USD (requires FBX plugin build)
usdcleaner model.fbx -o model_optimized.usdc

# Traditional USD input still works
usdcleaner model.usd -o model_optimized.usdc
```

### Optimization Pipeline

| Pass | Description | Default |
|------|-------------|---------|
| **FBX Import Fixup** | BIM-specific post-import corrections: up-axis (Y→Z), unit scale, empty group pruning | Auto (FBX input only) |
| **Metadata Stripping** | Removes authored `customData['userDocBrief']`, empty arrays, None-valued attributes (preserving time-sampled data and UsdShade connections), and redundant subdivision defaults | On |
| **Identity Xform Stripping** | Clears identity xformOps (transforms that compose to the identity matrix), preserving animated transforms | On |
| **Vertex Welding** | Spatial-hash-based O(N) welding of coincident vertices with auto-epsilon detection and bounds validation | On |
| **Degenerate Face Removal** | Removes faces with < 3 unique vertex indices (created by welding), compacting faceVarying primvars | On |
| **Lamina Face Removal** | Removes duplicate faces via canonical hash with secondary comparison to prevent false positives | On |
| **Material Deduplication** | Deep SHA-256 hashing of full shader networks including material interface inputs; rebinds direct and per-face-subset material bindings | On |
| **Geometric Instancing** | Centroid-normalized matching of identical meshes into `UsdGeomPointInstancer` prims with multi-material prototype support | Off |
| **Hierarchy Flattening** | Collapses single-child Xform chains, composing transforms bottom-up, preserving animated transforms | Off |
| **GPU Cache Optimization** | meshoptimizer-based vertex cache and fetch optimization with full primvar remapping | On |

### Pass Ordering

```
[FbxImportFixup] → MetadataStripper → IdentityXformStripper → VertexWelding →
DegenerateFaceRemoval → LaminaFaceRemoval → MaterialDeduplication →
[PointInstancerAuthor] → [HierarchyFlattener] → GpuCacheOptimization
```

Bracketed passes are off by default. FbxImportFixup is automatically inserted when the input is an `.fbx` file.

## Requirements

- **OS**: Windows 10/11 (x64)
- **Compiler**: MSVC 2022 (v143), C++17
- **CMake**: 3.26+
- **OpenUSD**: NVIDIA pre-built v25.08 ([download](https://developer.nvidia.com/usd))
- **vcpkg**: For meshoptimizer, Google Test, CLI11
- **Autodesk FBX SDK** (optional): 2020.3.9+ for direct FBX file import ([download](https://aps.autodesk.com/developer/overview/fbx-sdk))

## Build

### 1. Install Dependencies

```bash
# vcpkg (if not already installed)
git clone https://github.com/microsoft/vcpkg.git D:/vcpkg
D:/vcpkg/bootstrap-vcpkg.bat

# NVIDIA pre-built USD
# Download and extract to D:/USD (or your preferred location)
```

### 2. Set Environment Variables

```
VCPKG_ROOT=D:\vcpkg
USD_ROOT=D:\USD
```

### 3. Configure and Build

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake \
  -DUSD_ROOT=%USD_ROOT% \
  -DUSDCLEANER_BUILD_BINDINGS=OFF

cmake --build build --config Release
```

### 3a. With FBX Import Support (Optional)

To enable direct `.fbx` file input, install the [Autodesk FBX SDK](https://aps.autodesk.com/developer/overview/fbx-sdk) and initialize the usdFBX submodule:

```bash
git submodule update --init external/usdFBX

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake \
  -DUSD_ROOT=%USD_ROOT% \
  -DUSDCLEANER_BUILD_FBX_PLUGIN=ON \
  -DUSDCLEANER_BUILD_BINDINGS=OFF

cmake --build build --config Release
```

The FBX SDK is auto-detected from its default install location (`C:\Program Files\Autodesk\FBX\FBX SDK\`). The `usdFbx.dll` plugin is built into `build/plugin/usd/usdFbx/` and auto-discovered by the CLI at runtime.

### 4. Run Tests

Tests require USD and Python DLLs on PATH. Use the provided PowerShell scripts:

```powershell
# Run all 47 tests (11 suites)
.\run_tests.ps1

# Run a single test suite
.\run_single_test.ps1 test_metadata_stripper
```

Or manually:

```powershell
$env:PATH = "build\tests\Release;build\src\core\Release;$env:USD_ROOT\lib;$env:USD_ROOT\bin;$env:USD_ROOT\python;" + $env:PATH
build\tests\Release\test_spatial_hash.exe
build\tests\Release\test_vertex_welder.exe
# ... etc
```

> **Note**: CTest does not work due to DLL path issues on Windows. Always use the PowerShell scripts or set PATH manually.

## Usage

### CLI

```bash
# Single file (default 7 passes)
usdcleaner input.usd -o output.usdc

# Direct FBX input (requires FBX plugin build)
usdcleaner model.fbx -o model_optimized.usdc

# FBX with BIM-specific options
usdcleaner model.fbx -o output.usdc --fbx-up-axis z --fbx-unit-scale 1.0

# With options
usdcleaner input.usd -o output.usdc --epsilon 1e-5 --no-cache-opt --format usda

# Enable Phase 2 passes (instancing + hierarchy flattening)
usdcleaner input.usd -o output.usdc --enable-instancing --enable-hierarchy-flatten

# All available flags
usdcleaner input.usd -o output.usdc \
  --epsilon 1e-5 \
  --no-cache-opt \
  --no-metadata-strip \
  --no-identity-xform-strip \
  --enable-instancing \
  --min-instance-count 2 \
  --no-normalize-centroids \
  --enable-scale-normalization \
  --enable-hierarchy-flatten \
  --fbx-up-axis z \
  --fbx-unit-scale 1.0 \
  --format usdc

# Batch directory (processes .usd, .usda, .usdc, and .fbx files)
usdcleaner ./input_dir/ -o ./output_dir/
```

### Python (requires C++ bindings build)

```python
from usdcleaner import ProcessorConfig, StageProcessor

config = ProcessorConfig()
config.enable_welding = True
config.auto_epsilon = True

processor = StageProcessor(config)
processor.process("input.usd", "output.usdc")
print(processor.get_metrics_json())
```

## Project Structure

```
src/
  core/           # C++ shared library (usdcleaner_core)
    common/       # Types, SpatialHash, HashUtils, UsdUtils, Metrics
    welding/      # VertexWelder, PrimvarRemapper
    topology/     # DegenerateFaceRemover, LaminaFaceRemover
    materials/    # MaterialHasher, MaterialDeduplicator
    cache/        # GpuCacheOptimizer (meshoptimizer)
    metadata/     # MetadataStripper, IdentityXformStripper
    instancing/   # GeometryHasher, PointInstancerAuthor
    hierarchy/    # HierarchyFlattener
    import/       # FbxImportFixup (BIM post-import corrections)
    pipeline/     # OptimizationPass, Pipeline, StageProcessor, BatchProcessor
  cli/            # CLI executable (with plugin auto-discovery)
  plugin/         # USD file format plugins
    patches/      # Patched headers for usdFBX Windows build
  bindings/       # Python bindings (boost::python)
external/
  usdFBX/         # Remedy Entertainment's usdFBX SdfFileFormat plugin (submodule)
python/           # Python package (orchestration layer)
tests/
  cpp/            # Google Test unit and integration tests (47 tests, 11 suites)
  data/           # Test USD files
cmake/            # CMake modules (FindFBXSDK.cmake, USD dependency stubs)
```

## Architecture

- **C++ core + Python orchestration**: All computation in C++. Python layer is orchestration only.
- **Pass-based pipeline**: Each optimization is an `OptimizationPass` with `Execute(UsdStageRefPtr)`.
- **Pass ordering**: Metadata -> Xforms -> Geometry -> Materials -> Instancing -> Hierarchy -> Cache.
- **Metrics**: Before/after snapshots per pass, serialized to JSON.
- **Batch**: TBB parallel_for over independent files (when available).

## Test Suites

| Suite | Tests | Description |
|-------|-------|-------------|
| `test_spatial_hash` | 6 | Spatial hash grid merging and compaction |
| `test_vertex_welder` | 3 | Vertex welding on cube geometry |
| `test_degenerate` | 2 | Degenerate face detection and removal |
| `test_lamina` | 1 | Duplicate face removal |
| `test_material_hasher` | 2 | Material SHA-256 hashing consistency |
| `test_pipeline` | 3 | Full pipeline integration, idempotency, JSON metrics |
| `test_metadata_stripper` | 4 | customData removal, empty arrays, subdiv defaults |
| `test_identity_xform` | 4 | Identity transform detection and clearing |
| `test_instancing` | 8 | Geometry hashing, PointInstancer, centroid normalization, multi-material |
| `test_hierarchy` | 4 | Single-child chain flattening, safety checks |
| `test_regression_fixes` | 10 | Regression tests for v3 bug fixes + v4 material fixes |

## Correctness Guarantees

The optimization passes include the following safety measures:

- **Time-sampled data preservation**: All passes (MetadataStripper, IdentityXformStripper, HierarchyFlattener, PointInstancerAuthor) check for time-sampled attributes and skip animated data to prevent destroying animations.
- **CustomData scoping**: MetadataStripper only removes `userDocBrief` from `customData` dictionaries, preserving other authored keys (BIM properties, user annotations).
- **UsdShade connection safety**: MetadataStripper skips all inputs/outputs on Shader, Material, and NodeGraph prims, and any attribute with authored connections — preventing destruction of material surface connections and color values.
- **Material interface hashing**: MaterialHasher includes material-level interface inputs (`inputs:baseColor`, etc.) in the SHA-256 hash, not just shader-level inputs — correctly differentiating materials that share the same shader topology but have different colors/values.
- **Face deduplication accuracy**: LaminaFaceRemover uses a two-level approach (hash + secondary index comparison) to prevent false-positive removal from hash collisions.
- **Primvar consistency**: Face removal passes compact faceVarying and uniform primvars in lockstep with topology changes. GPU cache optimization remaps all vertex-interpolated primvars when reordering vertices.
- **Index bounds validation**: VertexWelder validates all remapped indices against the new points array size, logging errors for out-of-bounds indices instead of producing corrupt geometry.
- **Material binding preservation**: PointInstancerAuthor extracts and reapplies material bindings to prototypes. MaterialDeduplicator handles both direct and per-face-subset bindings.
- **Centroid-normalized instancing**: Meshes with identical shape but different local-space offsets (common in Navisworks BIM exports) are correctly matched by translating vertices to centroid origin before hashing. Instance transforms are adjusted to compensate.
- **Multi-material instancing**: Meshes with identical geometry but different material bindings are grouped into a single PointInstancer with multiple prototypes (one per unique material), using `protoIndices` to map each instance to its material variant.
- **Descriptive prototype naming**: PointInstancerAuthor derives prototype names from source mesh/parent names (e.g., `proto_WallPanel`) instead of generic `proto_0` numbering.
- **Concave polygon safety**: GpuCacheOptimizer detects n-gons (faces with >4 vertices) and skips meshes containing them to avoid incorrect fan triangulation.

## Real BIM File Results

Tested on 12 Navisworks USD files (1,606 MB total input) with the full 9-pass pipeline:

| File | Input | Output | Materials | Meshes Instanced | Time |
|------|-------|--------|-----------|-----------------|------|
| CIV-50001 | 0.25 MB | 0.41 MB | 13 → 5 | 0/12 | 0.7s |
| CIV-50002 | 10.09 MB | 10.03 MB | 310 → 5 | 0/291 | 0.9s |
| STR-50002 | 10.29 MB | 9.92 MB | 3,523 → 2 | 362/3,522 | 5.7s |
| ARC-50001 | 97.59 MB | 97.14 MB | 7,129 → 83 | 636/6,391 | 20.9s |
| ARC-50002 | 16.25 MB | 15.66 MB | 3,352 → 29 | 1,035/3,223 | 5.4s |
| FAC-50003 | 690.16 MB | 688.71 MB | 6,424 → 33 | 762/5,690 | 40.5s |
| FCT-50001 | 2.89 MB | 2.66 MB | 129 → 2 | 61/107 | 0.5s |
| **RND-INT** | **713.90 MB** | **697.47 MB** | **28,319 → 81** | **14,064/19,082** | **114s** |
| LSE-50001 | 22.56 MB | 22.51 MB | 877 → 26 | 86/800 | 2.6s |
| LSE-50002 | 5.65 MB | 5.74 MB | 2,079 → 7 | 21/180 | 1.9s |
| WFT-50001 | 30.04 MB | 30.17 MB | 1,360 → 4 | 269/979 | 4.8s |
| SGN-50001 | 6.58 MB | 6.70 MB | 111 → 6 | 8/33 | 1.8s |
| **Total** | **1,606 MB** | **1,587 MB** | **53,626 → 283** | **17,304/40,310** | **200s** |

Key optimizations:
- **Material deduplication**: 53,626 → 283 unique materials (99.5% reduction)
- **Centroid-normalized instancing**: 17,304 meshes instanced (43% of all meshes) into 5,309 PointInstancers — up from 243 meshes (0.6%) in v4 before centroid normalization
- **RND-INT highlight**: 14,064 of 19,082 meshes (74%) instanced, reducing file from 714 MB to 697 MB
- **File size**: 19.15 MB total reduction (1.19%) — 4x more than v4's 5.04 MB (0.31%)
- **Scene graph**: 124K intermediate Xform prims flattened

## License

MIT
