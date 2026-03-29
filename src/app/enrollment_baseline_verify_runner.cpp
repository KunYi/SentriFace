#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "enroll/enrollment_baseline_generation.hpp"
#include "search/face_search.hpp"

namespace {

void PrintUsage() {
  std::cerr
      << "usage: enrollment_baseline_verify_runner <baseline_embedding_output.{csv|sfbp}> "
      << "<person_id> [output_summary.txt] [embedding_dim]\n";
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

std::string DefaultOutputPath(const std::string& csv_path) {
  const std::size_t pos = csv_path.find_last_of("/\\");
  const std::string dir =
      pos == std::string::npos ? "." : csv_path.substr(0, pos);
  return dir + "/baseline_verify_summary.txt";
}

bool WriteSummary(const std::string& output_path,
                  int expected_person_id,
                  const sentriface::enroll::BaselinePrototypePackage& package,
                  const sentriface::search::FaceSearchV2& search,
                  const std::string& search_index_input) {
  std::ofstream out(output_path);
  if (!out.good()) {
    return false;
  }

  int matched_top1 = 0;
  float min_top1_score = 1.0f;
  float min_margin = 1.0f;

  out << "user_id=" << package.user_id << "\n";
  out << "user_name=" << package.user_name << "\n";
  out << "person_id=" << expected_person_id << "\n";
  out << "query_prototypes=" << package.prototypes.size() << "\n";
  out << "search_index_input=" << search_index_input << "\n";

  for (const auto& prototype : package.prototypes) {
    const auto result = search.Query(prototype.embedding);
    const bool has_top1 = result.ok && !result.hits.empty();
    const int top1_person_id = has_top1 ? result.hits[0].person_id : -1;
    const float top1_score = has_top1 ? result.hits[0].score : 0.0f;
    const float margin = result.ok ? result.top1_top2_margin : 0.0f;
    const bool top1_match = top1_person_id == expected_person_id;

    if (top1_match) {
      ++matched_top1;
      if (top1_score < min_top1_score) {
        min_top1_score = top1_score;
      }
      if (margin < min_margin) {
        min_margin = margin;
      }
    }

    out << "\n[query]\n";
    out << "sample_index=" << prototype.sample_index << "\n";
    out << "slot=" << prototype.slot << "\n";
    out << "top1_person_id=" << top1_person_id << "\n";
    out << "top1_score=" << top1_score << "\n";
    out << "top1_match=" << (top1_match ? 1 : 0) << "\n";
    out << "top1_margin=" << margin << "\n";
    if (result.hits.size() >= 2U) {
      out << "top2_person_id=" << result.hits[1].person_id << "\n";
      out << "top2_score=" << result.hits[1].score << "\n";
    }
  }

  out << "\nmatched_top1=" << matched_top1 << "\n";
  out << "all_top1_match="
      << (matched_top1 == static_cast<int>(package.prototypes.size()) ? 1 : 0)
      << "\n";
  out << "min_top1_score="
      << (matched_top1 > 0 ? min_top1_score : 0.0f) << "\n";
  out << "min_top1_margin="
      << (matched_top1 > 0 ? min_margin : 0.0f) << "\n";
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    PrintUsage();
    return 1;
  }

  const std::string input_path(argv[1]);
  int person_id = -1;
  if (!ParseIntArg(argv[2], &person_id) || person_id < 0) {
    std::cerr << "invalid person_id\n";
    return 1;
  }

  const std::string output_path =
      argc >= 4 ? std::string(argv[3]) : DefaultOutputPath(input_path);
  int embedding_dim = 512;
  if (argc >= 5 &&
      (!ParseIntArg(argv[4], &embedding_dim) || embedding_dim <= 0)) {
    std::cerr << "invalid embedding_dim\n";
    return 1;
  }

  sentriface::enroll::BaselineEmbeddingCsvImportConfig import_config;
  import_config.embedding_dim = embedding_dim;
  import_config.manually_enrolled = true;

  sentriface::enroll::BaselinePrototypePackage package;
  const auto load_result = sentriface::enroll::LoadBaselinePrototypePackage(
      input_path, import_config, &package);
  if (!load_result.ok) {
    std::cerr << "load_failed: " << load_result.error_message << "\n";
    return 2;
  }
  const int package_embedding_dim =
      sentriface::enroll::InferBaselinePrototypePackageEmbeddingDim(package);
  if (package_embedding_dim > 0) {
    embedding_dim = package_embedding_dim;
  }

  sentriface::search::FaceSearchV2 search(
      sentriface::search::SearchConfig {.embedding_dim = embedding_dim, .top_k = 5});
  sentriface::search::FaceSearchV2IndexPackage index_package;
  std::string search_index_input;
  const auto index_result =
      sentriface::enroll::LoadOrBuildFaceSearchV2IndexPackage(
          input_path, package, person_id, embedding_dim, 1.0f, &index_package,
          &search_index_input);
  if (!index_result.ok || !search.LoadFromIndexPackage(index_package)) {
    std::cerr << "load_search_index_failed\n";
    return 5;
  }

  if (!WriteSummary(output_path, person_id, package, search, search_index_input)) {
    std::cerr << "write_failed\n";
    return 6;
  }

  std::cout << "baseline_verify_summary=" << output_path << "\n";
  std::cout << "search_index_input=" << search_index_input << "\n";
  std::cout << "query_prototypes=" << package.prototypes.size() << "\n";
  return 0;
}
