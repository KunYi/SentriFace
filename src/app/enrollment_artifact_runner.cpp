#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "enroll/enrollment_artifact_import.hpp"
#include "enroll/enrollment_baseline_generation.hpp"

namespace {

void PrintUsage() {
  std::cerr
      << "usage: enrollment_artifact_runner <enrollment_summary.txt> <person_id> "
      << "[output_summary.txt] [max_samples] [embedding_dim] [backend] [model_path]\n";
}

std::string DefaultOutputPath(const std::string& summary_path) {
  const std::size_t pos = summary_path.find_last_of("/\\");
  const std::string dir =
      pos == std::string::npos ? "." : summary_path.substr(0, pos);
  return dir + "/baseline_artifact_summary.txt";
}

bool ParseIntArg(const char* text, int* out_value) {
  if (text == nullptr || out_value == nullptr) {
    return false;
  }
  char* end = nullptr;
  const long value = std::strtol(text, &end, 10);
  if (end == text || *end != '\0') {
    return false;
  }
  *out_value = static_cast<int>(value);
  return true;
}

bool WriteSummary(
    const std::string& output_path,
    int person_id,
    const sentriface::enroll::EnrollmentArtifactPackage& artifact,
    const sentriface::enroll::BaselineEnrollmentPlan& plan,
    const sentriface::enroll::BaselineGenerationConfig& config,
    const sentriface::enroll::BaselinePrototypePackage& package,
    const std::string& baseline_package_path,
    const std::string& search_index_path) {
  std::ofstream out(output_path);
  if (!out.good()) {
    return false;
  }

  out << "summary_path=" << artifact.summary_path << "\n";
  out << "artifact_dir=" << artifact.artifact_dir << "\n";
  out << "user_id=" << artifact.user_id << "\n";
  out << "user_name=" << artifact.user_name << "\n";
  out << "person_id=" << person_id << "\n";
  out << "source=" << artifact.source << "\n";
  out << "observation=" << artifact.observation << "\n";
  out << "capture_plan=" << artifact.capture_plan << "\n";
  out << "artifact_samples=" << artifact.samples.size() << "\n";
  out << "selected_samples=" << plan.samples.size() << "\n";
  out << "baseline_prototypes=" << package.prototypes.size() << "\n";
  out << "baseline_backend=" << sentriface::enroll::ToString(config.backend) << "\n";
  out << "baseline_binary_package=" << baseline_package_path << "\n";
  out << "search_index_package=" << search_index_path << "\n";
  out << "package_person_count=1\n";
  out << "package_baseline_count=" << package.prototypes.size() << "\n";

  for (const auto& sample : plan.samples) {
    out << "\n[sample]\n";
    out << "index=" << sample.sample_index << "\n";
    out << "slot=" << sample.slot << "\n";
    out << "preferred_image=" << sample.preferred_image_path << "\n";
    out << "frame_image=" << sample.frame_image_path << "\n";
    out << "face_crop=" << sample.face_crop_path << "\n";
    out << "quality_score=" << sample.quality_score << "\n";
    out << "yaw_deg=" << sample.yaw_deg << "\n";
    out << "pitch_deg=" << sample.pitch_deg << "\n";
    out << "roll_deg=" << sample.roll_deg << "\n";
  }

  for (const auto& prototype : package.prototypes) {
    out << "\n[prototype]\n";
    out << "sample_index=" << prototype.sample_index << "\n";
    out << "slot=" << prototype.slot << "\n";
    out << "source_image=" << prototype.source_image_path << "\n";
    out << "source_image_digest=" << prototype.source_image_digest << "\n";
    out << "embedding_dim=" << prototype.embedding.size() << "\n";
    out << "quality_score=" << prototype.metadata.quality_score << "\n";
    out << "manually_enrolled="
        << (prototype.metadata.manually_enrolled ? "1" : "0") << "\n";
  }

  return true;
}

bool WriteEmbeddingInputManifest(
    const std::string& output_path,
    const sentriface::enroll::BaselineEmbeddingInputManifest& manifest) {
  std::ofstream out(output_path);
  if (!out.good()) {
    return false;
  }

  out << "user_id=" << manifest.user_id << "\n";
  out << "user_name=" << manifest.user_name << "\n";
  for (const auto& record : manifest.records) {
    out << "\n[record]\n";
    out << "sample_index=" << record.sample_index << "\n";
    out << "slot=" << record.slot << "\n";
    out << "image_path=" << record.image_path << "\n";
    out << "quality_score=" << record.quality_score << "\n";
    out << "yaw_deg=" << record.yaw_deg << "\n";
    out << "pitch_deg=" << record.pitch_deg << "\n";
    out << "roll_deg=" << record.roll_deg << "\n";
  }

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    PrintUsage();
    return 1;
  }

  const std::string summary_path(argv[1]);

  int person_id = -1;
  if (!ParseIntArg(argv[2], &person_id) || person_id < 0) {
    std::cerr << "invalid person_id\n";
    return 1;
  }

  const std::string output_path =
      argc >= 4 ? std::string(argv[3]) : DefaultOutputPath(summary_path);

  int max_samples = 3;
  if (argc >= 5 && (!ParseIntArg(argv[4], &max_samples) || max_samples <= 0)) {
    std::cerr << "invalid max_samples\n";
    return 1;
  }

  int embedding_dim = 512;
  if (argc >= 6 &&
      (!ParseIntArg(argv[5], &embedding_dim) || embedding_dim <= 0)) {
    std::cerr << "invalid embedding_dim\n";
    return 1;
  }

  sentriface::enroll::BaselineGenerationBackend backend =
      sentriface::enroll::BaselineGenerationBackend::kMockDeterministic;
  if (argc >= 7) {
    const std::string backend_arg(argv[6]);
    if (backend_arg == "mock" || backend_arg == "mock_deterministic") {
      backend = sentriface::enroll::BaselineGenerationBackend::kMockDeterministic;
    } else if (backend_arg == "onnxruntime") {
      backend = sentriface::enroll::BaselineGenerationBackend::kOnnxRuntime;
    } else {
      std::cerr << "invalid backend\n";
      return 1;
    }
  }

  sentriface::enroll::EnrollmentArtifactPackage artifact;
  sentriface::enroll::BaselineEnrollmentPlan plan;
  sentriface::enroll::BaselineGenerationConfig generation_config;
  generation_config.backend = backend;
  generation_config.max_samples = max_samples;
  generation_config.embedding_dim = embedding_dim;
  if (argc >= 8) {
    generation_config.model_path = argv[7];
  }

  sentriface::enroll::BaselinePrototypePackage package;
  std::string baseline_package_path;
  std::string search_index_path;
  const auto generate_result =
      sentriface::enroll::GenerateAndSaveBaselinePackageArtifactsFromArtifactSummary(
          summary_path, person_id, generation_config, 1.0f, output_path,
          &artifact, &plan, &package, &baseline_package_path, &search_index_path);
  if (!generate_result.ok) {
    std::cerr << "generate_failed: " << generate_result.error_message << "\n";
    return 2;
  }

  if (!WriteSummary(
          output_path, person_id, artifact, plan, generation_config, package,
          baseline_package_path, search_index_path)) {
    std::cerr << "write_failed: unable_to_write_output_summary\n";
    return 4;
  }

  sentriface::enroll::BaselineEmbeddingInputManifest manifest;
  const auto manifest_result =
      sentriface::enroll::BuildBaselineEmbeddingInputManifest(plan, &manifest);
  if (!manifest_result.ok) {
    std::cerr << "manifest_failed: " << manifest_result.error_message << "\n";
    return 7;
  }

  const std::string manifest_path = output_path;
  const std::string embedding_input_path =
      manifest_path.substr(0, manifest_path.find_last_of('.')) +
      "_embedding_input_manifest.txt";
  if (!WriteEmbeddingInputManifest(embedding_input_path, manifest)) {
    std::cerr << "write_failed: unable_to_write_embedding_input_manifest\n";
    return 6;
  }

  std::cout << "baseline_artifact_summary=" << output_path << "\n";
  std::cout << "baseline_binary_package=" << baseline_package_path << "\n";
  std::cout << "search_index_package=" << search_index_path << "\n";
  std::cout << "baseline_embedding_input_manifest=" << embedding_input_path << "\n";
  std::cout << "selected_samples=" << plan.samples.size() << "\n";
  std::cout << "baseline_prototypes=" << package.prototypes.size() << "\n";
  return 0;
}
