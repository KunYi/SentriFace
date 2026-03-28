#ifndef SENTRIFACE_APP_PIPELINE_REPORT_HPP_
#define SENTRIFACE_APP_PIPELINE_REPORT_HPP_

#include <string>

namespace sentriface::app {

struct PipelineRunStats {
  int frames_processed = 0;
  int detections_processed = 0;
  int tracks_seen = 0;
  int embedding_triggers = 0;
  int unlock_ready_tracks = 0;
  int unlock_actions = 0;
  int short_gap_merges = 0;
  int adaptive_updates_applied = 0;
};

bool WritePipelineReport(const std::string& path, const PipelineRunStats& stats);

}  // namespace sentriface::app

#endif  // SENTRIFACE_APP_PIPELINE_REPORT_HPP_
