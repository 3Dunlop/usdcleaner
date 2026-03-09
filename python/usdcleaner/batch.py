"""Batch processing orchestration for USDCleaner."""

from pathlib import Path
from typing import List


def collect_usd_files(directory: str, extensions: tuple = (".usd", ".usda", ".usdc")) -> List[str]:
    """Collect all USD files in a directory."""
    dir_path = Path(directory)
    if not dir_path.is_dir():
        raise NotADirectoryError(f"Not a directory: {directory}")

    files = []
    for ext in extensions:
        files.extend(str(p) for p in dir_path.glob(f"*{ext}"))

    return sorted(files)


def process_batch(input_dir: str, output_dir: str, config=None):
    """Process all USD files in a directory."""
    from . import BatchConfig, BatchProcessor, ProcessorConfig

    files = collect_usd_files(input_dir)
    if not files:
        print(f"No USD files found in {input_dir}")
        return

    batch_config = BatchConfig()
    if config:
        batch_config.processor_config = config
    batch_config.output_directory = output_dir

    processor = BatchProcessor(batch_config)
    processor.process_files(files)
