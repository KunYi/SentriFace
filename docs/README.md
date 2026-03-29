# Docs Index

Project name in docs: `SentriFace`

Top-level entry:
- [README.md](../README.md)
- [AGENTS.md](../AGENTS.md)

## Project
- [Decision Log](project/decision_log.md)
- [Bring-up Notes](project/bringup_notes.md)

## Platform
- [Hardware Baseline](platform/rv1106_system_hardware_baseline.md)
- [Numeric Type Policy](platform/rv1106_numeric_type_policy.md)
- [Geometry Numeric Boundary](platform/geometry_numeric_boundary.md)
- [RKNN Conversion And Generalization Guide](platform/rknn_conversion_and_generalization_guide.md)
- [RKNN Embedding Validation Checklist](platform/rknn_embedding_validation_checklist.md)

## Detector
- [Detector Module Interface](detector/detector_module_interface_spec.md)
- [SCRFD Backend Spec](detector/scrfd_detector_backend_spec.md)
- [SCRFD ONNX Workflow](detector/scrfd_onnx_runner_workflow.md)

## Tracker / Decision
- [Tracker Module Interface](tracker/tracker_module_interface_spec.md)
- [Decision Module Interface](decision/decision_module_interface_spec.md)

## Embedding / Search / Enrollment
- [Embedding Module Interface](embedding/embedding_module_interface_spec.md)
- [Embedding Model Evaluation](embedding/embedding_model_evaluation.md)
- [Embedding Model Validation Plan](embedding/embedding_model_validation_plan.md)
- [Embedding Candidate Asset Checklist](embedding/embedding_candidate_asset_checklist.md)
- [Embedding Benchmark Report Template](embedding/embedding_benchmark_report_template.md)
- [Baseline Embedding Manifest Workflow](embedding/baseline_embedding_manifest_workflow.md)
- [Search Module Interface](search/search_module_interface_spec.md)
- [Dot Kernel Strategy](search/dot_kernel_strategy.md)
- [Dot Kernel Benchmark Guide](search/dot_kernel_benchmark_guide.md)
- [Search Index Package Binary Spec](search/search_index_package_binary_spec.md)
- [Enrollment Module Interface](enrollment/enrollment_module_interface_spec.md)
- [Enrollment Artifact Runner Guide](enrollment/enrollment_artifact_runner_guide.md)
- [Baseline Prototype Package Binary Spec](enrollment/baseline_prototype_package_binary_spec.md)
- [SQLite Persistence Evaluation](enrollment/sqlite_persistence_evaluation.md)
- [Baseline Embedding Import Workflow](enrollment/baseline_embedding_import_workflow.md)
- [Enrollment Baseline Import Runner Guide](enrollment/enrollment_baseline_import_runner_guide.md)
- [Enrollment Baseline Verify Runner Guide](enrollment/enrollment_baseline_verify_runner_guide.md)

Package-first artifacts are `.sfbp` and `.sfsi`; CSV stays for interoperability and debug.

## Pipeline / Validation
- [Offline Sequence Runner Spec](pipeline/offline_sequence_runner_spec.md)
- [RTSP Validation Milestone](pipeline/rtsp_validation_milestone.md)

## Host
- [Host Qt Enroll Tool Spec](host/host_qt_enroll_tool_spec.md)
- [Host Qt Enroll Webcam Guide](host/host_qt_enroll_webcam_guide.md)

## Access / Logging
- [Access Control Backend Spec](access/access_control_backend_spec.md)
- [RGB Liveness Anti-Spoof Spec](access/rgb_liveness_anti_spoof_spec.md)
- [Logging System Spec](logging/logging_system_spec.md)
