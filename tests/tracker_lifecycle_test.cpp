#include <cstdint>
#include <vector>

#include "tracker/detection.hpp"
#include "tracker/tracker.hpp"
#include "tracker/tracker_config.hpp"
#include "tracker/types.hpp"

int main() {
  using sentriface::tracker::Detection;
  using sentriface::tracker::RectF;
  using sentriface::tracker::TrackState;
  using sentriface::tracker::Tracker;
  using sentriface::tracker::TrackerConfig;

  TrackerConfig config;
  config.min_confirm_hits = 3;
  config.max_miss = 2;

  Tracker tracker(config);

  Detection det;
  det.bbox = RectF {100.0f, 120.0f, 80.0f, 96.0f};
  det.score = 0.95f;

  std::vector<Detection> detections {det};
  tracker.Step(detections, static_cast<std::uint64_t>(1000));
  tracker.Step(detections, static_cast<std::uint64_t>(1033));

  if (tracker.GetTracks().empty()) {
    return 1;
  }
  if (tracker.GetTracks()[0].state != TrackState::kTentative) {
    return 2;
  }

  tracker.Step(detections, static_cast<std::uint64_t>(1066));
  if (tracker.GetTracks()[0].state != TrackState::kConfirmed) {
    return 3;
  }

  tracker.Step({}, static_cast<std::uint64_t>(1099));
  if (tracker.GetTracks().empty()) {
    return 4;
  }
  if (tracker.GetTracks()[0].state != TrackState::kLost) {
    return 5;
  }

  tracker.Step({}, static_cast<std::uint64_t>(1132));
  if (tracker.GetTracks()[0].state != TrackState::kLost) {
    return 6;
  }

  tracker.Step({}, static_cast<std::uint64_t>(1165));
  if (!tracker.GetTracks().empty()) {
    return 7;
  }

  return 0;
}
