# Host Qt Enrollment State Machine

## Purpose

This document defines the first-stage host enrollment state machine for:

```text
host/qt_enroll_app/
```

The goal is to prevent the host UI from becoming a loose collection of preview
widgets and prompts. Enrollment should behave like a clear product workflow.

## State Model

The current stage-1 state machine is:

- `searching_face`
- `guiding_pose`
- `ready_countdown`
- `capturing`
- `review`

These states are implemented by:

```text
host/qt_enroll_app/include/qt_enroll_app/enrollment_session.hpp
host/qt_enroll_app/src/enrollment_session.cpp
```

## State Intent

### `searching_face`

No valid face is currently available.

Use this state to show:

- align yourself with the preview
- place your face in the virtual frame

### `guiding_pose`

A face exists, but framing or pose is not yet acceptable.

Typical prompts:

- move left
- move right
- move closer
- move back
- face forward
- hold still

### `ready_countdown`

The user is acceptable and stable enough to prepare capture.

This is intended to reduce:

- accidental capture
- unstable head motion
- low-quality one-off enrollment frames

### `capturing`

A guided enrollment frame should be captured at this moment.

The product should later connect this state to:

- real image capture
- alignment
- embedding extraction

### `review`

Enough guided samples have been collected and the operator can review the
session before committing enrollment.

## Why This Matters

For this product, enrollment should be:

- guided
- repeatable
- operator-friendly
- multilingual

It should not behave like a single blind snapshot tool.

## Capture Diversity Plan

A practical enrollment session should not stop at "three random frames".

The current host direction should explicitly aim for a small, repeatable
capture diversity plan such as:

- frontal
- slight left
- slight right

This gives a better baseline for later search and re-verification than storing
multiple near-identical frontal frames.

The current host skeleton now includes a dedicated capture-plan layer for this
purpose.

## WebCAM-First Integration

This state machine is specifically intended to support the agreed validation
order:

1. local `WebCAM`
2. `RTSP`
3. actual `RV1106` deployment

The state machine should therefore remain independent from any single transport
or device backend.

## Preview Input Abstraction

The host app now also has a first-stage preview source abstraction:

```text
host/qt_enroll_app/include/qt_enroll_app/frame_source.hpp
host/qt_enroll_app/src/frame_source.cpp
```

Current modes:

- `mock`
- `local_webcam`
- `rtsp`

Stage 1 only requires `mock` and architecture boundaries. Real `WebCAM` capture
should be the next integration target.

Current practical target:

- local webcam preview first
- detector/alignment overlay later
- RTSP preview after the local webcam path is stable

The startup contract should already support:

- `--input=mock`
- `--input=webcam --webcam-index=0`
- `--input=rtsp --rtsp-url=...`

This lets the product workflow settle early, even if the actual backend
implementation arrives incrementally.
