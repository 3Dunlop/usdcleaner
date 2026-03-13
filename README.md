# USDCleaner

High-performance USD geometry and material optimization pipeline for BIM data exported from Autodesk Navisworks via FBX.

## What It Does

BIM data exported from Navisworks produces heavily bloated USD stages: disconnected vertices at every face boundary, thousands of cloned materials with numerical suffixes, redundant metadata, deep single-child Xform chains, and zero geometric instancing. USDCleaner applies rigorous topological and semantic optimization to produce real-time-ready `.usdc` assets for NVIDIA Omniverse, Unreal Engine, and similar Hydra-based renderers.

### Optimization Pipeline

| Pass | Description | Default |
|------|-------------|---------|
| **Metadata Stripping** | Removes authored `customData['userDocBrief']`, empty arrays, None-valued attributes (preserving time-sampled data and UsdShade connections), and redundant subdivision defaults | On |
| **Identity Xform Stripping** | Clears identity xformOps (transforms that compose to the identity matrix), preserving animated transforms | On |
| **Vertex Welding** | Spatial-hash-based O(N) welding of coincident vertices with auto-epsilon detection and bounds validation | On |
| **Degenerate Face Removal** | Removes faces with < 3 unique vertex indices (created by welding), compacting faceVarying primvars | On |
| **Lamina Face Removal** | Removes duplicate faces via canonical hash with secondary comparison to prevent false positives | On |
| **Material Deduplication** | Deep SHA-256 hashing of full shader networks including material interface inputs; rebinds direct and per-face-subset material bindings | On |
| **Geometric Instancing** | Replaces groups of identical meshes with `UsdGeomPointInstancer` prims with descriptive prototype names, preserving material bindings | Off |
| **Hierarchy Flattening** | Collapses single-child Xform chains, composing transforms bottom-up, preserving animated transforms | Off |
| **GPU Cache Optimization** | meshoptimizer-based vertex cache and fetch optimization with full primvar remapping | On |

### Pass Ordering

```
MetadataStripper → IdentityXformStripper → VertexWelding → DegenerateFaceRemoval →
LaminaFaceRemoval → MaterialDeduplication → [PointInstancerAuthor] → [HierarchyFlattener] →
GpuCacheOptimization
```

Bracketed passes are off by default and enabled via CLI flags.

## Requirements

- **OS**: Windows 10/11 (x64)
- **Compiler**: MSVC 2022 (v143), C++17
- **CMake**: 3.26+
- **OpenUSD**: NVIDIA pre-built v25.08 ([download](https://developer.nvidia.com/usd))
- **vcpkg**: For meshoptimizer, Google Test, CLI11

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

### 4. Run Tests

Tests require USD and Python DLLs on PATH. Use the provided PowerShell scripts:

```powershell
# Run all 42 tests (11 suites)
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
  --min-instance-count 3 \
  --enable-hierarchy-flatten \
  --format usdc

# Batch directory
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
    pipeline/     # OptimizationPass, Pipeline, StageProcessor, BatchProcessor
  cli/            # CLI executable
  bindings/       # Python bindings (boost::python)
python/           # Python package (orchestration layer)
tests/
  cpp/            # Google Test unit and integration tests (42 tests, 11 suites)
  data/           # Test USD files
cmake/            # CMake modules and USD dependency stubs
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
| `test_instancing` | 3 | Geometry hashing, PointInstancer creation |
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
- **Descriptive prototype naming**: PointInstancerAuthor derives prototype names from source mesh/parent names (e.g., `proto_WallPanel`) instead of generic `proto_0` numbering.
- **Concave polygon safety**: GpuCacheOptimizer detects n-gons (faces with >4 vertices) and skips meshes containing them to avoid incorrect fan triangulation.

## Real BIM File Results

Tested on 12 Navisworks USD files (1,606 MB total input) with the full 9-pass pipeline:

| File | Input | Output | Materials In → Out | Time |
|------|-------|--------|-------------------|------|
| CIV-50001 | 0.25 MB | 0.41 MB | 13 → 5 | 0.1s |
| CIV-50002 | 10.09 MB | 10.03 MB | 310 → 5 | 1.0s |
| STR-50002 | 10.29 MB | 9.96 MB | 3,523 → 2 | 4.8s |
| ARC-50001 | 97.59 MB | 97.28 MB | 7,129 → 83 | 19.2s |
| ARC-50002 | 16.25 MB | 15.74 MB | 3,352 → 29 | 5.4s |
| FAC-50003 | 690.16 MB | 689.08 MB | 6,424 → 33 | 45.9s |
| FCT-50001 | 2.89 MB | 2.89 MB | 129 → 2 | 0.6s |
| RND-INT | 713.90 MB | 710.62 MB | 28,319 → 81 | 119.3s |
| LSE-50001 | 22.56 MB | 22.51 MB | 877 → 26 | 2.5s |
| LSE-50002 | 5.65 MB | 5.74 MB | 2,079 → 7 | 1.7s |
| WFT-50001 | 30.04 MB | 30.24 MB | 1,360 → 4 | 4.7s |
| SGN-50001 | 6.58 MB | 6.70 MB | 111 → 6 | 2.0s |
| **Total** | **1,606 MB** | **1,601 MB** | **53,626 → 283** | **207s** |

Material deduplication is the primary win: 53,626 materials reduced to 283 unique materials across all files. File size reduction is modest (0.31%) because BIM geometry is already dense and each element tends to be geometrically unique — the main value is scene graph cleanliness and runtime performance (124K intermediate Xform prims removed, duplicate materials pruned by 99.5%).

## License

MIT
