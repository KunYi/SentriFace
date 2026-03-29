#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "enroll/enrollment_baseline_generation.hpp"
#include "search/face_search.hpp"

#ifndef SENTRIFACE_ENROLLMENT_BASELINE_VERIFY_RUNNER_PATH
#define SENTRIFACE_ENROLLMENT_BASELINE_VERIFY_RUNNER_PATH "enrollment_baseline_verify_runner"
#endif

namespace {

bool Contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

}  // namespace

int main() {
  const std::filesystem::path work_dir =
      std::filesystem::temp_directory_path() / "sentriface_baseline_verify_runner_test";
  std::filesystem::create_directories(work_dir);

  const std::filesystem::path csv_path = work_dir / "baseline_embedding_output.csv";
  const std::filesystem::path binary_path = work_dir / "baseline_embedding_output.sfbp";
  const std::filesystem::path search_index_path = work_dir / "baseline_embedding_output.sfsi";
  {
    std::ofstream out(csv_path);
    out << "user_id,user_name,sample_index,slot,image_path,quality_score,yaw_deg,pitch_deg,roll_deg,e0,e1,e2,e3\n";
    out << "E9201,Jamie,1,frontal,/tmp/a.bmp,0.95,0,0,0,1,0,0,0\n";
    out << "E9201,Jamie,2,left_quarter,/tmp/b.bmp,0.91,0,0,0,0.9,0.1,0,0\n";
  }

  sentriface::enroll::BaselineEmbeddingCsvImportConfig import_config;
  import_config.embedding_dim = 4;
  sentriface::enroll::BaselinePrototypePackage package;
  const auto load_result = sentriface::enroll::LoadBaselinePrototypePackage(
      csv_path.string(), import_config, &package);
  if (!load_result.ok) {
    return 10;
  }
  const auto save_binary_result =
      sentriface::enroll::SaveBaselinePrototypePackageBinary(
          package, binary_path.string());
  if (!save_binary_result.ok) {
    return 11;
  }

  sentriface::search::SearchConfig search_config;
  search_config.embedding_dim = 4;
  search_config.top_k = 5;
  sentriface::search::FaceSearchV2 search(search_config);
  sentriface::search::FacePrototypeV2 prototype_a;
  prototype_a.person_id = 92;
  prototype_a.prototype_id = 1;
  prototype_a.zone = 0;
  prototype_a.label = "Jamie";
  prototype_a.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  prototype_a.prototype_weight = 1.0f;
  sentriface::search::FacePrototypeV2 prototype_b;
  prototype_b.person_id = 92;
  prototype_b.prototype_id = 2;
  prototype_b.zone = 0;
  prototype_b.label = "Jamie";
  prototype_b.embedding = {0.9f, 0.1f, 0.0f, 0.0f};
  prototype_b.prototype_weight = 1.0f;
  if (!search.AddPrototype(prototype_a) || !search.AddPrototype(prototype_b)) {
    return 12;
  }
  if (!search.SaveIndexPackageBinary(search_index_path.string())) {
    return 13;
  }

  const std::filesystem::path output_path = work_dir / "baseline_verify_summary.txt";
  const std::string command =
      std::string(SENTRIFACE_ENROLLMENT_BASELINE_VERIFY_RUNNER_PATH) + " " +
      binary_path.string() + " 92 " + output_path.string() + " 4";
  const int rc = std::system(command.c_str());
  if (rc != 0) {
    return 1;
  }

  const std::string summary = ReadFile(output_path);
  if (!Contains(summary, "person_id=92")) {
    return 2;
  }
  if (!Contains(summary, "query_prototypes=2")) {
    return 3;
  }
  if (!Contains(summary, "search_index_input=" + search_index_path.string())) {
    return 6;
  }
  if (!Contains(summary, "matched_top1=2")) {
    return 4;
  }
  if (!Contains(summary, "all_top1_match=1")) {
    return 5;
  }

  std::filesystem::remove(search_index_path);
  const std::filesystem::path fallback_output_path =
      work_dir / "baseline_verify_summary_fallback.txt";
  const std::string fallback_command =
      std::string(SENTRIFACE_ENROLLMENT_BASELINE_VERIFY_RUNNER_PATH) + " " +
      binary_path.string() + " 92 " + fallback_output_path.string() + " 4";
  const int fallback_rc = std::system(fallback_command.c_str());
  if (fallback_rc != 0) {
    return 7;
  }

  const std::string fallback_summary = ReadFile(fallback_output_path);
  if (!Contains(fallback_summary, "search_index_input=package_rebuild")) {
    return 8;
  }
  if (!Contains(fallback_summary, "all_top1_match=1")) {
    return 9;
  }
  if (!std::filesystem::exists(search_index_path)) {
    return 10;
  }
  const std::filesystem::path rerun_output_path =
      work_dir / "baseline_verify_summary_rerun.txt";
  const std::string rerun_command =
      std::string(SENTRIFACE_ENROLLMENT_BASELINE_VERIFY_RUNNER_PATH) + " " +
      binary_path.string() + " 92 " + rerun_output_path.string() + " 4";
  const int rerun_rc = std::system(rerun_command.c_str());
  if (rerun_rc != 0) {
    return 11;
  }
  const std::string rerun_summary = ReadFile(rerun_output_path);
  if (!Contains(rerun_summary, "search_index_input=" + search_index_path.string())) {
    return 12;
  }

  const std::filesystem::path default_output_path =
      work_dir / "baseline_verify_summary.txt";
  std::filesystem::remove(default_output_path);
  const std::string default_command =
      std::string(SENTRIFACE_ENROLLMENT_BASELINE_VERIFY_RUNNER_PATH) + " " +
      binary_path.string() + " 92";
  const int default_rc = std::system(default_command.c_str());
  if (default_rc != 0) {
    return 13;
  }
  if (!std::filesystem::exists(default_output_path)) {
    return 14;
  }

  std::filesystem::remove_all(work_dir);
  return 0;
}
