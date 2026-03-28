#!/usr/bin/env python3

import argparse
import csv
from collections import Counter, defaultdict
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Summarize offline_decision_timeline.csv for decision/session validation."
    )
    parser.add_argument("timeline_csv", help="Path to offline_decision_timeline.csv")
    parser.add_argument("output_txt", help="Path to output summary txt")
    return parser.parse_args()


def main():
    args = parse_args()
    timeline_path = Path(args.timeline_csv)
    output_path = Path(args.output_txt)

    rows = list(csv.DictReader(timeline_path.open()))
    unique_track_ids = sorted({int(row["track_id"]) for row in rows})
    unlock_ready_rows = [row for row in rows if int(row["unlock_ready"]) != 0]
    merge_values = [int(row["short_gap_merges"]) for row in rows]
    stable_person_ids = Counter(int(row["stable_person_id"]) for row in rows)

    per_track_rows = defaultdict(list)
    for row in rows:
        per_track_rows[int(row["track_id"])].append(row)

    lines = [
        f"rows={len(rows)}",
        f"unique_track_ids={len(unique_track_ids)}",
        f"track_ids={','.join(str(track_id) for track_id in unique_track_ids)}",
        f"unlock_ready_rows={len(unlock_ready_rows)}",
        f"max_short_gap_merges={max(merge_values) if merge_values else 0}",
        "stable_person_ids="
        + ",".join(f"{person_id}:{count}" for person_id, count in sorted(stable_person_ids.items())),
        "per_track=",
    ]

    for track_id in unique_track_ids:
        track_rows = per_track_rows[track_id]
        consistent_hits = [int(row["consistent_hits"]) for row in track_rows]
        avg_scores = [float(row["average_score"]) for row in track_rows]
        unlocks = [int(row["unlock_ready"]) for row in track_rows]
        lines.append(
            "track_id="
            f"{track_id},"
            f"samples={len(track_rows)},"
            f"max_consistent_hits={max(consistent_hits)},"
            f"best_average_score={max(avg_scores):.6f},"
            f"unlock_ready_seen={max(unlocks)}"
        )

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(output_path.read_text(encoding="utf-8"), end="")


if __name__ == "__main__":
    main()
