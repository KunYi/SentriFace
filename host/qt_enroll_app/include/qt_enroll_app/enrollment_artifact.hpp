#ifndef SENTRIFACE_HOST_QT_ENROLL_APP_ENROLLMENT_ARTIFACT_HPP_
#define SENTRIFACE_HOST_QT_ENROLL_APP_ENROLLMENT_ARTIFACT_HPP_

#include <QImage>
#include <QString>

#include <vector>

#include "qt_enroll_app/capture_plan.hpp"
#include "qt_enroll_app/guidance_engine.hpp"

namespace sentriface::host {

struct CapturedEnrollmentSample {
  int sample_index = 0;
  int elapsed_ms = 0;
  int candidate_count = 0;
  CaptureSlotKind slot_kind = CaptureSlotKind::kFrontal;
  FacePoseSample face_sample {};
  float sample_weight = 1.0f;
  QImage preview_image;
};

struct EnrollmentArtifactRequest {
  QString user_id;
  QString user_name;
  QString source_label;
  QString observation_label;
  QString capture_plan_summary;
  std::vector<CapturedEnrollmentSample> captured_samples;
};

struct EnrollmentArtifactResult {
  bool ok = false;
  QString output_dir;
  QString summary_path;
  QString error_message;
};

EnrollmentArtifactResult ExportEnrollmentArtifact(
    const EnrollmentArtifactRequest& request);

}  // namespace sentriface::host

#endif  // SENTRIFACE_HOST_QT_ENROLL_APP_ENROLLMENT_ARTIFACT_HPP_
