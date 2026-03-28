#!/usr/bin/env python3

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Convert embedding CSV into offline embedding replay manifest by matching tracker timeline."
    )
    parser.add_argument("embedding_csv", help="Path to embedding CSV.")
    parser.add_argument("tracker_timeline_csv", help="Path to offline_tracker_timeline.csv.")
    parser.add_argument("output_manifest", help="Path to output embedding manifest.")
    parser.add_argument(
        "--frame-manifest",
        default="",
        help="Optional frame manifest for resolving frame_index to timestamp_ms.",
    )
    parser.add_argument(
        "--iou-threshold",
        type=float,
        default=0.5,
        help="Minimum IoU required to match embedding rows to tracks. Default: 0.5.",
    )
    return parser.parse_args()


def load_frame_timestamps(path: str):
    if not path:
        return []
    timestamps = []
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            parts = stripped.split()
            timestamps.append(int(parts[1]))
    return timestamps


def resolve_timestamp(row, frame_timestamps):
    if "timestamp_ms" in row and row["timestamp_ms"]:
        return int(row["timestamp_ms"])
    frame_index = int(row["frame_index"])
    if not frame_timestamps:
        raise ValueError("frame manifest is required when embedding CSV has no timestamp_ms")
    return frame_timestamps[frame_index]


def iou(ax, ay, aw, ah, bx, by, bw, bh):
    ax2 = ax + aw
    ay2 = ay + ah
    bx2 = bx + bw
    by2 = by + bh
    x1 = max(ax, bx)
    y1 = max(ay, by)
    x2 = min(ax2, bx2)
    y2 = min(ay2, by2)
    iw = max(0.0, x2 - x1)
    ih = max(0.0, y2 - y1)
    inter = iw * ih
    union = aw * ah + bw * bh - inter
    return inter / union if union > 0.0 else 0.0


def main():
    args = parse_args()
    embedding_csv = Path(args.embedding_csv)
    tracker_timeline_csv = Path(args.tracker_timeline_csv)
    output_manifest = Path(args.output_manifest)

    frame_timestamps = load_frame_timestamps(args.frame_manifest)

    tracker_rows = list(csv.DictReader(tracker_timeline_csv.open()))
    tracks_by_timestamp = defaultdict(list)
    for row in tracker_rows:
        tracks_by_timestamp[int(row["timestamp_ms"])].append(row)

    embedding_rows = list(csv.DictReader(embedding_csv.open()))
    embedding_fields = [field for field in embedding_rows[0].keys() if field.startswith("e")] if embedding_rows else []

    output_manifest.parent.mkdir(parents=True, exist_ok=True)
    written = 0
    with output_manifest.open("w", encoding="utf-8") as handle:
        for row in embedding_rows:
            timestamp_ms = resolve_timestamp(row, frame_timestamps)
            candidates = tracks_by_timestamp.get(timestamp_ms, [])
            if not candidates:
                continue

            ex = float(row["x"])
            ey = float(row["y"])
            ew = float(row["w"])
            eh = float(row["h"])
            best_track = None
            best_iou = -1.0
            for candidate in candidates:
                score = iou(
                    ex,
                    ey,
                    ew,
                    eh,
                    float(candidate["bbox_x"]),
                    float(candidate["bbox_y"]),
                    float(candidate["bbox_w"]),
                    float(candidate["bbox_h"]),
                )
                if score > best_iou:
                    best_iou = score
                    best_track = candidate

            if best_track is None or best_iou < args.iou_threshold:
                continue

            values = [str(timestamp_ms), best_track["track_id"]]
            values.extend(row[field] for field in embedding_fields)
            handle.write(" ".join(values) + "\n")
            written += 1

    print(f"written_rows={written}")
    print(f"output_manifest={output_manifest}")


if __name__ == "__main__":
    main()
