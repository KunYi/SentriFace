#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate a mock embedding replay manifest from offline_tracker_timeline.csv."
    )
    parser.add_argument("timeline_csv", help="Path to offline_tracker_timeline.csv")
    parser.add_argument("output_manifest", help="Path to output embedding manifest")
    parser.add_argument(
        "--track-ids",
        default="",
        help="Comma-separated track ids to include. Default: all track ids.",
    )
    parser.add_argument(
        "--vector",
        default="1.0,0.0,0.0,0.0",
        help="Comma-separated embedding vector to assign to selected tracks.",
    )
    parser.add_argument(
        "--confirmed-only",
        action="store_true",
        help="Only emit rows where state == 1 (confirmed).",
    )
    return parser.parse_args()


def parse_track_ids(value: str):
    if not value:
        return set()
    return {int(item.strip()) for item in value.split(",") if item.strip()}


def parse_vector(value: str):
    vector = [float(item.strip()) for item in value.split(",") if item.strip()]
    if not vector:
        raise ValueError("embedding vector cannot be empty")
    return vector


def main():
    args = parse_args()
    timeline_path = Path(args.timeline_csv)
    output_path = Path(args.output_manifest)

    selected_track_ids = parse_track_ids(args.track_ids)
    vector = parse_vector(args.vector)

    rows = list(csv.DictReader(timeline_path.open()))
    output_path.parent.mkdir(parents=True, exist_ok=True)

    written = 0
    with output_path.open("w", encoding="utf-8") as handle:
        for row in rows:
            track_id = int(row["track_id"])
            if selected_track_ids and track_id not in selected_track_ids:
                continue
            if args.confirmed_only and int(row["state"]) != 1:
                continue
            values = [row["timestamp_ms"], row["track_id"]]
            values.extend(str(v) for v in vector)
            handle.write(" ".join(values) + "\n")
            written += 1

    print(f"written_rows={written}")
    print(f"output_manifest={output_path}")


if __name__ == "__main__":
    main()
