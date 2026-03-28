#include "app/pipeline_report.hpp"

#include <fstream>

namespace sentriface::app {

bool WritePipelineReport(const std::string& path, const PipelineRunStats& stats) {
  std::ofstream out(path);
  if (!out.is_open()) {
    return false;
  }

  out << "frames_processed=" << stats.frames_processed << '\n';
  out << "detections_processed=" << stats.detections_processed << '\n';
  out << "tracks_seen=" << stats.tracks_seen << '\n';
  out << "embedding_triggers=" << stats.embedding_triggers << '\n';
  out << "unlock_ready_tracks=" << stats.unlock_ready_tracks << '\n';
  out << "unlock_actions=" << stats.unlock_actions << '\n';
  out << "short_gap_merges=" << stats.short_gap_merges << '\n';
  out << "adaptive_updates_applied=" << stats.adaptive_updates_applied << '\n';
  return true;
}

}  // namespace sentriface::app
