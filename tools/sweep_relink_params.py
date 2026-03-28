#!/usr/bin/env python3

import argparse
import csv
import os
import subprocess
import tempfile
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Sweep short-gap relink params against offline replay artifacts."
    )
    parser.add_argument("frame_manifest", help="Path to frame manifest")
    parser.add_argument("detection_manifest", help="Path to detection manifest")
    parser.add_argument("output_csv", help="Path to sweep result csv")
    parser.add_argument(
        "--embedding-manifest",
        default="",
        help="Optional embedding manifest for embedding-assisted relink replay.",
    )
    parser.add_argument(
        "--profile",
        choices=("baseline", "window", "embedding"),
        default="embedding",
        help="Sweep profile to run. embedding is recommended for current mainline replay work.",
    )
    return parser.parse_args()


def parse_kv_file(path: Path):
    values = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line == "per_track=" or line.startswith("track_id="):
            continue
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def run_case(frame_manifest: str,
             detection_manifest: str,
             embedding_manifest: str,
             env_overrides: dict):
    root = Path.cwd()
    with tempfile.TemporaryDirectory(prefix="sentriface_relink_sweep_") as temp_dir:
        temp_path = Path(temp_dir)
        env = os.environ.copy()
        env.update(env_overrides)

        cmd = [
            str(root / "build" / "offline_sequence_runner"),
            frame_manifest,
            detection_manifest,
        ]
        if embedding_manifest:
            cmd.append(embedding_manifest)

        subprocess.run(
            cmd,
            cwd=temp_path,
            env=env,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        timeline_path = temp_path / "offline_decision_timeline.csv"
        summary_path = temp_path / "decision_summary.txt"
        eval_path = temp_path / "decision_eval.txt"
        report_path = temp_path / "offline_sequence_report.txt"

        subprocess.run(
            [
                "python3",
                str(root / "tools" / "summarize_decision_timeline.py"),
                str(timeline_path),
                str(summary_path),
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        subprocess.run(
            [
                "python3",
                str(root / "tools" / "evaluate_decision_summary.py"),
                str(summary_path),
                str(eval_path),
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        summary = parse_kv_file(summary_path)
        evaluation = parse_kv_file(eval_path)
        report = parse_kv_file(report_path)
        return {
            "overall": evaluation.get("overall", "unknown"),
            "rows": summary.get("rows", ""),
            "unique_track_ids": summary.get("unique_track_ids", ""),
            "unlock_ready_rows": summary.get("unlock_ready_rows", ""),
            "max_short_gap_merges": summary.get("max_short_gap_merges", ""),
            "unlock_actions": report.get("unlock_actions", ""),
            "adaptive_updates_applied": report.get("adaptive_updates_applied", ""),
        }


def build_cases(profile: str, use_embedding_manifest: bool):
    cases = []
    if profile == "baseline":
        return [{
            "SENTRIFACE_PIPELINE_USE_V2": "1",
            "SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS": "1500",
            "SENTRIFACE_SHORT_GAP_MERGE_MIN_PREVIOUS_HITS": "2",
            "SENTRIFACE_SHORT_GAP_MERGE_CENTER_GATE_RATIO": "1.25",
            "SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MIN": "0.40",
            "SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MAX": "2.50",
            "SENTRIFACE_ENABLE_EMBEDDING_ASSISTED_RELINK": "1" if use_embedding_manifest else "0",
            "SENTRIFACE_EMBEDDING_RELINK_SIMILARITY_MIN": "0.45",
        }]

    if profile == "window":
        for window_ms in (1000, 1500, 2000, 2500):
            for min_previous_hits in (2, 3):
                cases.append({
                    "SENTRIFACE_PIPELINE_USE_V2": "1",
                    "SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS": str(window_ms),
                    "SENTRIFACE_SHORT_GAP_MERGE_MIN_PREVIOUS_HITS": str(min_previous_hits),
                    "SENTRIFACE_SHORT_GAP_MERGE_CENTER_GATE_RATIO": "1.25",
                    "SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MIN": "0.40",
                    "SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MAX": "2.50",
                    "SENTRIFACE_ENABLE_EMBEDDING_ASSISTED_RELINK": "1" if use_embedding_manifest else "0",
                    "SENTRIFACE_EMBEDDING_RELINK_SIMILARITY_MIN": "0.45",
                })
        return cases

    embedding_modes = ("0",) if not use_embedding_manifest else ("0", "1")
    for enable_embedding in embedding_modes:
        similarity_values = ("",) if enable_embedding == "0" else ("0.35", "0.45", "0.55", "0.65")
        for window_ms in (1500, 2000, 2500):
            for min_previous_hits in (2, 3):
                for center_gate_ratio in (1.00, 1.25, 1.50):
                    for similarity_min in similarity_values:
                        cases.append({
                            "SENTRIFACE_PIPELINE_USE_V2": "1",
                            "SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS": str(window_ms),
                            "SENTRIFACE_SHORT_GAP_MERGE_MIN_PREVIOUS_HITS": str(min_previous_hits),
                            "SENTRIFACE_SHORT_GAP_MERGE_CENTER_GATE_RATIO": f"{center_gate_ratio:.2f}",
                            "SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MIN": "0.40",
                            "SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MAX": "2.50",
                            "SENTRIFACE_ENABLE_EMBEDDING_ASSISTED_RELINK": enable_embedding,
                            "SENTRIFACE_EMBEDDING_RELINK_SIMILARITY_MIN": similarity_min or "0.45",
                        })
    return cases


def main():
    args = parse_args()
    frame_manifest = str(Path(args.frame_manifest).resolve())
    detection_manifest = str(Path(args.detection_manifest).resolve())
    embedding_manifest = str(Path(args.embedding_manifest).resolve()) if args.embedding_manifest else ""
    output_csv = Path(args.output_csv)
    output_csv.parent.mkdir(parents=True, exist_ok=True)

    cases = build_cases(args.profile, bool(embedding_manifest))
    fieldnames = [
        "window_ms",
        "min_previous_hits",
        "center_gate_ratio",
        "area_ratio_min",
        "area_ratio_max",
        "embedding_assisted",
        "embedding_similarity_min",
        "overall",
        "rows",
        "unique_track_ids",
        "unlock_ready_rows",
        "max_short_gap_merges",
        "unlock_actions",
        "adaptive_updates_applied",
    ]

    with output_csv.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for case in cases:
            result = run_case(
                frame_manifest,
                detection_manifest,
                embedding_manifest,
                case,
            )
            writer.writerow({
                "window_ms": case["SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS"],
                "min_previous_hits": case["SENTRIFACE_SHORT_GAP_MERGE_MIN_PREVIOUS_HITS"],
                "center_gate_ratio": case["SENTRIFACE_SHORT_GAP_MERGE_CENTER_GATE_RATIO"],
                "area_ratio_min": case["SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MIN"],
                "area_ratio_max": case["SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MAX"],
                "embedding_assisted": case["SENTRIFACE_ENABLE_EMBEDDING_ASSISTED_RELINK"],
                "embedding_similarity_min": case["SENTRIFACE_EMBEDDING_RELINK_SIMILARITY_MIN"],
                "overall": result["overall"],
                "rows": result["rows"],
                "unique_track_ids": result["unique_track_ids"],
                "unlock_ready_rows": result["unlock_ready_rows"],
                "max_short_gap_merges": result["max_short_gap_merges"],
                "unlock_actions": result["unlock_actions"],
                "adaptive_updates_applied": result["adaptive_updates_applied"],
            })

    print(f"output_csv={output_csv}")


if __name__ == "__main__":
    main()
