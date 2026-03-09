"""Metrics reporting for USDCleaner."""

import json
from pathlib import Path


def write_json_report(metrics_json: str, output_path: str):
    """Write metrics JSON to a file."""
    Path(output_path).write_text(metrics_json, encoding="utf-8")


def format_summary(metrics_json: str) -> str:
    """Format a human-readable summary from metrics JSON."""
    data = json.loads(metrics_json)
    lines = ["USDCleaner Optimization Report", "=" * 40]

    for p in data.get("passes", []):
        name = p.get("name", "Unknown")
        lines.append(f"\nPass: {name}")
        lines.append(f"  Vertices: {p.get('totalVertices', 0)} "
                      f"(removed: {p.get('verticesRemoved', 0)})")
        lines.append(f"  Faces: {p.get('totalFaces', 0)} "
                      f"(removed: {p.get('facesRemoved', 0)})")
        lines.append(f"  Materials: {p.get('materialCount', 0)} "
                      f"(removed: {p.get('materialsRemoved', 0)})")

    return "\n".join(lines)
