#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "enroll/enrollment_baseline_generation.hpp"
#include "search/face_search.hpp"

int main() {
  const std::filesystem::path index_path =
      std::filesystem::current_path() / "sample_pipeline_runner_v2.sfsi";
  const std::filesystem::path baseline_package_path =
      std::filesystem::current_path() / "sample_pipeline_runner_v2.sfbp";
  sentriface::search::SearchConfig config;
  config.embedding_dim = 4;
  config.top_k = 2;
  sentriface::search::FaceSearchV2 search(config);
  sentriface::search::FacePrototypeV2 prototype;
  prototype.person_id = 1;
  prototype.prototype_id = 10;
  prototype.zone = 0;
  prototype.label = "alice";
  prototype.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  prototype.prototype_weight = 1.0f;
  if (!search.AddPrototype(prototype)) {
    return 10;
  }
  if (!search.SaveIndexPackageBinary(index_path.string())) {
    return 11;
  }
  sentriface::enroll::BaselinePrototypePackage baseline_package;
  baseline_package.user_id = "E7001";
  baseline_package.user_name = "alice";
  sentriface::enroll::BaselinePrototypeRecord record;
  record.sample_index = 1;
  record.slot = "frontal";
  record.source_image_path = "sample_01.bmp";
  record.source_image_digest = "1234abcd";
  record.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  record.metadata.manually_enrolled = true;
  baseline_package.prototypes.push_back(record);
  const auto save_baseline_result =
      sentriface::enroll::SaveBaselinePrototypePackageBinary(
          baseline_package, baseline_package_path.string());
  if (!save_baseline_result.ok) {
    return 12;
  }

  const int rc = std::system(
      "SENTRIFACE_PIPELINE_USE_V2=1 "
      "SENTRIFACE_SEARCH_INDEX_PATH=sample_pipeline_runner_v2.sfsi "
      "\"" SENTRIFACE_BINARY_DIR "/sample_pipeline_runner\" "
      "> sample_pipeline_runner_v2_stdout.txt");
  if (rc != 0) {
    return 1;
  }

  std::ifstream stdout_file("sample_pipeline_runner_v2_stdout.txt");
  if (!stdout_file.is_open()) {
    return 2;
  }

  std::string line;
  bool saw_mode_v2 = false;
  while (std::getline(stdout_file, line)) {
    if (line == "pipeline_mode=v2") {
      saw_mode_v2 = true;
      break;
    }
  }
  if (!saw_mode_v2) {
    return 3;
  }

  const int baseline_rc = std::system(
      "SENTRIFACE_PIPELINE_USE_V2=1 "
      "SENTRIFACE_BASELINE_PACKAGE_PATH=sample_pipeline_runner_v2.sfbp "
      "SENTRIFACE_BASELINE_PERSON_ID=7 "
      "\"" SENTRIFACE_BINARY_DIR "/sample_pipeline_runner\" "
      "> sample_pipeline_runner_v2_baseline_stdout.txt");
  if (baseline_rc != 0) {
    return 4;
  }

  std::ifstream report("sample_pipeline_report.txt");
  if (!report.is_open()) {
    return 5;
  }

  bool saw_short_gap_merges = false;
  while (std::getline(report, line)) {
    if (line == "short_gap_merges=1") {
      saw_short_gap_merges = true;
      break;
    }
  }

  std::filesystem::remove(index_path);
  std::filesystem::remove(baseline_package_path);

  return saw_short_gap_merges ? 0 : 6;
}
