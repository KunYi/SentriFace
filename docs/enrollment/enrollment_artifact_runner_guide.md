# Enrollment Artifact Runner Guide

`enrollment_artifact_runner` is a bring-up utility for converting a host-side
`enrollment_summary.txt` artifact package into a baseline prototype package
summary.

Current scope:
- load host enrollment artifact
- build baseline sample selection plan
- generate baseline prototypes through a selectable backend
- emit `.sfbp` baseline prototype package as the primary package boundary
- emit `.sfsi` search-ready index package as the primary runtime boundary
- emit baseline embedding input manifest for later real embedding workflow
- emit a human-readable summary

Runner 內部現在走正式 helper：
- `GenerateBaselinePrototypePackageFromArtifactSummary(...)`
- `GenerateAndSaveBaselinePackageArtifactsFromArtifactSummary(...)`

這把 `artifact -> plan -> baseline package -> .sfbp/.sfsi` 收成可重用的
enrollment 邊界，避免這段流程只存在於 CLI orchestration。

Example:

```bash
./build/enrollment_artifact_runner \
  artifacts/host_qt_enroll/E1007_20260328_174623/enrollment_summary.txt \
  1007 \
  artifacts/host_qt_enroll/E1007_20260328_174623/baseline_artifact_summary.txt \
  3 \
  64 \
  mock
```

Arguments:
- `enrollment_summary.txt`
- `person_id`
- optional output summary path
- optional `max_samples` with default `3`
- optional `embedding_dim` with default `512`
- optional backend with default `mock`

Current backend values:
- `mock`
- `mock_deterministic`
- `onnxruntime`

若使用 `onnxruntime` backend，需額外提供 `model_path`：

```bash
./build/enrollment_artifact_runner \
  artifacts/host_qt_enroll/E1007_20260328_174623/enrollment_summary.txt \
  1007 \
  artifacts/host_qt_enroll/E1007_20260328_174623/baseline_artifact_summary.txt \
  3 \
  512 \
  onnxruntime \
  third_party/models/buffalo_sc/w600k_mbf.onnx
```

這條路徑要求 repo 在 configure 時已啟用：

```bash
-DSENTRIFACE_ENABLE_ONNXRUNTIME=ON
-DSENTRIFACE_ONNXRUNTIME_ROOT=/path/to/onnxruntime
```

Output summary includes:
- artifact metadata
- selected baseline samples
- generated prototype digests
- package output paths
- package-side counts

Runner also emits:
- `*.sfbp`
- `*.sfsi`
- `*_embedding_input_manifest.txt`

This manifest is the next-step bridge for future real alignment / embedding
generation.

This utility is for bring-up only.
It already exposes a generic baseline generation entry, and currently supports:

- deterministic mock generation
- C++ `onnxruntime` embedding generation from artifact preferred images
