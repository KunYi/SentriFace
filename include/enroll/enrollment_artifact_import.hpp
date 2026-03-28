#ifndef SENTRIFACE_ENROLL_ENROLLMENT_ARTIFACT_IMPORT_HPP_
#define SENTRIFACE_ENROLL_ENROLLMENT_ARTIFACT_IMPORT_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "enroll/enrollment_store.hpp"

namespace sentriface::enroll {

struct EnrollmentArtifactSampleRecord {
  int index = 0;
  std::string slot;
  std::uint32_t elapsed_ms = 0;
  bool has_face = false;
  float bbox_x = 0.0f;
  float bbox_y = 0.0f;
  float bbox_w = 0.0f;
  float bbox_h = 0.0f;
  float yaw_deg = 0.0f;
  float pitch_deg = 0.0f;
  float roll_deg = 0.0f;
  float quality_score = 0.0f;
  float sample_weight = 1.0f;
  int preview_width = 0;
  int preview_height = 0;
  std::string frame_image_path;
  std::string face_crop_path;
};

struct EnrollmentArtifactPackage {
  std::string artifact_dir;
  std::string summary_path;
  std::string user_id;
  std::string user_name;
  std::string source;
  std::string observation;
  std::string capture_plan;
  std::vector<EnrollmentArtifactSampleRecord> samples;
};

struct BaselineEnrollmentSample {
  int sample_index = 0;
  std::string slot;
  std::string preferred_image_path;
  std::string frame_image_path;
  std::string face_crop_path;
  float quality_score = 0.0f;
  float yaw_deg = 0.0f;
  float pitch_deg = 0.0f;
  float roll_deg = 0.0f;
  float sample_weight = 1.0f;
};

struct BaselineEnrollmentPlan {
  std::string user_id;
  std::string user_name;
  std::vector<BaselineEnrollmentSample> samples;
};

struct EnrollmentImportDiagnostic {
  bool ok = false;
  std::string error_message;
};

EnrollmentImportDiagnostic LoadEnrollmentArtifactPackage(
    const std::string& summary_path,
    EnrollmentArtifactPackage* out_package);

EnrollmentImportDiagnostic ApplyArtifactIdentityToStoreV2(
    const EnrollmentArtifactPackage& package,
    int person_id,
    EnrollmentStoreV2* store);

EnrollmentImportDiagnostic BuildBaselineEnrollmentPlan(
    const EnrollmentArtifactPackage& package,
    int max_samples,
    BaselineEnrollmentPlan* out_plan);

}  // namespace sentriface::enroll

#endif  // SENTRIFACE_ENROLL_ENROLLMENT_ARTIFACT_IMPORT_HPP_
