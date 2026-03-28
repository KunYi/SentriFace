#include <cstdlib>
#include <fstream>
#include <string>

int main() {
  const std::string manifest = "offline_sequence_runner_v2_manifest.txt";
  const std::string detection_manifest = "offline_sequence_runner_v2_detections.txt";
  const std::string embedding_manifest = "offline_sequence_runner_v2_embeddings.txt";
  const std::string log_file = "offline_sequence_runner_v2.log";
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

  std::remove(log_file.c_str());

  const int rc = std::system(
      "SENTRIFACE_PIPELINE_USE_V2=1 "
      "SENTRIFACE_PIPELINE_APPLY_ADAPTIVE_UPDATES=1 "
      "SENTRIFACE_LOG_ENABLE=1 "
      "SENTRIFACE_LOG_LEVEL=debug "
      "SENTRIFACE_LOG_BACKEND=file "
      "SENTRIFACE_LOG_MODULES=offline_runner,pipeline,access "
      "SENTRIFACE_LOG_FILE=offline_sequence_runner_v2.log "
      "\"" SENTRIFACE_BINARY_DIR "/offline_sequence_runner\" "
      "offline_sequence_runner_v2_manifest.txt "
      "offline_sequence_runner_v2_detections.txt "
      "offline_sequence_runner_v2_embeddings.txt "
      "> offline_sequence_runner_v2_stdout.txt");
  if (rc != 0) {
    return 1;
  }

  std::ifstream stdout_file("offline_sequence_runner_v2_stdout.txt");
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

  std::ifstream report("offline_sequence_report.txt");
  if (!report.is_open()) {
    return 4;
  }

  bool saw_unlock_actions = false;
  bool saw_adaptive_updates = false;
  while (std::getline(report, line)) {
    if (line.rfind("unlock_actions=", 0) == 0) {
      saw_unlock_actions = true;
    }
    if (line.rfind("adaptive_updates_applied=", 0) == 0) {
      saw_adaptive_updates = true;
    }
  }

  if (!saw_unlock_actions || !saw_adaptive_updates) {
    return 5;
  }

  std::ifstream update_timeline("offline_prototype_updates.csv");
  if (!update_timeline.is_open()) {
    return 6;
  }

  if (!std::getline(update_timeline, line)) {
    return 7;
  }
  if (line.find("accepted,cooldown_blocked") == std::string::npos ||
      line.find("rejection_reason") == std::string::npos) {
    return 8;
  }

  std::ifstream log_in(log_file);
  if (!log_in.is_open()) {
    return 9;
  }
  std::string log_content((std::istreambuf_iterator<char>(log_in)),
                          std::istreambuf_iterator<char>());
  if (log_content.find(" I/offline_runner: offline_sequence_runner_started") ==
          std::string::npos ||
      log_content.find(" I/offline_runner: offline_sequence_runner_finished") ==
          std::string::npos) {
    return 10;
  }

  return 0;
}
