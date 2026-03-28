#include <cstdlib>
#include <fstream>
#include <string>

int main() {
  const int rc = std::system(
      "SENTRIFACE_PIPELINE_USE_V2=1 "
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

  std::ifstream report("sample_pipeline_report.txt");
  if (!report.is_open()) {
    return 4;
  }

  bool saw_short_gap_merges = false;
  while (std::getline(report, line)) {
    if (line == "short_gap_merges=1") {
      saw_short_gap_merges = true;
      break;
    }
  }

  return saw_short_gap_merges ? 0 : 5;
}
