# USDCleaner

High-performance USD geometry and material optimization pipeline for BIM data exported from Autodesk Navisworks via FBX.

## What It Does

BIM data exported from Navisworks produces heavily bloated USD stages: disconnected vertices at every face boundary, thousands of cloned materials with numerical suffixes, redundant metadata, deep single-child Xform chains, and zero geometric instancing. USDCleaner applies rigorous topological and semantic optimization to produce real-time-ready `.usdc` assets for NVIDIA Omniverse, Unreal Engine, and similar Hydra-based renderers.

### Optimization Pipeline

| Pass | Description | Default |
|------|-------------|---------|
| **Metadata Stripping** | Removes authored `customData`, empty arrays, None-valued attributes, and redundant subdivision defaults | On |
| **Identity Xform Stripping** | Clears identity xformOps (transforms that compose to the identity matrix) | On |
| **Vertex Welding** | Spatial-hash-based O(N) welding of coincident vertices with auto-epsilon detection | On |
| **Degenerate Face Removal** | Removes faces with < 3 unique vertex indices (created by welding) | On |
| **Lamina Face Removal** | Removes duplicate faces sharing identical vertex sets via canonical hash | On |
| **Material Deduplication** | Deep SHA-256 hashing of shader networks; rebinds meshes to master materials | On |
| **Geometric Instancing** | Replaces groups of identical meshes with `UsdGeomPointInstancer` prims | Off |
| **Hierarchy Flattening** | Collapses single-child Xform chains, composing transforms bottom-up | Off |
| **GPU Cache Optimization** | meshoptimizer-based vertex cache and fetch optimization | On |

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
# Run all 32 tests
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
  cpp/            # Google Test unit and integration tests (32 tests, 10 suites)
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

## License

MIT
