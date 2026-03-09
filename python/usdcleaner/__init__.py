"""
USDCleaner: High-performance USD geometry & material optimization pipeline.

Optimizes Navisworks-derived USD files by welding vertices, removing duplicate
faces, deduplicating materials, and optimizing for GPU rendering.
"""

__version__ = "0.1.0"

try:
    from ._usdcleaner import (
        ProcessorConfig,
        StageProcessor,
        BatchConfig,
        BatchProcessor,
        VertexWelder,
        DegenerateFaceRemover,
        LaminaFaceRemover,
        MaterialDeduplicator,
        GpuCacheOptimizer,
        PassMetrics,
        MetricsCollector,
    )
except ImportError:
    import warnings
    warnings.warn(
        "USDCleaner C++ bindings not available. "
        "Build the project with -DUSDCLEANER_BUILD_BINDINGS=ON.",
        ImportWarning
    )
