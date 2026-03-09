"""Configuration file parsing for USDCleaner."""

import json
from pathlib import Path


def load_config(path: str) -> dict:
    """Load a USDCleaner configuration file (JSON or YAML)."""
    config_path = Path(path)

    if not config_path.exists():
        raise FileNotFoundError(f"Config file not found: {path}")

    text = config_path.read_text(encoding="utf-8")

    if config_path.suffix in (".yaml", ".yml"):
        try:
            import yaml
            return yaml.safe_load(text)
        except ImportError:
            raise ImportError("PyYAML is required for YAML config files: pip install pyyaml")
    else:
        return json.loads(text)


def config_to_processor_config(config: dict):
    """Convert a parsed config dict to a ProcessorConfig object."""
    from . import ProcessorConfig

    pc = ProcessorConfig()
    passes = config.get("pipeline", {}).get("passes", {})

    weld = passes.get("vertex_welding", {})
    pc.enable_welding = weld.get("enabled", True)
    if weld.get("epsilon") == "auto":
        pc.auto_epsilon = True
    else:
        pc.auto_epsilon = False
        pc.welding_epsilon = weld.get("epsilon_value", 1e-5)

    pc.enable_degenerate_removal = passes.get(
        "degenerate_face_removal", {}).get("enabled", True)

    lamina = passes.get("lamina_face_removal", {})
    pc.enable_lamina_removal = lamina.get("enabled", True)
    pc.keep_opposite_winding = lamina.get("keep_opposite_winding", False)

    mat = passes.get("material_deduplication", {})
    pc.enable_material_dedup = mat.get("enabled", True)
    pc.skip_animated_materials = mat.get("skip_animated", True)

    cache = passes.get("gpu_cache_optimization", {})
    pc.enable_cache_optimization = cache.get("enabled", True)
    pc.triangulate = cache.get("triangulate", False)

    output = config.get("pipeline", {}).get("output", {})
    pc.output_format = output.get("format", "usdc")

    return pc
