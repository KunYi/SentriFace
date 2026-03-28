# Host Qt Enroll WebCAM Guide

## Purpose

This guide defines the first practical bring-up path for the host enrollment
tool using a local webcam.

It follows the agreed validation order:

1. local webcam
2. RTSP later
3. RV1106 device validation after that

## Build

```bash
tools/build_host_qt_enroll.sh
```

Default output:

```text
build/host_qt/host/qt_enroll_app/sentriface_enroll_app
```

## Run With WebCAM

```bash
tools/run_host_qt_enroll_webcam.sh
```

If ONNX Runtime and `third_party/models/buffalo_sc/det_500m.onnx` are present,
the helper scripts now also enable host-side `SCRFD` observation automatically.
That changes webcam mode from:

- preview-only waiting state

to:

- live preview
- live detector observation
- overlay-driven enrollment guidance

Or select a specific webcam index and nominal preview size:

```bash
tools/run_host_qt_enroll_webcam.sh build/host_qt 0 1280 720
```

Mirror 與左右提示也可分開控制：

```bash
build/host_qt/host/qt_enroll_app/sentriface_enroll_app \
  --input=webcam \
  --webcam-index=0 \
  --mirror-preview \
  --swap-lateral-guidance
```

如果現場安裝不需要鏡像或不希望左右提示對調：

```bash
build/host_qt/host/qt_enroll_app/sentriface_enroll_app \
  --input=webcam \
  --webcam-index=0 \
  --no-mirror-preview \
  --no-swap-lateral-guidance
```

## Direct Launch

```bash
build/host_qt/host/qt_enroll_app/sentriface_enroll_app \
  --input=webcam \
  --webcam-index=0 \
  --width=1280 \
  --height=720 \
  --observation=scrfd \
  --detector-model=third_party/models/buffalo_sc/det_500m.onnx
```

## Current Stage-1 Scope

The current local webcam path is intended to validate:

- basic identity entry
- application startup
- preview source selection
- webcam frame preview
- preview freshness / live status
- virtual frame overlay
- guidance/session UI flow
- detector-observation-driven enrollment when host-side `SCRFD` is available

The current UI should visibly expose:

- preview live/stale state
- received frame count
- last frame age
- observation live/stale state
- observation result count
- whether the latest observation currently sees a face

It does not yet mean that:

- enrollment images are truly committed

For quick offscreen/mock validation, the tool also supports:

```bash
build/host_qt/host/qt_enroll_app/sentriface_enroll_app \
  --input=mock \
  --observation=none \
  --user-id=E1001 \
  --user-name=Alice \
  --auto-start \
  --auto-close-on-review
```

When the guided capture reaches `review`, the tool now exports:

```text
artifacts/host_qt_enroll/<user_id>_<timestamp>/enrollment_summary.txt
```

This is useful for verifying:

- identity fields passed through the flow
- capture slots were completed
- the enrollment state machine actually reached review/export
- selected frames and face crops were really saved

For current bring-up convenience, the host tool keeps:

- identity entry
- guided capture flow

in the same screen, even though a production deployment may later separate these
steps across different operator workflows.

Those are the next integration stages.

## Troubleshooting

If the preview source shows as unsupported:

- verify Qt Multimedia is available in the host Qt installation
- rebuild the host app with the same Qt environment

If the app starts but the webcam frame does not appear:

- try another webcam index
- verify the host system camera is not occupied by another process
- confirm the current Qt Multimedia backend supports `QCamera + QVideoProbe`

## Product Direction

This guide is intentionally WebCAM-first.

Do not treat RTSP or RV1106 board validation as the first bring-up step for the
host enrollment tool. The goal is to stabilize:

- UI flow
- preview behavior
- operator guidance

before adding remote transport or embedded-device factors.
