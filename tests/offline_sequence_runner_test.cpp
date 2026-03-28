#include <cstdlib>
#include <fstream>
#include <string>

int main() {
  const std::string manifest = "offline_sequence_runner_manifest.txt";
  const std::string detection_manifest = "offline_sequence_runner_detections.txt";
  const std::string embedding_manifest = "offline_sequence_runner_embeddings.txt";
  {
    std::ofstream out(manifest);
    out << "frame_000.jpg 1000 640 640 3 rgb888\n";
    out << "frame_001.jpg 1033 640 640 3 rgb888\n";
    out << "frame_002.jpg 1066 640 640 3 rgb888\n";
  }
  {
    std::ofstream out(detection_manifest);
    out << "1000 100 120 136 152 0.95 128 156 188 155 158 196 136 240 186 239\n";
    out << "1033 104 121 136 152 0.96 132 157 192 156 162 197 140 241 190 240\n";
    out << "1066 108 122 136 152 0.97 136 158 196 157 166 198 144 242 194 241\n";
  }
  {
    std::ofstream out(embedding_manifest);
    out << "1033 0 1.0 0.0 0.0 0.0\n";
    out << "1066 0 1.0 0.0 0.0 0.0\n";
  }

  const int rc = std::system(
      "\"" SENTRIFACE_BINARY_DIR "/offline_sequence_runner\" "
      "offline_sequence_runner_manifest.txt "
      "offline_sequence_runner_detections.txt "
      "offline_sequence_runner_embeddings.txt > /dev/null");
  if (rc != 0) {
    return 1;
  }

  std::ifstream timeline("offline_tracker_timeline.csv");
  if (!timeline.is_open()) {
    return 2;
  }

  std::string header;
  std::getline(timeline, header);
  if (header.find("timestamp_ms,track_id,state") != 0) {
    return 3;
  }

  std::string row;
  if (!std::getline(timeline, row) || row.empty()) {
    return 4;
  }

  std::ifstream decision_timeline("offline_decision_timeline.csv");
  if (!decision_timeline.is_open()) {
    return 5;
  }

  std::string decision_header;
  std::getline(decision_timeline, decision_header);
  if (decision_header.find("timestamp_ms,track_id,stable_person_id") != 0) {
    return 6;
  }

  std::ifstream report("offline_sequence_report.txt");
  if (!report.is_open()) {
    return 7;
  }
  bool saw_short_gap_merges = false;
  bool saw_unlock_actions = false;
  while (std::getline(report, row)) {
    if (row.rfind("short_gap_merges=", 0) == 0) {
      saw_short_gap_merges = true;
    }
    if (row.rfind("unlock_actions=", 0) == 0) {
      saw_unlock_actions = true;
    }
  }
  if (!saw_short_gap_merges) {
    return 8;
  }
  if (!saw_unlock_actions) {
    return 9;
  }

  return 0;
}
