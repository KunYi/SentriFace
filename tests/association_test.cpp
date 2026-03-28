#include <vector>

#include "tracker/association.hpp"
#include "tracker/detection.hpp"
#include "tracker/track.hpp"
#include "tracker/tracker_config.hpp"
#include "tracker/types.hpp"

int main() {
  using sentriface::tracker::Association;
  using sentriface::tracker::Detection;
  using sentriface::tracker::RectF;
  using sentriface::tracker::Track;
  using sentriface::tracker::TrackState;
  using sentriface::tracker::TrackerConfig;

  TrackerConfig config;
  Association assoc(config);

  Track t0;
  t0.track_id = 0;
  t0.state = TrackState::kConfirmed;
  t0.bbox_pred = RectF {100.0f, 100.0f, 80.0f, 96.0f};

  Track t1;
  t1.track_id = 1;
  t1.state = TrackState::kConfirmed;
  t1.bbox_pred = RectF {300.0f, 100.0f, 80.0f, 96.0f};

  Detection d0;
  d0.bbox = RectF {104.0f, 102.0f, 79.0f, 95.0f};
  d0.score = 0.95f;

  Detection d1;
  d1.bbox = RectF {296.0f, 103.0f, 82.0f, 94.0f};
  d1.score = 0.94f;

  std::vector<Track> tracks {t0, t1};
  std::vector<Detection> detections {d0, d1};

  const auto candidates = assoc.BuildCandidates(tracks, detections);
  if (candidates.size() < 2) {
    return 1;
  }

  bool saw_t0_d0 = false;
  bool saw_t1_d1 = false;
  for (const auto& c : candidates) {
    if (c.track_index == 0 && c.detection_index == 0) {
      saw_t0_d0 = true;
    }
    if (c.track_index == 1 && c.detection_index == 1) {
      saw_t1_d1 = true;
    }
  }

  if (!saw_t0_d0 || !saw_t1_d1) {
    return 2;
  }

  Detection far_det;
  far_det.bbox = RectF {600.0f, 600.0f, 80.0f, 96.0f};
  far_det.score = 0.99f;

  Detection huge_det;
  huge_det.bbox = RectF {100.0f, 100.0f, 400.0f, 400.0f};
  huge_det.score = 0.99f;

  const auto rejected_far = assoc.BuildCandidates(tracks, std::vector<Detection> {far_det});
  if (!rejected_far.empty()) {
    return 3;
  }

  const auto rejected_scale = assoc.BuildCandidates(tracks, std::vector<Detection> {huge_det});
  if (!rejected_scale.empty()) {
    return 4;
  }

  return 0;
}
