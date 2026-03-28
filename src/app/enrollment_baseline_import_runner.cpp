#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "enroll/enrollment_baseline_generation.hpp"

namespace {

void PrintUsage() {
  std::cerr
      << "usage: enrollment_baseline_import_runner <baseline_embedding_output.csv> "
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
  return dir + "/baseline_import_summary.txt";
}

bool WriteSummary(const std::string& output_path,
                  int person_id,
                  const sentriface::enroll::BaselinePrototypePackage& package,
                  const sentriface::enroll::EnrollmentStoreV2& store) {
  std::ofstream out(output_path);
  if (!out.good()) {
    return false;
  }
  out << "user_id=" << package.user_id << "\n";
  out << "user_name=" << package.user_name << "\n";
  out << "person_id=" << person_id << "\n";
  out << "baseline_prototypes=" << package.prototypes.size() << "\n";
  out << "store_person_count=" << store.PersonCount() << "\n";
  out << "store_baseline_count="
      << store.PrototypeCount(person_id, sentriface::enroll::PrototypeZone::kBaseline)
      << "\n";

  for (const auto& prototype : package.prototypes) {
    out << "\n[prototype]\n";
    out << "sample_index=" << prototype.sample_index << "\n";
    out << "slot=" << prototype.slot << "\n";
    out << "source_image=" << prototype.source_image_path << "\n";
    out << "source_image_digest=" << prototype.source_image_digest << "\n";
    out << "embedding_dim=" << prototype.embedding.size() << "\n";
    out << "quality_score=" << prototype.metadata.quality_score << "\n";
  }
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

  if (!WriteSummary(output_path, person_id, package, store)) {
    std::cerr << "write_failed\n";
    return 5;
  }

  std::cout << "baseline_import_summary=" << output_path << "\n";
  std::cout << "baseline_prototypes=" << package.prototypes.size() << "\n";
  return 0;
}
