# Enrollment Artifact Runner Guide

`enrollment_artifact_runner` is a bring-up utility for converting a host-side
`enrollment_summary.txt` artifact package into a baseline prototype package
summary.

Current scope:
- load host enrollment artifact
- build baseline sample selection plan
- generate baseline prototypes through a selectable backend
- emit baseline embedding input manifest for later real embedding workflow
- apply identity and baseline prototypes into a temporary `EnrollmentStoreV2`
- emit a human-readable summary

Example:

```bash
./build/enrollment_artifact_runner \
  artifacts/host_qt_enroll/E1007_20260328_174623/enrollment_summary.txt \
  1007 \
  artifacts/host_qt_enroll/E1007_20260328_174623/baseline_package_summary.txt \
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

Output summary includes:
- artifact metadata
- selected baseline samples
- generated prototype digests
- temporary store counts

Runner also emits:
- `*_embedding_input_manifest.txt`

This manifest is the next-step bridge for future real alignment / embedding
generation.

This utility is for bring-up only.
It already exposes a generic baseline generation entry, but the only currently
implemented backend is deterministic mock generation.
