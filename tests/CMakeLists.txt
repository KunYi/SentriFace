add_executable(tracker_smoke_test
  tracker_smoke_test.cpp
)

target_link_libraries(tracker_smoke_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(tracker_smoke_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME tracker_smoke_test
  COMMAND tracker_smoke_test
)

add_executable(kalman_filter_test
  kalman_filter_test.cpp
)

target_link_libraries(kalman_filter_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(kalman_filter_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME kalman_filter_test
  COMMAND kalman_filter_test
)

add_executable(face_quality_test
  face_quality_test.cpp
)

target_link_libraries(face_quality_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(face_quality_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME face_quality_test
  COMMAND face_quality_test
)

add_executable(association_test
  association_test.cpp
)

target_link_libraries(association_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(association_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME association_test
  COMMAND association_test
)

add_executable(tracker_lifecycle_test
  tracker_lifecycle_test.cpp
)

target_link_libraries(tracker_lifecycle_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(tracker_lifecycle_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME tracker_lifecycle_test
  COMMAND tracker_lifecycle_test
)

add_executable(tracker_reacquire_test
  tracker_reacquire_test.cpp
)

target_link_libraries(tracker_reacquire_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(tracker_reacquire_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME tracker_reacquire_test
  COMMAND tracker_reacquire_test
)

add_executable(tracker_multiface_test
  tracker_multiface_test.cpp
)

target_link_libraries(tracker_multiface_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(tracker_multiface_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME tracker_multiface_test
  COMMAND tracker_multiface_test
)

add_executable(tracker_query_test
  tracker_query_test.cpp
)

target_link_libraries(tracker_query_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(tracker_query_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME tracker_query_test
  COMMAND tracker_query_test
)

add_executable(tracker_embedding_gate_test
  tracker_embedding_gate_test.cpp
)

target_link_libraries(tracker_embedding_gate_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(tracker_embedding_gate_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME tracker_embedding_gate_test
  COMMAND tracker_embedding_gate_test
)

add_executable(tracker_capacity_test
  tracker_capacity_test.cpp
)

target_link_libraries(tracker_capacity_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(tracker_capacity_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME tracker_capacity_test
  COMMAND tracker_capacity_test
)

add_executable(tracker_embedding_interval_test
  tracker_embedding_interval_test.cpp
)

target_link_libraries(tracker_embedding_interval_test
  PRIVATE
    sentriface_tracker
)

target_include_directories(tracker_embedding_interval_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME tracker_embedding_interval_test
  COMMAND tracker_embedding_interval_test
)

add_executable(face_aligner_test
  face_aligner_test.cpp
)

target_link_libraries(face_aligner_test
  PRIVATE
    sentriface_align
)

target_include_directories(face_aligner_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME face_aligner_test
  COMMAND face_aligner_test
)

add_executable(face_embedder_test
  face_embedder_test.cpp
)

target_link_libraries(face_embedder_test
  PRIVATE
    sentriface_embedding
)

target_include_directories(face_embedder_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME face_embedder_test
  COMMAND face_embedder_test
)

add_executable(image_io_test
  image_io_test.cpp
)

target_link_libraries(image_io_test
  PRIVATE
    sentriface_camera
)

target_include_directories(image_io_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME image_io_test
  COMMAND image_io_test
)

add_executable(face_search_test
  face_search_test.cpp
)

target_link_libraries(face_search_test
  PRIVATE
    sentriface_search
)

target_include_directories(face_search_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME face_search_test
  COMMAND face_search_test
)

add_executable(dot_kernel_test
  dot_kernel_test.cpp
)

target_link_libraries(dot_kernel_test
  PRIVATE
    sentriface_search
)

target_include_directories(dot_kernel_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME dot_kernel_test
  COMMAND dot_kernel_test
)

add_executable(face_search_v2_test
  face_search_v2_test.cpp
)

target_link_libraries(face_search_v2_test
  PRIVATE
    sentriface_search
)

target_include_directories(face_search_v2_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME face_search_v2_test
  COMMAND face_search_v2_test
)

add_executable(face_search_v2_person_aggregation_test
  face_search_v2_person_aggregation_test.cpp
)

target_link_libraries(face_search_v2_person_aggregation_test
  PRIVATE
    sentriface_search
)

target_include_directories(face_search_v2_person_aggregation_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME face_search_v2_person_aggregation_test
  COMMAND face_search_v2_person_aggregation_test
)

add_executable(face_search_v2_debug_test
  face_search_v2_debug_test.cpp
)

target_link_libraries(face_search_v2_debug_test
  PRIVATE
    sentriface_search
)

target_include_directories(face_search_v2_debug_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME face_search_v2_debug_test
  COMMAND face_search_v2_debug_test
)

add_executable(face_decision_test
  face_decision_test.cpp
)

target_link_libraries(face_decision_test
  PRIVATE
    sentriface_decision
)

target_include_directories(face_decision_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME face_decision_test
  COMMAND face_decision_test
)

add_executable(enrollment_store_test
  enrollment_store_test.cpp
)

target_link_libraries(enrollment_store_test
  PRIVATE
    sentriface_enroll
)

target_include_directories(enrollment_store_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME enrollment_store_test
  COMMAND enrollment_store_test
)

add_executable(enrollment_store_v2_test
  enrollment_store_v2_test.cpp
)

target_link_libraries(enrollment_store_v2_test
  PRIVATE
    sentriface_enroll
)

target_include_directories(enrollment_store_v2_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME enrollment_store_v2_test
  COMMAND enrollment_store_v2_test
)

add_executable(enrollment_artifact_import_test
  enrollment_artifact_import_test.cpp
)

target_link_libraries(enrollment_artifact_import_test
  PRIVATE
    sentriface_enroll
)

target_include_directories(enrollment_artifact_import_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME enrollment_artifact_import_test
  COMMAND enrollment_artifact_import_test
)

add_executable(enrollment_baseline_generation_test
  enrollment_baseline_generation_test.cpp
)

target_link_libraries(enrollment_baseline_generation_test
  PRIVATE
    sentriface_enroll
    sentriface_search
)

target_include_directories(enrollment_baseline_generation_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME enrollment_baseline_generation_test
  COMMAND enrollment_baseline_generation_test
)

add_executable(enrollment_artifact_runner_test
  enrollment_artifact_runner_test.cpp
)

target_link_libraries(enrollment_artifact_runner_test
  PRIVATE
    sentriface_enroll
)

target_include_directories(enrollment_artifact_runner_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(enrollment_artifact_runner_test
  PRIVATE
    SENTRIFACE_ENROLLMENT_ARTIFACT_RUNNER_PATH="$<TARGET_FILE:enrollment_artifact_runner>"
)

add_test(
  NAME enrollment_artifact_runner_test
  COMMAND enrollment_artifact_runner_test
)

set_tests_properties(enrollment_artifact_runner_test
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(enrollment_baseline_import_runner_test
  enrollment_baseline_import_runner_test.cpp
)

target_link_libraries(enrollment_baseline_import_runner_test
  PRIVATE
    sentriface_enroll
)

target_include_directories(enrollment_baseline_import_runner_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(enrollment_baseline_import_runner_test
  PRIVATE
    SENTRIFACE_ENROLLMENT_BASELINE_IMPORT_RUNNER_PATH="$<TARGET_FILE:enrollment_baseline_import_runner>"
)

add_test(
  NAME enrollment_baseline_import_runner_test
  COMMAND enrollment_baseline_import_runner_test
)

set_tests_properties(enrollment_baseline_import_runner_test
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(enrollment_baseline_verify_runner_test
  enrollment_baseline_verify_runner_test.cpp
)

target_link_libraries(enrollment_baseline_verify_runner_test
  PRIVATE
    sentriface_enroll
    sentriface_search
)

target_include_directories(enrollment_baseline_verify_runner_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(enrollment_baseline_verify_runner_test
  PRIVATE
    SENTRIFACE_ENROLLMENT_BASELINE_VERIFY_RUNNER_PATH="$<TARGET_FILE:enrollment_baseline_verify_runner>"
)

add_test(
  NAME enrollment_baseline_verify_runner_test
  COMMAND enrollment_baseline_verify_runner_test
)

set_tests_properties(enrollment_baseline_verify_runner_test
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(enrollment_search_v2_integration_test
  enrollment_search_v2_integration_test.cpp
)

target_link_libraries(enrollment_search_v2_integration_test
  PRIVATE
    sentriface_enroll
    sentriface_search
)

target_include_directories(enrollment_search_v2_integration_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME enrollment_search_v2_integration_test
  COMMAND enrollment_search_v2_integration_test
)

add_executable(search_v2_pipeline_compat_test
  search_v2_pipeline_compat_test.cpp
)

target_link_libraries(search_v2_pipeline_compat_test
  PRIVATE
    sentriface_app
    sentriface_enroll
    sentriface_search
)

target_include_directories(search_v2_pipeline_compat_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME search_v2_pipeline_compat_test
  COMMAND search_v2_pipeline_compat_test
)

add_executable(face_pipeline_test
  face_pipeline_test.cpp
)

target_link_libraries(face_pipeline_test
  PRIVATE
    sentriface_app
)

target_include_directories(face_pipeline_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME face_pipeline_test
  COMMAND face_pipeline_test
)

add_executable(frame_source_test
  frame_source_test.cpp
)

target_link_libraries(frame_source_test
  PRIVATE
    sentriface_camera
)

target_include_directories(frame_source_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME frame_source_test
  COMMAND frame_source_test
)

add_executable(face_detector_test
  face_detector_test.cpp
)

target_link_libraries(face_detector_test
  PRIVATE
    sentriface_detector
    sentriface_camera
)

target_include_directories(face_detector_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME face_detector_test
  COMMAND face_detector_test
)

add_executable(manifest_face_detector_test
  manifest_face_detector_test.cpp
)

target_link_libraries(manifest_face_detector_test
  PRIVATE
    sentriface_detector
    sentriface_camera
)

target_include_directories(manifest_face_detector_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME manifest_face_detector_test
  COMMAND manifest_face_detector_test
)

add_executable(scrfd_face_detector_test
  scrfd_face_detector_test.cpp
)

target_link_libraries(scrfd_face_detector_test
  PRIVATE
    sentriface_detector
    sentriface_camera
)

target_include_directories(scrfd_face_detector_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(scrfd_face_detector_test
  PRIVATE
    SENTRIFACE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
)

add_test(
  NAME scrfd_face_detector_test
  COMMAND scrfd_face_detector_test
)

add_executable(sample_pipeline_runner_test
  sample_pipeline_runner_test.cpp
)

add_dependencies(sample_pipeline_runner_test sample_pipeline_runner)

target_compile_definitions(sample_pipeline_runner_test
  PRIVATE
    SENTRIFACE_BINARY_DIR="${CMAKE_BINARY_DIR}"
)

add_test(
  NAME sample_pipeline_runner_test
  COMMAND sample_pipeline_runner_test
)

set_tests_properties(sample_pipeline_runner_test PROPERTIES
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(sample_pipeline_runner_v2_test
  sample_pipeline_runner_v2_test.cpp
)

add_dependencies(sample_pipeline_runner_v2_test sample_pipeline_runner)

target_link_libraries(sample_pipeline_runner_v2_test
  PRIVATE
    sentriface_search
    sentriface_enroll
)

target_include_directories(sample_pipeline_runner_v2_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(sample_pipeline_runner_v2_test
  PRIVATE
    SENTRIFACE_BINARY_DIR="${CMAKE_BINARY_DIR}"
)

add_test(
  NAME sample_pipeline_runner_v2_test
  COMMAND sample_pipeline_runner_v2_test
)

set_tests_properties(sample_pipeline_runner_v2_test PROPERTIES
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(offline_sequence_source_test
  offline_sequence_source_test.cpp
)

target_link_libraries(offline_sequence_source_test
  PRIVATE
    sentriface_camera
)

target_include_directories(offline_sequence_source_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME offline_sequence_source_test
  COMMAND offline_sequence_source_test
)

add_executable(offline_sequence_runner_test
  offline_sequence_runner_test.cpp
)

add_dependencies(offline_sequence_runner_test offline_sequence_runner)

target_compile_definitions(offline_sequence_runner_test
  PRIVATE
    SENTRIFACE_BINARY_DIR="${CMAKE_BINARY_DIR}"
)

add_test(
  NAME offline_sequence_runner_test
  COMMAND offline_sequence_runner_test
)

set_tests_properties(offline_sequence_runner_test PROPERTIES
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(offline_sequence_runner_v2_test
  offline_sequence_runner_v2_test.cpp
)

add_dependencies(offline_sequence_runner_v2_test offline_sequence_runner)

target_link_libraries(offline_sequence_runner_v2_test
  PRIVATE
    sentriface_search
    sentriface_enroll
)

target_include_directories(offline_sequence_runner_v2_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(offline_sequence_runner_v2_test
  PRIVATE
    SENTRIFACE_BINARY_DIR="${CMAKE_BINARY_DIR}"
)

add_test(
  NAME offline_sequence_runner_v2_test
  COMMAND offline_sequence_runner_v2_test
)

set_tests_properties(offline_sequence_runner_v2_test PROPERTIES
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(pipeline_report_test
  pipeline_report_test.cpp
)

target_link_libraries(pipeline_report_test
  PRIVATE
    sentriface_app
)

target_include_directories(pipeline_report_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME pipeline_report_test
  COMMAND pipeline_report_test
)

add_executable(unlock_controller_test
  unlock_controller_test.cpp
)

target_link_libraries(unlock_controller_test
  PRIVATE
    sentriface_access
)

target_include_directories(unlock_controller_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME unlock_controller_test
  COMMAND unlock_controller_test
)

add_executable(system_logger_test
  system_logger_test.cpp
)

target_link_libraries(system_logger_test
  PRIVATE
    sentriface_logging
)

target_include_directories(system_logger_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(
  NAME system_logger_test
  COMMAND system_logger_test
)
