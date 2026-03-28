#!/usr/bin/env python3

import argparse
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Evaluate decision_summary.txt against simple access-control heuristics."
    )
    parser.add_argument("summary_txt", help="Path to decision_summary.txt")
    parser.add_argument("output_txt", help="Path to output evaluation txt")
    parser.add_argument(
        "--baseline-summary",
        default="",
        help="Optional baseline summary for before/after comparison.",
    )
    return parser.parse_args()


def parse_summary(path: Path):
    values = {}
    per_track = []
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line == "per_track=":
            continue
        if line.startswith("track_id="):
            parts = {}
            for item in line.split(","):
                key, value = item.split("=", 1)
                parts[key] = value
            per_track.append(parts)
        elif "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values, per_track


def main():
    args = parse_args()
    summary_path = Path(args.summary_txt)
    output_path = Path(args.output_txt)

    values, per_track = parse_summary(summary_path)

    rows = int(values.get("rows", "0"))
    unique_track_ids = int(values.get("unique_track_ids", "0"))
    unlock_ready_rows = int(values.get("unlock_ready_rows", "0"))
    max_short_gap_merges = int(values.get("max_short_gap_merges", "0"))

    findings = []
    if rows == 0:
      findings.append("No decision rows were recorded.")
    if unique_track_ids <= 1:
      findings.append("Single decision track across the clip; ideal for a controlled single-person session.")
    elif unique_track_ids <= 3:
      findings.append("Decision layer still sees multiple track ids, but this remains acceptable for stage-1 webcam validation.")
    else:
      findings.append("Decision layer still sees many track ids; session continuity should be reviewed.")

    if unlock_ready_rows > 0:
      findings.append(f"Unlock-ready was observed on {unlock_ready_rows} decision rows.")
    else:
      findings.append("No unlock-ready rows were observed.")

    if max_short_gap_merges > 0:
      findings.append(f"Short-gap merge triggered {max_short_gap_merges} time(s).")
    else:
      findings.append("No short-gap merge was observed in this replay.")

    strong_tracks = []
    for track in per_track:
      if int(track.get("max_consistent_hits", "0")) >= 3 and int(track.get("unlock_ready_seen", "0")) != 0:
        strong_tracks.append(track.get("track_id", "?"))
    if strong_tracks:
      findings.append("At least one strong decision session was observed on track ids: " + ",".join(strong_tracks))
    else:
      findings.append("No strong decision session reached the current stage-1 heuristic.")

    comparison_lines = []
    if args.baseline_summary:
      baseline_values, _ = parse_summary(Path(args.baseline_summary))
      baseline_unlock_ready_rows = int(baseline_values.get("unlock_ready_rows", "0"))
      baseline_short_gap_merges = int(baseline_values.get("max_short_gap_merges", "0"))
      unlock_delta = unlock_ready_rows - baseline_unlock_ready_rows
      merge_delta = max_short_gap_merges - baseline_short_gap_merges
      comparison_lines.append(
          f"baseline_unlock_ready_rows={baseline_unlock_ready_rows}"
      )
      comparison_lines.append(f"unlock_ready_row_delta={unlock_delta}")
      comparison_lines.append(
          f"baseline_max_short_gap_merges={baseline_short_gap_merges}"
      )
      comparison_lines.append(f"short_gap_merge_delta={merge_delta}")
      if unlock_delta > 0:
        findings.append("Compared with baseline, unlock-ready coverage improved.")
      if merge_delta > 0:
        findings.append("Compared with baseline, short-gap merge activity increased.")

    overall = "pass"
    if rows == 0 or unlock_ready_rows == 0:
      overall = "review"

    lines = [
        f"overall={overall}",
        f"rows={rows}",
        f"unique_track_ids={unique_track_ids}",
        f"unlock_ready_rows={unlock_ready_rows}",
        f"max_short_gap_merges={max_short_gap_merges}",
    ]
    lines.extend(comparison_lines)
    lines.append("findings=")
    lines.extend(findings)

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(output_path.read_text(encoding="utf-8"), end="")


if __name__ == "__main__":
    main()
