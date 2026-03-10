# USDCleaner

High-performance USD geometry and material optimization pipeline for BIM data exported from Autodesk Navisworks via FBX.

## What It Does

BIM data exported from Navisworks produces heavily bloated USD stages: disconnected vertices at every face boundary, thousands of cloned materials with numerical suffixes, and zero geometric instancing. USDCleaner applies rigorous topological and semantic optimization to produce real-time-ready `.usdc` assets for NVIDIA Omniverse, Unreal Engine, and similar Hydra-based renderers.

### Optimization Pipeline

| Pass | Description |
|------|-------------|
| **Vertex Welding** | Spatial-hash-based O(N) welding of coincident vertices with auto-epsilon detection |
| **Degenerate Face Removal** | Removes faces with < 3 unique vertex indices (created by welding) |
| **Lamina Face Removal** | Removes duplicate faces sharing identical vertex sets via canonical hash |
| **Material Deduplication** | Deep SHA-256 hashing of shader networks; rebinds meshes to master materials |
| **GPU Cache Optimization** | meshoptimizer-based vertex cache and fetch optimization |

### Planned (Phase 2)

- Geometric instancing via `UsdGeomPointInstancer`
- Hierarchy flattening (single-child Xform chain elimination)
- Interior face culling (Embree BVH ray casting)
- LOD generation via mesh simplification

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

Tests require USD and Python DLLs on PATH:

```powershell
$env:PATH = "build\tests\Release;build\src\core\Release;$env:USD_ROOT\lib;$env:USD_ROOT\bin;$env:USD_ROOT\python;" + $env:PATH
build\tests\Release\test_spatial_hash.exe
build\tests\Release\test_vertex_welder.exe
# ... etc
```

## Usage

### CLI

```bash
# Single file
usdcleaner input.usd -o output.usdc

# With options
usdcleaner input.usd -o output.usdc --epsilon 1e-5 --no-cache-opt --format usda

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
    pipeline/     # OptimizationPass, Pipeline, StageProcessor, BatchProcessor
  cli/            # CLI executable
  bindings/       # Python bindings (boost::python)
python/           # Python package (orchestration layer)
tests/            # Google Test unit and integration tests
cmake/            # CMake modules and USD dependency stubs
```

## Architecture

- **C++ core + Python orchestration**: All computation in C++. Python layer is orchestration only.
- **Pass-based pipeline**: Each optimization is an `OptimizationPass` with `Execute(UsdStageRefPtr)`.
- **Pass ordering**: Weld -> Degenerate -> Lamina -> MaterialDedup -> CacheOpt (strict order).
- **Metrics**: Before/after snapshots per pass, serialized to JSON.
- **Batch**: TBB parallel_for over independent files (when available).

## License

MIT
