# USDCleaner

High-performance USD geometry and material optimization pipeline for BIM data exported from Autodesk Navisworks via FBX.

## What It Does

BIM data exported from Navisworks produces heavily bloated USD stages: disconnected vertices at every face boundary, thousands of cloned materials with numerical suffixes, redundant metadata, deep single-child Xform chains, and zero geometric instancing. USDCleaner applies rigorous topological and semantic optimization to produce real-time-ready `.usdc` assets for NVIDIA Omniverse, Unreal Engine, and similar Hydra-based renderers.

### Optimization Pipeline

| Pass | Description | Default |
|------|-------------|---------|
| **Metadata Stripping** | Removes authored `customData['userDocBrief']`, empty arrays, None-valued attributes (preserving time-sampled data), and redundant subdivision defaults | On |
| **Identity Xform Stripping** | Clears identity xformOps (transforms that compose to the identity matrix), preserving animated transforms | On |
| **Vertex Welding** | Spatial-hash-based O(N) welding of coincident vertices with auto-epsilon detection and bounds validation | On |
| **Degenerate Face Removal** | Removes faces with < 3 unique vertex indices (created by welding), compacting faceVarying primvars | On |
| **Lamina Face Removal** | Removes duplicate faces via canonical hash with secondary comparison to prevent false positives | On |
| **Material Deduplication** | Deep SHA-256 hashing of shader networks; rebinds direct and per-face-subset material bindings | On |
| **Geometric Instancing** | Replaces groups of identical meshes with `UsdGeomPointInstancer` prims, preserving material bindings | Off |
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
# Run all 39 tests (11 suites)
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
  cpp/            # Google Test unit and integration tests (39 tests, 11 suites)
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
| `test_regression_fixes` | 7 | Regression tests for Phase 1+2 bug fixes |

## Correctness Guarantees

The optimization passes include the following safety measures:

- **Time-sampled data preservation**: All passes (MetadataStripper, IdentityXformStripper, HierarchyFlattener, PointInstancerAuthor) check for time-sampled attributes and skip animated data to prevent destroying animations.
- **CustomData scoping**: MetadataStripper only removes `userDocBrief` from `customData` dictionaries, preserving other authored keys (BIM properties, user annotations).
- **Face deduplication accuracy**: LaminaFaceRemover uses a two-level approach (hash + secondary index comparison) to prevent false-positive removal from hash collisions.
- **Primvar consistency**: Face removal passes compact faceVarying and uniform primvars in lockstep with topology changes. GPU cache optimization remaps all vertex-interpolated primvars when reordering vertices.
- **Index bounds validation**: VertexWelder validates all remapped indices against the new points array size, logging errors for out-of-bounds indices instead of producing corrupt geometry.
- **Material binding preservation**: PointInstancerAuthor extracts and reapplies material bindings to prototypes. MaterialDeduplicator handles both direct and per-face-subset bindings.
- **Concave polygon safety**: GpuCacheOptimizer detects n-gons (faces with >4 vertices) and skips meshes containing them to avoid incorrect fan triangulation.

## Real BIM File Results

Tested on 12 Navisworks USD files (1.6 GB total input):

| Pipeline | Output Size | Reduction |
|----------|-------------|-----------|
| Default 7-pass | 1596.30 MB | 0.62% |
| Full 9-pass | 1601.15 MB | 0.32% |

The full pipeline produces slightly larger output than default because instancing adds PointInstancer overhead that outweighs savings for BIM data (where each element is geometrically unique). The primary value of the full pipeline is scene graph cleanliness and runtime performance (up to 124K intermediate Xform prims removed, 28K duplicate materials pruned).

## License

MIT
