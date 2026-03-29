#ifndef SENTRIFACE_ENROLL_ENROLLMENT_BASELINE_GENERATION_HPP_
#define SENTRIFACE_ENROLL_ENROLLMENT_BASELINE_GENERATION_HPP_

#include <string>
#include <vector>

#include "enroll/enrollment_artifact_import.hpp"
#include "enroll/enrollment_store.hpp"

namespace sentriface::enroll {

enum class BaselineGenerationBackend : std::uint8_t {
  kMockDeterministic = 0,
  kOnnxRuntime = 1,
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
  std::string model_path;
  int embedding_dim = 512;
  int max_samples = 3;
};

constexpr const char* kBaselinePrototypePackageBinaryExtension = ".sfbp";

const char* ToString(BaselineGenerationBackend backend);

std::string MakeBaselinePrototypePackagePath(const std::string& input_path);

std::string MakeFaceSearchV2IndexPath(const std::string& input_path);

int InferBaselinePrototypePackageEmbeddingDim(
    const BaselinePrototypePackage& package);

EnrollmentImportDiagnostic GenerateBaselinePrototypePackage(
    const BaselineEnrollmentPlan& plan,
    const BaselineGenerationConfig& config,
    BaselinePrototypePackage* out_package);

EnrollmentImportDiagnostic GenerateBaselinePrototypePackageFromArtifactSummary(
    const std::string& summary_path,
    const BaselineGenerationConfig& config,
    EnrollmentArtifactPackage* out_artifact,
    BaselineEnrollmentPlan* out_plan,
    BaselinePrototypePackage* out_package);

EnrollmentImportDiagnostic GenerateAndSaveBaselinePackageArtifactsFromArtifactSummary(
    const std::string& summary_path,
    int person_id,
    const BaselineGenerationConfig& config,
    float baseline_weight,
    const std::string& output_summary_path,
    EnrollmentArtifactPackage* out_artifact,
    BaselineEnrollmentPlan* out_plan,
    BaselinePrototypePackage* out_package,
    std::string* out_baseline_package_path,
    std::string* out_search_index_path);

EnrollmentImportDiagnostic BuildBaselineEmbeddingInputManifest(
    const BaselineEnrollmentPlan& plan,
    BaselineEmbeddingInputManifest* out_manifest);

EnrollmentImportDiagnostic LoadBaselinePrototypePackageFromEmbeddingCsv(
    const std::string& csv_path,
    const BaselineEmbeddingCsvImportConfig& config,
    BaselinePrototypePackage* out_package);

EnrollmentImportDiagnostic SaveBaselinePrototypePackageBinary(
    const BaselinePrototypePackage& package,
    const std::string& output_path);

EnrollmentImportDiagnostic SaveBaselinePackageArtifacts(
    const BaselinePrototypePackage& package,
    int person_id,
    int embedding_dim,
    float baseline_weight,
    const std::string& baseline_package_path,
    const std::string& search_index_path);

EnrollmentImportDiagnostic LoadBaselinePrototypePackageBinary(
    const std::string& input_path,
    BaselinePrototypePackage* out_package);

EnrollmentImportDiagnostic LoadBaselinePrototypePackage(
    const std::string& input_path,
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

EnrollmentImportDiagnostic LoadBaselinePrototypePackageIntoStoreV2(
    const std::string& input_path,
    const BaselineEmbeddingCsvImportConfig& config,
    int person_id,
    EnrollmentStoreV2* store);

EnrollmentImportDiagnostic BuildFaceSearchV2IndexPackageFromBaselinePrototypePackage(
    const BaselinePrototypePackage& package,
    int person_id,
    int embedding_dim,
    float baseline_weight,
    sentriface::search::FaceSearchV2IndexPackage* out_package);

EnrollmentImportDiagnostic LoadOrBuildFaceSearchV2IndexPackage(
    const std::string& input_path,
    const BaselinePrototypePackage& package,
    int person_id,
    int embedding_dim,
    float baseline_weight,
    sentriface::search::FaceSearchV2IndexPackage* out_package,
    std::string* out_index_input);

}  // namespace sentriface::enroll

#endif  // SENTRIFACE_ENROLL_ENROLLMENT_BASELINE_GENERATION_HPP_
