#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "enroll/enrollment_baseline_generation.hpp"

#ifndef SENTRIFACE_ENROLLMENT_BASELINE_IMPORT_RUNNER_PATH
#define SENTRIFACE_ENROLLMENT_BASELINE_IMPORT_RUNNER_PATH "enrollment_baseline_import_runner"
#endif

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Expectation failed: " << message << std::endl;
    std::exit(1);
  }
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  const std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() / "sentriface_enrollment_baseline_import_runner_test";
  std::filesystem::remove_all(temp_dir);
  std::filesystem::create_directories(temp_dir);

  const std::filesystem::path csv_path = temp_dir / "baseline_embedding_output.csv";
  const std::filesystem::path output_path = temp_dir / "baseline_import_summary.txt";
  const std::filesystem::path binary_path = temp_dir / "baseline_embedding_output.sfbp";
  const std::filesystem::path search_index_path = temp_dir / "baseline_embedding_output.sfsi";
  {
    std::ofstream csv(csv_path);
    csv << "user_id,user_name,sample_index,slot,image_path,quality_score,yaw_deg,pitch_deg,roll_deg,e0,e1,e2,e3\n";
    csv << "E9101,Import_User,1,frontal,/tmp/sample_a.bmp,0.95,0,0,0,1,0,0,0\n";
    csv << "E9101,Import_User,2,left_quarter,/tmp/sample_b.bmp,0.90,-12,0,0,0,1,0,0\n";
  }

  sentriface::enroll::BaselineEmbeddingCsvImportConfig import_config;
  import_config.embedding_dim = 4;
  import_config.manually_enrolled = true;
  sentriface::enroll::BaselinePrototypePackage package;
  const auto load_result = sentriface::enroll::LoadBaselinePrototypePackage(
      csv_path.string(), import_config, &package);
  Expect(load_result.ok, "preload csv should succeed");
  const auto save_binary_result =
      sentriface::enroll::SaveBaselinePrototypePackageBinary(
          package, binary_path.string());
  Expect(save_binary_result.ok, "pre-save binary package should succeed");

  const std::string command =
      std::string("\"") +
      std::string(SENTRIFACE_ENROLLMENT_BASELINE_IMPORT_RUNNER_PATH) +
      "\" \"" + binary_path.string() + "\" 91 \"" + output_path.string() + "\" 4";
  const int rc = std::system(command.c_str());
  Expect(rc == 0, "baseline import runner should succeed on package input");
  Expect(std::filesystem::exists(output_path), "import summary should exist");
  Expect(std::filesystem::exists(binary_path), "binary package should exist");
  Expect(std::filesystem::exists(search_index_path), "search index package should exist");

  const std::string text = ReadTextFile(output_path);
  Expect(text.find("user_id=E9101") != std::string::npos,
         "summary should contain user_id");
  Expect(text.find("person_id=91") != std::string::npos,
         "summary should contain person_id");
  Expect(text.find("baseline_prototypes=2") != std::string::npos,
         "summary should contain prototype count");
  Expect(text.find("package_baseline_count=2") != std::string::npos,
         "summary should contain package count");
  Expect(text.find("binary_package=" + binary_path.string()) != std::string::npos,
         "summary should contain binary package path");
  Expect(text.find("search_index_package=" + search_index_path.string()) !=
             std::string::npos,
         "summary should contain search index package path");

  const std::filesystem::path output_from_binary_path =
      temp_dir / "baseline_import_summary_from_binary.txt";
  const std::string binary_command =
      std::string("\"") +
      std::string(SENTRIFACE_ENROLLMENT_BASELINE_IMPORT_RUNNER_PATH) +
      "\" \"" + binary_path.string() + "\" 91 \"" +
      output_from_binary_path.string() + "\" 4";
  const int binary_rc = std::system(binary_command.c_str());
  Expect(binary_rc == 0, "baseline import runner should accept binary package");
  Expect(std::filesystem::exists(output_from_binary_path),
         "binary import summary should exist");

  const std::filesystem::path output_from_csv_path =
      temp_dir / "baseline_import_summary_from_csv.txt";
  const std::string csv_command =
      std::string("\"") +
      std::string(SENTRIFACE_ENROLLMENT_BASELINE_IMPORT_RUNNER_PATH) +
      "\" \"" + csv_path.string() + "\" 91 \"" +
      output_from_csv_path.string() + "\" 4";
  const int csv_rc = std::system(csv_command.c_str());
  Expect(csv_rc == 0, "baseline import runner should still accept csv input");
  Expect(std::filesystem::exists(output_from_csv_path),
         "csv import summary should exist");

  const std::filesystem::path default_output_path =
      temp_dir / "baseline_import_summary.txt";
  std::filesystem::remove(default_output_path);
  const std::string default_command =
      std::string("\"") +
      std::string(SENTRIFACE_ENROLLMENT_BASELINE_IMPORT_RUNNER_PATH) +
      "\" \"" + binary_path.string() + "\" 91";
  const int default_rc = std::system(default_command.c_str());
  Expect(default_rc == 0, "baseline import runner should support default summary path");
  Expect(std::filesystem::exists(default_output_path),
         "baseline import runner should use baseline_import_summary.txt by default");

  std::filesystem::remove_all(temp_dir);
  return 0;
}
