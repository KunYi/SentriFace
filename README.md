# SentriFace

`SentriFace` is a local-first face access control prototype targeting `Luckfox Pico RV1106G2 / RV1106G3`.

Current product direction:
- fixed-position, near-range, single-entry access control
- `SCRFD-500M` for detection
- `IoU + Kalman Filter` for tracking
- event-driven embedding and weighted local search
- local-first target scale: `200` validated, `200 ~ 500` expected

This repository is still in milestone bring-up.

Important status note:
- `RV1106G2 / RV1106G3` is the intended target platform
- current validation is still primarily `PC + ONNX + local WebCAM`
- `RTSP`, `RKNN`, and real `RV1106` deployment are not yet fully validated
- something that works on the PC today must not be treated as automatically deployable on `RV1106`

## Current Status

Working end-to-end paths already exist for:
- webcam-first host enrollment via `Qt 5.15.x`
- enrollment artifact export with accepted frame + face crop
- baseline embedding generation using `w600k_mbf.onnx`
- baseline prototype package (`.sfbp`) and search index package (`.sfsi`) direct-load
- baseline self-verify using `FaceSearchV2` over `.sfsi`
- offline replay for detector / tracker / decision validation

Recent bring-up focus:
- host enrollment UI simplification
- webcam preview stabilization
- real artifact validation instead of mock-only flow
- centralized tuning constants for host enrollment guidance
- keeping target-platform risk explicit while validation is still PC-first

## Repository Layout

- `docs/` specifications, guides, decision log, bring-up notes
- `include/` public interfaces
- `src/` core implementation
- `tests/` unit and integration tests
- `tools/` validation and workflow helpers
- `host/` host-side tooling such as the Qt enrollment app

## Bootstrap

The repository does not track:
- `.venv/`
- `third_party/models/`
- `third_party/onnxruntime/`

Create a local development environment with:

```bash
tools/bootstrap.sh
```

Optional downloads:

```bash
tools/bootstrap.sh --download-onnxruntime --download-buffalo-sc
```

This will:
- create `.venv`
- install Python packages from `requirements.txt`
- create the expected `third_party/` directory layout
- optionally download ONNX Runtime C/C++ and the `buffalo_sc` asset zip

## Read Order

Start here:
1. [AGENTS.md](AGENTS.md)
2. [docs/README.md](docs/README.md)
3. [docs/project/decision_log.md](docs/project/decision_log.md)
4. load only the module docs needed for the task at hand

## Host Enrollment Quick Start

Build:

```bash
tools/build_host_qt_enroll.sh
```

Run with webcam:

```bash
tools/run_host_qt_enroll_webcam.sh
```

Run debug build with logs:

```bash
tools/run_host_qt_enroll_debug.sh
```

## Baseline Embedding Workflow

Given a real host enrollment artifact:

```bash
tools/run_enrollment_baseline_verify.sh \
  artifacts/host_qt_enroll/<USER>_<TIMESTAMP>/enrollment_summary.txt \
  <PERSON_ID>
```

This produces:
- `baseline_embedding_output.sfbp`
- `baseline_embedding_output.sfsi`
- `baseline_import_summary.txt`
- `baseline_verify_summary.txt`
- `baseline_embedding_output.csv` for interoperability / debug

正式 direct-load handoff 以 `.sfbp / .sfsi` 為主，`CSV` 保留為互通與 bring-up 除錯格式。

若 build 已啟用 C++ `onnxruntime`，也可直接走：

```bash
tools/run_enrollment_baseline_embedding_cpp.sh \
  artifacts/host_qt_enroll/<USER>_<TIMESTAMP>/enrollment_summary.txt \
  <PERSON_ID>
```

## Validation Order

Validation priority is fixed:
1. local webcam
2. RTSP
3. RV1106 target hardware

Do not mix UI bring-up, transport problems, and target deployment problems in the same debugging loop unless necessary.

At the current milestone, most successful runs are still in step 1.
Steps 2 and 3 remain required before claiming target readiness.

## License

This project is released under the MIT License.

See:
- [LICENSE](LICENSE)
