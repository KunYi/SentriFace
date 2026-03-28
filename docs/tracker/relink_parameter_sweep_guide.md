# Relink Parameter Sweep Guide

## Purpose

`tools/sweep_relink_params.py` is the replay-side companion to the earlier
tracker sweep tools. It is intended for controlled access-control tuning where
we care about:

- final `unlock_ready` behavior
- short-gap merge activity
- whether embedding-assisted relink improves session continuity

This tool does **not** claim to prove the globally best relink settings. Its
role is to quickly compare a small, bounded set of replay configurations on the
same artifact set.

## Inputs

Required:

- frame manifest
- detection manifest
- output csv

Optional:

- embedding manifest

Typical usage:

```bash
python3 tools/sweep_relink_params.py \
  artifacts/webcam_bright/offline_manifest.txt \
  artifacts/webcam_bright/detection_manifest.txt \
  artifacts/webcam_bright/relink_sweep.csv \
  --embedding-manifest artifacts/webcam_bright/embedding_manifest_real.txt \
  --profile embedding
```

## Profiles

### `baseline`

Runs a single reference configuration.

Useful when:

- checking the tool path itself
- producing a reproducible baseline row

### `window`

Sweeps:

- `SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS`
- `SENTRIFACE_SHORT_GAP_MERGE_MIN_PREVIOUS_HITS`

Useful when:

- validating how sensitive replay is to merge window length
- checking whether stricter previous-track stability changes session behavior

### `embedding`

Sweeps:

- merge window
- minimum previous hits
- center gate ratio
- embedding-assisted on/off
- embedding similarity threshold

This is the recommended current mainline profile when replay includes real
embedding events.

## Output Columns

- `window_ms`
- `min_previous_hits`
- `center_gate_ratio`
- `area_ratio_min`
- `area_ratio_max`
- `embedding_assisted`
- `embedding_similarity_min`
- `overall`
- `rows`
- `unique_track_ids`
- `unlock_ready_rows`
- `max_short_gap_merges`
- `unlock_actions`
- `adaptive_updates_applied`

## Interpretation

For the current single-person access-control product direction, the more useful
signals are:

- `unlock_ready_rows`
- `max_short_gap_merges`
- `unlock_actions`

`unique_track_ids` still matters, but it should not be treated as the only
success metric. For access control, a configuration can still be acceptable if:

- short-lived fragmentation exists
- but the final decision session remains stable
- and unlock behavior stays correct

## Current Guidance

Use this tool to compare:

1. no embedding-assisted relink
2. embedding-assisted relink with real embedding replay
3. stricter vs looser merge gating

Do not freeze replay results as final product parameters without later board-side
verification on the actual RV1106 target.
