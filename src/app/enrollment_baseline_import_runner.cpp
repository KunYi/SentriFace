#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "enroll/enrollment_baseline_generation.hpp"

namespace {

void PrintUsage() {
  std::cerr
      << "usage: enrollment_baseline_import_runner <baseline_embedding_output.{csv|sfbp}> "
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
                  const std::string& binary_package_path,
                  const std::string& search_index_path) {
  std::ofstream out(output_path);
  if (!out.good()) {
    return false;
  }
  out << "user_id=" << package.user_id << "\n";
  out << "user_name=" << package.user_name << "\n";
  out << "person_id=" << person_id << "\n";
  out << "baseline_prototypes=" << package.prototypes.size() << "\n";
  out << "binary_package=" << binary_package_path << "\n";
  out << "search_index_package=" << search_index_path << "\n";
  out << "package_person_count=1\n";
  out << "package_baseline_count=" << package.prototypes.size() << "\n";

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

  const std::string binary_package_path =
      sentriface::enroll::MakeBaselinePrototypePackagePath(input_path);
  const std::string search_index_path =
      sentriface::enroll::MakeFaceSearchV2IndexPath(input_path);
  const auto save_artifacts_result =
      sentriface::enroll::SaveBaselinePackageArtifacts(
          package, person_id, embedding_dim, 1.0f, binary_package_path,
          search_index_path);
  if (!save_artifacts_result.ok) {
    std::cerr << "save_artifacts_failed: "
              << save_artifacts_result.error_message << "\n";
    return 3;
  }

  if (!WriteSummary(output_path, person_id, package, binary_package_path,
                    search_index_path)) {
    std::cerr << "write_failed\n";
    return 4;
  }

  std::cout << "baseline_import_summary=" << output_path << "\n";
  std::cout << "baseline_binary_package=" << binary_package_path << "\n";
  std::cout << "search_index_package=" << search_index_path << "\n";
  std::cout << "baseline_prototypes=" << package.prototypes.size() << "\n";
  return 0;
}
