#!/usr/bin/env python3

import argparse
import csv
import os
import subprocess
import tempfile
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Sweep a small set of tracker params against offline replay artifacts."
    )
    parser.add_argument("frame_manifest", help="Path to frame manifest")
    parser.add_argument("detection_manifest", help="Path to detection manifest")
    parser.add_argument("output_csv", help="Path to sweep result csv")
    parser.add_argument(
        "--profile",
        choices=("baseline", "geometry"),
        default="geometry",
        help="Sweep profile to run. geometry is recommended for single-person fragmentation tuning.",
    )
    return parser.parse_args()


def run_case(frame_manifest: str, detection_manifest: str, env_overrides: dict):
    root = Path.cwd()
    with tempfile.TemporaryDirectory(prefix="sentriface_tracker_sweep_") as temp_dir:
        env = os.environ.copy()
        env.update(env_overrides)
        subprocess.run(
            [str(root / "build" / "offline_sequence_runner"), frame_manifest, detection_manifest],
            cwd=temp_dir,
            env=env,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        summary_path = Path(temp_dir) / "offline_tracker_timeline.csv"
        eval_out = Path(temp_dir) / "tracker_eval.txt"
        summary_txt = Path(temp_dir) / "tracker_summary.txt"
        subprocess.run(
            ["python3", str(root / "tools" / "summarize_tracker_timeline.py"),
             str(summary_path), str(summary_txt)],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        subprocess.run(
            ["python3", str(root / "tools" / "evaluate_tracker_summary.py"),
             str(summary_txt), str(eval_out)],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        summary = {}
        for line in summary_txt.read_text(encoding="utf-8").splitlines():
            if "=" in line and not line.startswith("track_id=") and line != "per_track=":
                key, value = line.split("=", 1)
                summary[key] = value

        evaluation = {}
        for line in eval_out.read_text(encoding="utf-8").splitlines():
            if "=" in line and line != "findings=":
                key, value = line.split("=", 1)
                evaluation[key] = value

        return {
            "overall": evaluation.get("overall", "unknown"),
            "unique_track_ids": summary.get("unique_track_ids", ""),
            "state_confirmed": summary.get("state_confirmed", ""),
            "state_lost": summary.get("state_lost", ""),
        }


def main():
    args = parse_args()
    frame_manifest = str(Path(args.frame_manifest).resolve())
    detection_manifest = str(Path(args.detection_manifest).resolve())
    output_csv = Path(args.output_csv)
    output_csv.parent.mkdir(parents=True, exist_ok=True)

    cases = []
    if args.profile == "baseline":
        for min_confirm_hits in (2, 3):
            for max_miss in (2, 3, 4):
                for iou_min in (0.15, 0.20, 0.25):
                    cases.append({
                        "SENTRIFACE_TRACKER_MIN_CONFIRM_HITS": str(min_confirm_hits),
                        "SENTRIFACE_TRACKER_MAX_MISS": str(max_miss),
                        "SENTRIFACE_TRACKER_IOU_MIN": f"{iou_min:.2f}",
                        "SENTRIFACE_TRACKER_CENTER_GATE_RATIO": "0.60",
                        "SENTRIFACE_TRACKER_AREA_RATIO_MIN": "0.50",
                        "SENTRIFACE_TRACKER_AREA_RATIO_MAX": "2.00",
                    })
    else:
        for center_gate_ratio in (0.45, 0.60, 0.75):
            for area_ratio_min in (0.35, 0.50, 0.65):
                for area_ratio_max in (1.60, 2.00, 2.40):
                    cases.append({
                        "SENTRIFACE_TRACKER_MIN_CONFIRM_HITS": "2",
                        "SENTRIFACE_TRACKER_MAX_MISS": "2",
                        "SENTRIFACE_TRACKER_IOU_MIN": "0.20",
                        "SENTRIFACE_TRACKER_CENTER_GATE_RATIO": f"{center_gate_ratio:.2f}",
                        "SENTRIFACE_TRACKER_AREA_RATIO_MIN": f"{area_ratio_min:.2f}",
                        "SENTRIFACE_TRACKER_AREA_RATIO_MAX": f"{area_ratio_max:.2f}",
                    })

    fieldnames = [
        "min_confirm_hits",
        "max_miss",
        "iou_min",
        "center_gate_ratio",
        "area_ratio_min",
        "area_ratio_max",
        "overall",
        "unique_track_ids",
        "state_confirmed",
        "state_lost",
    ]

    with output_csv.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for case in cases:
            result = run_case(frame_manifest, detection_manifest, case)
            writer.writerow({
                "min_confirm_hits": case["SENTRIFACE_TRACKER_MIN_CONFIRM_HITS"],
                "max_miss": case["SENTRIFACE_TRACKER_MAX_MISS"],
                "iou_min": case["SENTRIFACE_TRACKER_IOU_MIN"],
                "center_gate_ratio": case["SENTRIFACE_TRACKER_CENTER_GATE_RATIO"],
                "area_ratio_min": case["SENTRIFACE_TRACKER_AREA_RATIO_MIN"],
                "area_ratio_max": case["SENTRIFACE_TRACKER_AREA_RATIO_MAX"],
                "overall": result["overall"],
                "unique_track_ids": result["unique_track_ids"],
                "state_confirmed": result["state_confirmed"],
                "state_lost": result["state_lost"],
            })

    print(f"output_csv={output_csv}")


if __name__ == "__main__":
    main()
