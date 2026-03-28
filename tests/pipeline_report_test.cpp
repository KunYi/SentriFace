#include <fstream>
#include <string>

#include "app/pipeline_report.hpp"

int main() {
  using sentriface::app::PipelineRunStats;
  using sentriface::app::WritePipelineReport;

  const std::string path = "pipeline_report_test.txt";
  PipelineRunStats stats;
  stats.frames_processed = 3;
  stats.detections_processed = 3;
  stats.tracks_seen = 1;
  stats.embedding_triggers = 2;
  stats.unlock_ready_tracks = 1;
  stats.unlock_actions = 1;
  stats.adaptive_updates_applied = 2;

  if (!WritePipelineReport(path, stats)) {
    return 1;
  }

  std::ifstream in(path);
  if (!in.is_open()) {
    return 2;
  }

  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  if (content.find("frames_processed=3") == std::string::npos) {
    return 3;
  }
  if (content.find("unlock_ready_tracks=1") == std::string::npos) {
    return 4;
  }
  if (content.find("unlock_actions=1") == std::string::npos) {
    return 5;
  }
  if (content.find("adaptive_updates_applied=2") == std::string::npos) {
    return 6;
  }

  return 0;
}
