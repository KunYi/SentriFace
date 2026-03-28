#include <cstdlib>
#include <fstream>
#include <string>

int main() {
  const int rc = std::system("\"" SENTRIFACE_BINARY_DIR "/sample_pipeline_runner\" > /dev/null");
  if (rc != 0) {
    return 1;
  }

  std::ifstream report("sample_pipeline_report.txt");
  if (!report.is_open()) {
    return 2;
  }

  std::string line;
  bool saw_short_gap_merges = false;
  bool saw_unlock_actions = false;
  while (std::getline(report, line)) {
    if (line == "short_gap_merges=1") {
      saw_short_gap_merges = true;
    }
    if (line == "unlock_actions=0") {
      saw_unlock_actions = true;
    }
  }
  return (saw_short_gap_merges && saw_unlock_actions) ? 0 : 3;
}
