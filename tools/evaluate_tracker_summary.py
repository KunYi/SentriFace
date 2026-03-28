#!/usr/bin/env python3

import argparse
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Evaluate tracker_summary.txt against simple stage-1 heuristics."
    )
    parser.add_argument("summary_txt", help="Path to tracker_summary.txt")
    parser.add_argument("output_txt", help="Path to output evaluation txt")
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

    frames_with_tracks = int(values.get("frames_with_tracks", "0"))
    unique_track_ids = int(values.get("unique_track_ids", "0"))
    confirmed = int(values.get("state_confirmed", "0"))
    lost = int(values.get("state_lost", "0"))

    findings = []
    if frames_with_tracks == 0:
        findings.append("No tracked frames were recorded.")
    if unique_track_ids <= 1:
        findings.append("Single track id across the clip; this is ideal for a single-person controlled sequence.")
    elif unique_track_ids <= 3:
        findings.append("Track id count is acceptable for stage-1 webcam validation, but may indicate temporary ID fragmentation.")
    else:
        findings.append("Track id count is high for a short single-person clip; likely fragmentation risk.")

    lost_ratio = (lost / confirmed) if confirmed > 0 else 0.0
    if confirmed > 0 and lost_ratio <= 0.75:
        findings.append(f"Lost/confirmed ratio is acceptable for stage-1 validation ({lost_ratio:.2f}).")
    elif confirmed > 0:
        findings.append(f"Lost/confirmed ratio is high ({lost_ratio:.2f}); tracker continuity should be reviewed.")

    strong_tracks = []
    for track in per_track:
        max_hits = int(track.get("max_hits", "0"))
        best_score = float(track.get("best_score", "0.0"))
        if max_hits >= 4 and best_score >= 0.70:
            strong_tracks.append(track.get("track_id", "?"))
    if strong_tracks:
        findings.append("At least one strong confirmed track was observed: " + ",".join(strong_tracks))
    else:
        findings.append("No strong confirmed track reached the current stage-1 heuristic.")

    overall = "pass"
    if unique_track_ids > 3 or (confirmed > 0 and lost_ratio > 1.0):
        overall = "review"

    lines = [
        f"overall={overall}",
        f"frames_with_tracks={frames_with_tracks}",
        f"unique_track_ids={unique_track_ids}",
        f"state_confirmed={confirmed}",
        f"state_lost={lost}",
        "findings=",
    ]
    lines.extend(findings)

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(output_path.read_text(encoding="utf-8"), end="")


if __name__ == "__main__":
    main()
