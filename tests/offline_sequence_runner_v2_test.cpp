#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "enroll/enrollment_baseline_generation.hpp"
#include "enroll/enrollment_store.hpp"
#include "search/face_search.hpp"

int main() {
  const std::string manifest = "offline_sequence_runner_v2_manifest.txt";
  const std::string detection_manifest = "offline_sequence_runner_v2_detections.txt";
  const std::string embedding_manifest = "offline_sequence_runner_v2_embeddings.txt";
  const std::string log_file = "offline_sequence_runner_v2.log";
  const std::string updated_index_output = "offline_sequence_runner_v2_updated.sfsi";
  const std::filesystem::path index_path =
      std::filesystem::current_path() / "offline_sequence_runner_v2.sfsi";
  const std::filesystem::path baseline_package_path =
      std::filesystem::current_path() / "offline_sequence_runner_v2.sfbp";
  {
    std::ofstream out(manifest);
    out << "frame_000.jpg 1000 640 640 3 rgb888\n";
    out << "frame_001.jpg 1033 640 640 3 rgb888\n";
    out << "frame_002.jpg 1066 640 640 3 rgb888\n";
  }
  {
    std::ofstream out(detection_manifest);
    out << "1000 100 120 136 152 0.95 128 156 188 155 158 196 136 240 186 239\n";
    out << "1033 104 121 136 152 0.96 132 157 192 156 162 197 140 241 190 240\n";
    out << "1066 108 122 136 152 0.97 136 158 196 157 166 198 144 242 194 241\n";
  }
  {
    std::ofstream out(embedding_manifest);
    out << "1033 0 1.0 0.0 0.0 0.0\n";
    out << "1066 0 1.0 0.0 0.0 0.0\n";
  }

  sentriface::search::SearchConfig config;
  config.embedding_dim = 4;
  config.top_k = 2;
  sentriface::search::FaceSearchV2 search(config);
  sentriface::search::FacePrototypeV2 prototype;
  prototype.person_id = 1;
  prototype.prototype_id = 10;
  prototype.zone = 0;
  prototype.label = "mock_person";
  prototype.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  prototype.prototype_weight = 1.0f;
  if (!search.AddPrototype(prototype)) {
    return 11;
  }
  if (!search.SaveIndexPackageBinary(index_path.string())) {
    return 12;
  }
  sentriface::enroll::BaselinePrototypePackage baseline_package;
  baseline_package.user_id = "E7002";
  baseline_package.user_name = "mock_person";
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
    return 16;
  }

  std::remove(log_file.c_str());

  const int rc = std::system(
      "SENTRIFACE_PIPELINE_USE_V2=1 "
      "SENTRIFACE_PIPELINE_APPLY_ADAPTIVE_UPDATES=1 "
      "SENTRIFACE_SEARCH_INDEX_PATH=offline_sequence_runner_v2.sfsi "
      "SENTRIFACE_UPDATED_SEARCH_INDEX_PATH=offline_sequence_runner_v2_updated.sfsi "
      "SENTRIFACE_LOG_ENABLE=1 "
      "SENTRIFACE_LOG_LEVEL=debug "
      "SENTRIFACE_LOG_BACKEND=file "
      "SENTRIFACE_LOG_MODULES=offline_runner,pipeline,access "
      "SENTRIFACE_LOG_FILE=offline_sequence_runner_v2.log "
      "\"" SENTRIFACE_BINARY_DIR "/offline_sequence_runner\" "
      "offline_sequence_runner_v2_manifest.txt "
      "offline_sequence_runner_v2_detections.txt "
      "offline_sequence_runner_v2_embeddings.txt "
      "> offline_sequence_runner_v2_stdout.txt");
  if (rc != 0) {
    return 1;
  }

  std::ifstream stdout_file("offline_sequence_runner_v2_stdout.txt");
  if (!stdout_file.is_open()) {
    return 2;
  }

  std::string line;
  bool saw_mode_v2 = false;
  bool saw_updated_search_index = false;
  while (std::getline(stdout_file, line)) {
    if (line == "pipeline_mode=v2") {
      saw_mode_v2 = true;
    }
    if (line == "offline_updated_search_index=" + updated_index_output) {
      saw_updated_search_index = true;
    }
  }
  if (!saw_mode_v2) {
    return 3;
  }

  std::ifstream report("offline_sequence_report.txt");
  if (!report.is_open()) {
    return 4;
  }

  bool saw_unlock_actions = false;
  bool saw_adaptive_updates = false;
  bool saw_report_updated_search_index = false;
  while (std::getline(report, line)) {
    if (line.rfind("unlock_actions=", 0) == 0) {
      saw_unlock_actions = true;
    }
    if (line.rfind("adaptive_updates_applied=", 0) == 0) {
      saw_adaptive_updates = true;
    }
    if (line == "offline_updated_search_index=" + updated_index_output) {
      saw_report_updated_search_index = true;
    }
  }

  if (!saw_unlock_actions || !saw_adaptive_updates || !saw_report_updated_search_index) {
    return 5;
  }

  std::ifstream update_timeline("offline_prototype_updates.csv");
  if (!update_timeline.is_open()) {
    return 6;
  }

  if (!std::getline(update_timeline, line)) {
    return 7;
  }
  if (line.find("accepted,cooldown_blocked") == std::string::npos ||
      line.find("rejection_reason") == std::string::npos) {
    return 8;
  }

  std::ifstream log_in(log_file);
  if (!log_in.is_open()) {
    return 9;
  }
  std::string log_content((std::istreambuf_iterator<char>(log_in)),
                          std::istreambuf_iterator<char>());
  if (log_content.find(" I/offline_runner: offline_sequence_runner_started") ==
          std::string::npos ||
      log_content.find(" I/offline_runner: offline_sequence_runner_finished") ==
          std::string::npos) {
    return 10;
  }
  if (!saw_updated_search_index ||
      !std::filesystem::exists(updated_index_output)) {
    return 13;
  }
  sentriface::enroll::EnrollmentConfigV2 updated_store_config;
  updated_store_config.embedding_dim = 4;
  sentriface::enroll::EnrollmentStoreV2 updated_store(updated_store_config);
  if (!updated_store.LoadFromSearchIndexPackagePath(updated_index_output)) {
    return 14;
  }
  if (updated_store.PrototypeCount(1, sentriface::enroll::PrototypeZone::kRecentAdaptive) !=
      1U) {
    return 15;
  }
  std::filesystem::remove(updated_index_output);
  const int baseline_rc = std::system(
      "SENTRIFACE_PIPELINE_USE_V2=1 "
      "SENTRIFACE_PIPELINE_APPLY_ADAPTIVE_UPDATES=1 "
      "SENTRIFACE_BASELINE_PACKAGE_PATH=offline_sequence_runner_v2.sfbp "
      "SENTRIFACE_BASELINE_PERSON_ID=7 "
      "SENTRIFACE_UPDATED_SEARCH_INDEX_PATH=offline_sequence_runner_v2_updated.sfsi "
      "\"" SENTRIFACE_BINARY_DIR "/offline_sequence_runner\" "
      "offline_sequence_runner_v2_manifest.txt "
      "offline_sequence_runner_v2_detections.txt "
      "offline_sequence_runner_v2_embeddings.txt "
      "> offline_sequence_runner_v2_baseline_stdout.txt");
  if (baseline_rc != 0) {
    return 17;
  }
  std::ifstream baseline_stdout("offline_sequence_runner_v2_baseline_stdout.txt");
  if (!baseline_stdout.is_open()) {
    return 18;
  }
  bool baseline_saw_mode_v2 = false;
  while (std::getline(baseline_stdout, line)) {
    if (line == "pipeline_mode=v2") {
      baseline_saw_mode_v2 = true;
      break;
    }
  }
  if (!baseline_saw_mode_v2) {
    return 19;
  }
  sentriface::enroll::EnrollmentConfigV2 baseline_store_config;
  baseline_store_config.embedding_dim = 4;
  sentriface::enroll::EnrollmentStoreV2 baseline_store(baseline_store_config);
  sentriface::enroll::BaselineEmbeddingCsvImportConfig baseline_import_config;
  baseline_import_config.embedding_dim = 4;
  const auto baseline_load_result =
      sentriface::enroll::LoadBaselinePrototypePackageIntoStoreV2(
          baseline_package_path.string(), baseline_import_config, 7,
          &baseline_store);
  if (!baseline_load_result.ok) {
    return 20;
  }
  if (baseline_store.PrototypeCount(7, sentriface::enroll::PrototypeZone::kBaseline) !=
      1U) {
    return 21;
  }

  std::filesystem::remove(index_path);
  std::filesystem::remove(baseline_package_path);
  std::filesystem::remove(updated_index_output);

  return 0;
}
