# Host Qt Enroll Detector Integration Spec

## Purpose

This document defines the next integration boundary for the host enrollment
tool after preview-only webcam bring-up.

Current state:

- the host app can be built
- mock guidance flow exists
- local webcam preview path exists

But local webcam mode should not pretend that detector-driven guidance is
already complete.

## Current Product-Correct Behavior

When launched with:

```text
--input=webcam
```

the app should:

- show real preview frames if available
- keep the preview and session flow alive
- remain in a waiting/searching state until detector observation is connected

It should **not** silently reuse mock "ready to enroll" behavior in webcam mode.

## Next Required Boundary

The next functional layer should produce detector observations such as:

- face present / absent
- bounding box
- pose estimate
- quality estimate

The current implementation direction is:

- preview source remains separate from observation source
- `IObservationSource` is the boundary
- `mock` observation remains available for UI bring-up
- `scrfd` observation can be enabled on the host side when ONNX Runtime and a
  detector model path are available

These observations should then drive:

- `GuidanceEngine`
- `EnrollmentSessionController`
- capture plan progression

## Integration Principle

The host enrollment tool should keep these concerns separate:

1. preview frame source
2. detector observation source
3. guidance/session logic
4. enrollment capture/review

This separation prevents webcam bring-up, detector bring-up, and enrollment
workflow logic from becoming tightly coupled too early.

## Stage Order

1. preview-only webcam works
2. detector observation feeds guidance
3. alignment / quality preview is added
4. embedding capture / commit path is connected

## Current Conclusion

At the current stage, webcam mode should be treated as:

- preview-valid
- workflow-valid
- detector-observation-pending

That is a correct and honest intermediate product state.
