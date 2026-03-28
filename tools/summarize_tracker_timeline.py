#!/usr/bin/env python3

import argparse
import csv
from collections import Counter, defaultdict
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Summarize offline_tracker_timeline.csv for quick tracker validation."
    )
    parser.add_argument("timeline_csv", help="Path to offline_tracker_timeline.csv")
    parser.add_argument("output_txt", help="Path to output summary txt")
    return parser.parse_args()


def main():
    args = parse_args()
    timeline_path = Path(args.timeline_csv)
    output_path = Path(args.output_txt)

    rows = list(csv.DictReader(timeline_path.open()))
    unique_timestamps = sorted({int(row["timestamp_ms"]) for row in rows})
    unique_track_ids = sorted({int(row["track_id"]) for row in rows})
    state_counts = Counter(int(row["state"]) for row in rows)

    per_track_rows = defaultdict(list)
    for row in rows:
        per_track_rows[int(row["track_id"])].append(row)

    lines = [
        f"rows={len(rows)}",
        f"frames_with_tracks={len(unique_timestamps)}",
        f"unique_track_ids={len(unique_track_ids)}",
        f"track_ids={','.join(str(track_id) for track_id in unique_track_ids)}",
        f"state_tentative={state_counts.get(0, 0)}",
        f"state_confirmed={state_counts.get(1, 0)}",
        f"state_lost={state_counts.get(2, 0)}",
        "per_track=",
    ]

    for track_id in unique_track_ids:
        track_rows = per_track_rows[track_id]
        timestamps = [int(row["timestamp_ms"]) for row in track_rows]
        hits = [int(row["hits"]) for row in track_rows]
        misses = [int(row["miss"]) for row in track_rows]
        scores = [float(row["score"]) for row in track_rows]
        lines.append(
            "track_id="
            f"{track_id},"
            f"samples={len(track_rows)},"
            f"first_ts={min(timestamps)},"
            f"last_ts={max(timestamps)},"
            f"max_hits={max(hits)},"
            f"max_miss={max(misses)},"
            f"best_score={max(scores):.6f}"
        )

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(output_path.read_text(encoding="utf-8"), end="")


if __name__ == "__main__":
    main()
