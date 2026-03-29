# Host Qt Enrollment Tool Spec

## Purpose

This document defines a host-side enrollment tool under:

```text
host/qt_enroll_app/
```

The goal is not to run on the RV1106 access-control board itself. Instead, it
provides a larger-screen guided enrollment workflow for:

- operator-assisted user registration
- repeatable enrollment capture procedure
- multilingual expansion
- future voice guidance

The initial implementation target is:

- `Qt 5.15.x`
- desktop / host environment

## Validation Order

The host enrollment tool should follow a staged validation path:

1. local desktop `WebCAM`
2. `RTSP` stream integration
3. real `RV1106G2 / RV1106G3` deployment validation

This ordering is important for product development:

- WebCAM is the fastest way to validate preview flow, virtual frame behavior,
  posture guidance, i18n text prompts, and future TTS hooks.
- RTSP should come later, once the host workflow is already stable enough to
  receive remote frames.
- Final timing, usability, and integration conclusions must still be checked on
  the actual RV1106 target device.

The host Qt enrollment tool should therefore be designed as:

- `WebCAM-first`
- `RTSP-ready`
- `RV1106-validated later`

## Why Host-Side Enrollment

For this product, enrollment quality matters more than trying to force every
interaction onto the embedded device.

Host-side enrollment is useful because it can provide:

- larger preview
- operator control
- clearer user guidance
- easier i18n
- optional voice feedback

This is more suitable for product rollout than expecting the user to self-enroll
directly on a constrained board UI.

## Directory Layout

```text
host/
  qt_enroll_app/
    CMakeLists.txt
    include/qt_enroll_app/
    src/
    resources/
    translations/
```

The root project only builds this subproject when:

```text
SENTRIFACE_BUILD_HOST_QT_ENROLL=ON
```

This keeps the board-side mainline independent from optional desktop UI
dependencies.

Recommended host-side build tree:

```text
build/host_qt/
```

This keeps host artifacts under the main `build/` root while still separating
them from the default core/test build.

The host app should also accept source-mode style startup arguments such as:

```text
--input=mock
--input=webcam --webcam-index=0
--input=webcam --webcam-index=0 --width=1280 --height=720
--input=rtsp --rtsp-url=rtsp://...
```

若未顯式提供 `--input=...`，目前 host bring-up 預設應採：

- `--input=webcam`

For product flexibility, preview presentation and guidance direction should also
remain configurable:

```text
--mirror-preview
--no-mirror-preview
--swap-lateral-guidance
--no-swap-lateral-guidance
```

Recommended default for local webcam bring-up:

- mirror preview = on
- swap lateral guidance = on

For bring-up automation, the host app should also accept:

```text
--user-id=E1001
--user-name=Alice
--auto-start
--auto-close-on-review
```

Detector observation should remain a separate boundary and accept its own
startup arguments such as:

```text
--observation=scrfd
--detector-model=third_party/models/buffalo_sc/det_500m.onnx
--detector-interval-ms=250
--detector-score=0.55
```

Even when some backends are still skeletal, the startup contract should already
be fixed so later integration does not require reshaping the UI entry point.

The current stage-1 implementation direction is:

- mock source remains available
- local webcam preview on Linux should use `ffmpeg` as the default backend
- `Qt Multimedia` may remain as an experimental webcam path
- RTSP may keep a placeholder backend until later

See also:

- `docs/host/host_qt_enroll_webcam_guide.md`
- `docs/host/host_qt_enroll_detector_integration_spec.md`

## UI Baseline

The first standard enrollment UI should include:

- basic identity fields such as user id / name
- live preview area
- virtual framing box
- clear directional guidance
- enrollment readiness state
- multi-sample capture plan
- operator-side controls

Typical prompts:

- face centered
- move left
- move right
- move closer
- move back
- face forward
- hold still

## Guidance Strategy

The first host tool should treat guidance as a deterministic state machine built
from:

- face box center
- face size ratio
- yaw / pitch / roll
- quality score

The current skeleton already defines `GuidanceEngine` as the place where this
policy belongs.

For early `WebCAM-first` validation, these thresholds may be temporarily
relaxed when the desktop test environment is known to have:

- weaker lighting
- lower apparent quality score
- imperfect operator position

Such relaxation is allowed only if it is recorded in:

- `docs/project/bringup_notes.md`

and later revisited during:

- `RTSP`
- `RV1106G2 / RV1106G3`

validation.

For enrollment flow control, the host tool should also maintain a dedicated
session state machine instead of treating each frame independently.

See:

- `docs/host/host_qt_enroll_state_machine.md`

## i18n and Voice

The product direction should be international from the beginning.

Therefore the host enrollment tool should:

- keep text prompts translation-friendly
- isolate prompt keys from UI rendering
- allow future optional text-to-speech

Current skeleton direction:

- text prompt selection by `GuidanceKey`
- Qt translation files under `translations/`
- optional `Qt5::TextToSpeech` hook when available

Voice output is not a stage-1 hard requirement, but the architecture should not
block it.

## Recommended Standard Enrollment Flow

1. operator opens enrollment session
2. operator enters `User ID / Name` style basic fields
3. preview appears with virtual frame
4. system gives positional guidance
5. user reaches acceptable centered pose
6. several stable enrollment captures are taken
7. host validates quality and pose diversity
8. resulting embeddings are reviewed and committed to enrollment store

At the current product direction, "stable captures" should be interpreted as:

- per-slot collection window
- several candidate frames observed over a few seconds
- the final saved sample chosen by a feature-confidence ranking

instead of:

- one threshold crossing
- one immediate snapshot

At the current host implementation stage, reaching `review` also exports a
lightweight enrollment artifact under:

```text
artifacts/host_qt_enroll/<user_id>_<timestamp>/
```

The artifact currently includes:

- `enrollment_summary.txt`
- captured sample metadata
- captured full preview frames
- captured face crops derived from the selected bbox

The main product rule is:

- enrollment should be guided and repeatable
- not a single blind snapshot
- and not several near-identical captures with no pose diversity
- saved baseline samples should favor feature reliability, not only detector hit
  existence

## Future Integration

Later stages may connect this host tool to:

- local webcam input
- RTSP stream input
- detector output
- alignment preview
- embedding quality checks
- `EnrollmentStoreV2`
- remote synchronization with deployed devices

Stage 1 only needs:

- the UI/process skeleton
- basic identity field entry integrated into the same screen for test convenience
- guidance policy boundary
- enrollment session state machine
- preview input abstraction
- WebCAM-first validation path

RTSP and RV1106 integration should be treated as later validation milestones,
not first-step blockers.
