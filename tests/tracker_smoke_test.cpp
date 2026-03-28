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
  Tracker tracker(config);

  std::vector<Detection> detections;
  Detection det;
  det.bbox = RectF {100.0f, 120.0f, 80.0f, 96.0f};
  det.score = 0.95f;
  det.timestamp_ms = 1000;
  detections.push_back(det);

  tracker.Step(detections, static_cast<std::uint64_t>(1000));
  tracker.Step(detections, static_cast<std::uint64_t>(1033));
  tracker.Step(detections, static_cast<std::uint64_t>(1066));

  const auto& tracks = tracker.GetTracks();
  if (tracks.empty()) {
    return 1;
  }

  return 0;
}
