#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "enroll/enrollment_baseline_generation.hpp"
#include "enroll/enrollment_store.hpp"
#include "search/face_search.hpp"

namespace {

void PrintUsage() {
  std::cerr
      << "usage: enrollment_baseline_verify_runner <baseline_embedding_output.csv> "
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

bool BuildSearchIndex(const sentriface::enroll::EnrollmentStoreV2& store,
                      int embedding_dim,
                      sentriface::search::FaceSearchV2* out_search) {
  if (out_search == nullptr) {
    return false;
  }
  sentriface::search::FaceSearchV2 search(
      sentriface::search::SearchConfig {.embedding_dim = embedding_dim, .top_k = 5});
  for (const auto& prototype : store.ExportWeightedPrototypes()) {
    if (!search.AddPrototype(prototype)) {
      return false;
    }
  }
  *out_search = std::move(search);
  return true;
}

bool WriteSummary(const std::string& output_path,
                  int expected_person_id,
                  const sentriface::enroll::BaselinePrototypePackage& package,
                  const sentriface::search::FaceSearchV2& search) {
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

  const std::string csv_path(argv[1]);
  int person_id = -1;
  if (!ParseIntArg(argv[2], &person_id) || person_id < 0) {
    std::cerr << "invalid person_id\n";
    return 1;
  }

  const std::string output_path =
      argc >= 4 ? std::string(argv[3]) : DefaultOutputPath(csv_path);
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
  const auto load_result =
      sentriface::enroll::LoadBaselinePrototypePackageFromEmbeddingCsv(
          csv_path, import_config, &package);
  if (!load_result.ok) {
    std::cerr << "load_failed: " << load_result.error_message << "\n";
    return 2;
  }

  sentriface::enroll::EnrollmentStoreV2 store(
      sentriface::enroll::EnrollmentConfigV2 {.embedding_dim = embedding_dim});
  if (!store.UpsertPerson(person_id, package.user_name)) {
    std::cerr << "upsert_person_failed\n";
    return 3;
  }

  const auto apply_result =
      sentriface::enroll::ApplyBaselinePrototypePackageToStoreV2(
          package, person_id, &store);
  if (!apply_result.ok) {
    std::cerr << "apply_failed: " << apply_result.error_message << "\n";
    return 4;
  }

  sentriface::search::FaceSearchV2 search;
  if (!BuildSearchIndex(store, embedding_dim, &search)) {
    std::cerr << "build_search_failed\n";
    return 5;
  }

  if (!WriteSummary(output_path, person_id, package, search)) {
    std::cerr << "write_failed\n";
    return 6;
  }

  std::cout << "baseline_verify_summary=" << output_path << "\n";
  std::cout << "query_prototypes=" << package.prototypes.size() << "\n";
  return 0;
}
