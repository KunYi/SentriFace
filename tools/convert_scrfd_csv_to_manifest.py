#!/usr/bin/env python3

import argparse
import csv
import sys
from pathlib import Path


REQUIRED_FLOAT_FIELDS = [
    "x",
    "y",
    "w",
    "h",
    "score",
    "l0x",
    "l0y",
    "l1x",
    "l1y",
    "l2x",
    "l2y",
    "l3x",
    "l3y",
    "l4x",
    "l4y",
]


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Convert desktop SCRFD CSV output into offline detection replay manifest."
        )
    )
    parser.add_argument("input_csv", help="Path to input CSV file.")
    parser.add_argument("output_manifest", help="Path to output detection manifest.")
    parser.add_argument(
        "--frame-manifest",
        dest="frame_manifest",
        default="",
        help=(
            "Optional frame manifest. If provided, rows may use frame_index instead of "
            "timestamp_ms."
        ),
    )
    parser.add_argument(
        "--default-timestamp-step-ms",
        dest="default_step_ms",
        type=int,
        default=33,
        help=(
            "Fallback timestamp step when timestamp_ms is absent and frame_manifest is not "
            "provided. Default: 33."
        ),
    )
    return parser.parse_args()


def load_frame_manifest(path: str):
    if not path:
        return []

    timestamps = []
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            parts = stripped.split()
            if len(parts) < 2:
                raise ValueError(f"invalid frame manifest line: {stripped}")
            timestamps.append(int(parts[1]))
    return timestamps


def resolve_timestamp(row, row_index, frame_timestamps, default_step_ms):
    if row.get("timestamp_ms", "") != "":
        return int(row["timestamp_ms"])

    if row.get("frame_index", "") != "":
        frame_index = int(row["frame_index"])
        if frame_timestamps:
            if frame_index < 0 or frame_index >= len(frame_timestamps):
                raise ValueError(f"frame_index out of range: {frame_index}")
            return frame_timestamps[frame_index]
        return frame_index * default_step_ms

    if frame_timestamps:
        if row_index < 0 or row_index >= len(frame_timestamps):
            raise ValueError(f"row index out of frame manifest range: {row_index}")
        return frame_timestamps[row_index]

    return row_index * default_step_ms


def require_headers(fieldnames):
    if fieldnames is None:
        raise ValueError("input CSV has no header")

    missing = [name for name in REQUIRED_FLOAT_FIELDS if name not in fieldnames]
    if missing:
        raise ValueError(f"missing required CSV columns: {', '.join(missing)}")

    if "timestamp_ms" not in fieldnames and "frame_index" not in fieldnames:
        raise ValueError("CSV must contain either timestamp_ms or frame_index column")


def main():
    args = parse_args()

    input_csv = Path(args.input_csv)
    output_manifest = Path(args.output_manifest)
    frame_timestamps = load_frame_manifest(args.frame_manifest)

    if not input_csv.is_file():
        print(f"input_csv does not exist: {input_csv}", file=sys.stderr)
        return 2

    output_manifest.parent.mkdir(parents=True, exist_ok=True)

    rows_written = 0
    with open(input_csv, "r", encoding="utf-8", newline="") as src:
        reader = csv.DictReader(src)
        require_headers(reader.fieldnames)

        with open(output_manifest, "w", encoding="utf-8") as dst:
            for row_index, row in enumerate(reader):
                timestamp_ms = resolve_timestamp(
                    row, row_index, frame_timestamps, args.default_step_ms
                )
                values = [str(timestamp_ms)]
                for field in REQUIRED_FLOAT_FIELDS:
                    values.append(str(float(row[field])))
                dst.write(" ".join(values))
                dst.write("\n")
                rows_written += 1

    print(f"converted_rows={rows_written}")
    print(f"output_manifest={output_manifest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
