#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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
  {
    std::ofstream csv(csv_path);
    csv << "user_id,user_name,sample_index,slot,image_path,quality_score,yaw_deg,pitch_deg,roll_deg,e0,e1,e2,e3\n";
    csv << "E9101,Import_User,1,frontal,/tmp/sample_a.bmp,0.95,0,0,0,1,0,0,0\n";
    csv << "E9101,Import_User,2,left_quarter,/tmp/sample_b.bmp,0.90,-12,0,0,0,1,0,0\n";
  }

  const std::string command =
      std::string("\"") +
      std::string(SENTRIFACE_ENROLLMENT_BASELINE_IMPORT_RUNNER_PATH) +
      "\" \"" + csv_path.string() + "\" 91 \"" + output_path.string() + "\" 4";
  const int rc = std::system(command.c_str());
  Expect(rc == 0, "baseline import runner should succeed");
  Expect(std::filesystem::exists(output_path), "import summary should exist");

  const std::string text = ReadTextFile(output_path);
  Expect(text.find("user_id=E9101") != std::string::npos,
         "summary should contain user_id");
  Expect(text.find("person_id=91") != std::string::npos,
         "summary should contain person_id");
  Expect(text.find("baseline_prototypes=2") != std::string::npos,
         "summary should contain prototype count");
  Expect(text.find("store_baseline_count=2") != std::string::npos,
         "summary should contain store count");

  std::filesystem::remove_all(temp_dir);
  return 0;
}
