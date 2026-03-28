#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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
  {
    std::ofstream out(csv_path);
    out << "user_id,user_name,sample_index,slot,image_path,quality_score,yaw_deg,pitch_deg,roll_deg,e0,e1,e2,e3\n";
    out << "E9201,Jamie,1,frontal,/tmp/a.bmp,0.95,0,0,0,1,0,0,0\n";
    out << "E9201,Jamie,2,left_quarter,/tmp/b.bmp,0.91,0,0,0,0.9,0.1,0,0\n";
  }

  const std::filesystem::path output_path = work_dir / "baseline_verify_summary.txt";
  const std::string command =
      std::string(SENTRIFACE_ENROLLMENT_BASELINE_VERIFY_RUNNER_PATH) + " " +
      csv_path.string() + " 92 " + output_path.string() + " 4";
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
  if (!Contains(summary, "matched_top1=2")) {
    return 4;
  }
  if (!Contains(summary, "all_top1_match=1")) {
    return 5;
  }

  std::filesystem::remove_all(work_dir);
  return 0;
}
