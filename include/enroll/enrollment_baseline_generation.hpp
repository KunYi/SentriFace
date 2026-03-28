#ifndef SENTRIFACE_ENROLL_ENROLLMENT_BASELINE_GENERATION_HPP_
#define SENTRIFACE_ENROLL_ENROLLMENT_BASELINE_GENERATION_HPP_

#include <string>
#include <vector>

#include "enroll/enrollment_artifact_import.hpp"
#include "enroll/enrollment_store.hpp"

namespace sentriface::enroll {

enum class BaselineGenerationBackend : std::uint8_t {
  kMockDeterministic = 0,
};

struct BaselinePrototypeRecord {
  int sample_index = 0;
  std::string slot;
  std::string source_image_path;
  std::string source_image_digest;
  std::vector<float> embedding;
  PrototypeMetadata metadata {};
};

struct BaselinePrototypePackage {
  std::string user_id;
  std::string user_name;
  std::vector<BaselinePrototypeRecord> prototypes;
};

struct BaselineEmbeddingCsvImportConfig {
  int embedding_dim = 512;
  std::uint32_t timestamp_ms = 0;
  bool manually_enrolled = true;
};

struct BaselineEmbeddingInputRecord {
  int sample_index = 0;
  std::string slot;
  std::string image_path;
  float quality_score = 0.0f;
  float sample_weight = 1.0f;
  float yaw_deg = 0.0f;
  float pitch_deg = 0.0f;
  float roll_deg = 0.0f;
};

struct BaselineEmbeddingInputManifest {
  std::string user_id;
  std::string user_name;
  std::vector<BaselineEmbeddingInputRecord> records;
};

struct BaselineGenerationConfig {
  BaselineGenerationBackend backend =
      BaselineGenerationBackend::kMockDeterministic;
  int embedding_dim = 512;
  int max_samples = 3;
};

const char* ToString(BaselineGenerationBackend backend);

EnrollmentImportDiagnostic GenerateBaselinePrototypePackage(
    const BaselineEnrollmentPlan& plan,
    const BaselineGenerationConfig& config,
    BaselinePrototypePackage* out_package);

EnrollmentImportDiagnostic BuildBaselineEmbeddingInputManifest(
    const BaselineEnrollmentPlan& plan,
    BaselineEmbeddingInputManifest* out_manifest);

EnrollmentImportDiagnostic LoadBaselinePrototypePackageFromEmbeddingCsv(
    const std::string& csv_path,
    const BaselineEmbeddingCsvImportConfig& config,
    BaselinePrototypePackage* out_package);

EnrollmentImportDiagnostic GenerateMockBaselinePrototypePackage(
    const BaselineEnrollmentPlan& plan,
    const BaselineGenerationConfig& config,
    BaselinePrototypePackage* out_package);

EnrollmentImportDiagnostic ApplyBaselinePrototypePackageToStoreV2(
    const BaselinePrototypePackage& package,
    int person_id,
    EnrollmentStoreV2* store);

}  // namespace sentriface::enroll

#endif  // SENTRIFACE_ENROLL_ENROLLMENT_BASELINE_GENERATION_HPP_
