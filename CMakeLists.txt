cmake_minimum_required(VERSION 3.16)

project(sentriface
  VERSION 0.1.0
  LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SENTRIFACE_BUILD_TESTS "Build SentriFace tests" ON)
option(SENTRIFACE_ENABLE_ONNXRUNTIME "Enable optional ONNX Runtime C++ integration" OFF)
option(SENTRIFACE_ENABLE_GPIOD "Enable optional libgpiod GPIO integration" OFF)
option(SENTRIFACE_BUILD_HOST_QT_ENROLL "Build optional Qt 5.15.x host enrollment tool" OFF)
set(SENTRIFACE_ONNXRUNTIME_ROOT "" CACHE PATH "Root path of ONNX Runtime C/C++ package")

add_library(sentriface_tracker STATIC
  src/tracker/association.cpp
  src/tracker/face_quality.cpp
  src/tracker/kalman_filter.cpp
  src/tracker/tracker.cpp
)

add_library(sentriface_align STATIC
  src/align/face_aligner.cpp
)

add_library(sentriface_embedding STATIC
  src/embedding/face_embedder.cpp
)

add_library(sentriface_search STATIC
  src/search/face_search.cpp
)

add_library(sentriface_decision STATIC
  src/decision/face_decision.cpp
)

add_library(sentriface_enroll STATIC
  src/enroll/enrollment_artifact_import.cpp
  src/enroll/enrollment_baseline_generation.cpp
  src/enroll/enrollment_store.cpp
)

add_library(sentriface_access STATIC
  src/access/unlock_controller.cpp
)

add_library(sentriface_logging STATIC
  src/logging/system_logger.cpp
)

add_library(sentriface_app STATIC
  src/app/face_pipeline.cpp
  src/app/pipeline_report.cpp
)

add_library(sentriface_camera STATIC
  src/camera/frame_source.cpp
  src/camera/offline_sequence_source.cpp
)

add_library(sentriface_detector STATIC
  src/detector/face_detector.cpp
  src/detector/scrfd_runtime.cpp
)

if(SENTRIFACE_ENABLE_ONNXRUNTIME)
  if(NOT SENTRIFACE_ONNXRUNTIME_ROOT)
    message(FATAL_ERROR
      "SENTRIFACE_ENABLE_ONNXRUNTIME=ON requires SENTRIFACE_ONNXRUNTIME_ROOT "
      "to point to an ONNX Runtime C/C++ package root")
  endif()

  set(SENTRIFACE_ONNXRUNTIME_INCLUDE_DIR
    "${SENTRIFACE_ONNXRUNTIME_ROOT}/include")

  find_library(SENTRIFACE_ONNXRUNTIME_LIB
    NAMES onnxruntime
    PATHS
      "${SENTRIFACE_ONNXRUNTIME_ROOT}/lib"
      "${SENTRIFACE_ONNXRUNTIME_ROOT}/lib64"
    NO_DEFAULT_PATH)

  if(NOT EXISTS "${SENTRIFACE_ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_cxx_api.h")
    message(FATAL_ERROR
      "ONNX Runtime headers not found under ${SENTRIFACE_ONNXRUNTIME_INCLUDE_DIR}")
  endif()

  if(NOT SENTRIFACE_ONNXRUNTIME_LIB)
    message(FATAL_ERROR
      "ONNX Runtime library not found under ${SENTRIFACE_ONNXRUNTIME_ROOT}/lib or lib64")
  endif()

  target_include_directories(sentriface_detector
    PUBLIC
      "${SENTRIFACE_ONNXRUNTIME_INCLUDE_DIR}"
  )

  target_link_libraries(sentriface_detector
    PUBLIC
      "${SENTRIFACE_ONNXRUNTIME_LIB}"
  )

  target_compile_definitions(sentriface_detector
    PUBLIC
      SENTRIFACE_HAS_ONNXRUNTIME=1
  )
endif()

add_executable(sample_pipeline_runner
  src/app/sample_pipeline_runner.cpp
)

add_executable(offline_sequence_runner
  src/app/offline_sequence_runner.cpp
)

add_executable(scrfd_ppm_runner
  src/app/scrfd_ppm_runner.cpp
)

add_executable(enrollment_artifact_runner
  src/app/enrollment_artifact_runner.cpp
)

add_executable(enrollment_baseline_import_runner
  src/app/enrollment_baseline_import_runner.cpp
)

add_executable(enrollment_baseline_verify_runner
  src/app/enrollment_baseline_verify_runner.cpp
)

target_include_directories(sentriface_tracker
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sentriface_align
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sentriface_embedding
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sentriface_search
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sentriface_decision
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sentriface_enroll
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sentriface_app
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sentriface_access
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sentriface_logging
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sentriface_camera
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sentriface_detector
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(sentriface_tracker
  PUBLIC
    cxx_std_17
)

target_compile_features(sentriface_align
  PUBLIC
    cxx_std_17
)

target_compile_features(sentriface_embedding
  PUBLIC
    cxx_std_17
)

target_compile_features(sentriface_search
  PUBLIC
    cxx_std_17
)

target_compile_features(sentriface_decision
  PUBLIC
    cxx_std_17
)

target_compile_features(sentriface_enroll
  PUBLIC
    cxx_std_17
)

target_compile_features(sentriface_app
  PUBLIC
    cxx_std_17
)

target_compile_features(sentriface_access
  PUBLIC
    cxx_std_17
)

target_compile_features(sentriface_logging
  PUBLIC
    cxx_std_17
)

target_compile_features(sentriface_camera
  PUBLIC
    cxx_std_17
)

target_compile_features(sentriface_detector
  PUBLIC
    cxx_std_17
)

target_compile_options(sentriface_tracker
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(sentriface_align
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(sentriface_embedding
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(sentriface_search
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(sentriface_decision
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(sentriface_enroll
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(sentriface_app
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(sentriface_access
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(sentriface_access
  PUBLIC
    sentriface_logging
)

target_compile_options(sentriface_logging
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(sentriface_camera
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(sentriface_detector
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(sample_pipeline_runner
  PRIVATE
    sentriface_app
    sentriface_camera
    sentriface_detector
    sentriface_access
    sentriface_tracker
)

target_link_libraries(offline_sequence_runner
  PRIVATE
    sentriface_app
    sentriface_camera
    sentriface_detector
    sentriface_access
    sentriface_logging
    sentriface_tracker
)

target_link_libraries(scrfd_ppm_runner
  PRIVATE
    sentriface_detector
    sentriface_camera
)

target_link_libraries(enrollment_artifact_runner
  PRIVATE
    sentriface_enroll
)

target_link_libraries(enrollment_baseline_import_runner
  PRIVATE
    sentriface_enroll
)

target_link_libraries(enrollment_baseline_verify_runner
  PRIVATE
    sentriface_enroll
    sentriface_search
)

target_include_directories(sample_pipeline_runner
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(offline_sequence_runner
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(scrfd_ppm_runner
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(enrollment_artifact_runner
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(enrollment_baseline_import_runner
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(enrollment_baseline_verify_runner
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sentriface_app
  PUBLIC
    sentriface_search
    sentriface_decision
    sentriface_enroll
    sentriface_logging
)

if(SENTRIFACE_ENABLE_GPIOD)
  target_compile_definitions(sentriface_access
    PUBLIC
      SENTRIFACE_HAS_GPIOD=1
  )
endif()

if(SENTRIFACE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(SENTRIFACE_BUILD_HOST_QT_ENROLL)
  add_subdirectory(host/qt_enroll_app)
endif()
