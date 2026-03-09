"""Python CLI entry point for USDCleaner."""

import argparse
import sys
import json


def main():
    parser = argparse.ArgumentParser(
        description="USDCleaner - USD Geometry & Material Optimization Pipeline"
    )
    parser.add_argument("input", help="Input USD file or directory")
    parser.add_argument("-o", "--output", help="Output USD file or directory")
    parser.add_argument("--metrics", help="Output metrics JSON file")
    parser.add_argument("--format", choices=["usdc", "usda"], default="usdc",
                        help="Output format (default: usdc)")
    parser.add_argument("--epsilon", type=float, default=0.0,
                        help="Manual welding epsilon (0 = auto-detect)")
    parser.add_argument("--no-weld", action="store_true",
                        help="Disable vertex welding")
    parser.add_argument("--no-degenerate", action="store_true",
                        help="Disable degenerate face removal")
    parser.add_argument("--no-lamina", action="store_true",
                        help="Disable lamina face removal")
    parser.add_argument("--no-material-dedup", action="store_true",
                        help="Disable material deduplication")
    parser.add_argument("--no-cache-opt", action="store_true",
                        help="Disable GPU cache optimization")
    parser.add_argument("--triangulate", action="store_true",
                        help="Convert to triangles during cache optimization")

    args = parser.parse_args()

    try:
        from . import ProcessorConfig, StageProcessor
    except ImportError:
        print("Error: USDCleaner C++ bindings not available.", file=sys.stderr)
        sys.exit(1)

    config = ProcessorConfig()
    config.enable_welding = not args.no_weld
    config.enable_degenerate_removal = not args.no_degenerate
    config.enable_lamina_removal = not args.no_lamina
    config.enable_material_dedup = not args.no_material_dedup
    config.enable_cache_optimization = not args.no_cache_opt
    config.triangulate = args.triangulate
    config.output_format = args.format

    if args.epsilon > 0:
        config.auto_epsilon = False
        config.welding_epsilon = args.epsilon

    output = args.output or args.input.rsplit(".", 1)[0] + "_optimized." + args.format

    processor = StageProcessor(config)
    success = processor.process(args.input, output)

    if success and args.metrics:
        with open(args.metrics, "w") as f:
            f.write(processor.get_metrics_json())
        print(f"Metrics written to: {args.metrics}")

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
