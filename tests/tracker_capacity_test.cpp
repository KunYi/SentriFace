#include <cstdint>
#include <vector>

#include "tracker/detection.hpp"
#include "tracker/tracker.hpp"
#include "tracker/tracker_config.hpp"

int main() {
  using sentriface::tracker::Detection;
  using sentriface::tracker::RectF;
  using sentriface::tracker::Tracker;
  using sentriface::tracker::TrackerConfig;

  TrackerConfig config;
  config.max_tracks = 3;
  config.min_confirm_hits = 2;

  Tracker tracker(config);

  std::vector<Detection> detections;
  for (int i = 0; i < 5; ++i) {
    Detection det;
    det.bbox = RectF {50.0f + 110.0f * static_cast<float>(i), 120.0f, 80.0f, 96.0f};
    det.score = 0.95f;
    detections.push_back(det);
  }

  tracker.Step(detections, static_cast<std::uint64_t>(1000));
  if (tracker.GetTracks().size() != 3U) {
    return 1;
  }

  tracker.Step(detections, static_cast<std::uint64_t>(1033));
  if (tracker.GetTracks().size() != 3U) {
    return 2;
  }

  return 0;
}
