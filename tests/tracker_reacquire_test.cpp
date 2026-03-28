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
  config.max_miss = 3;

  Tracker tracker(config);

  Detection det;
  det.bbox = RectF {100.0f, 120.0f, 80.0f, 96.0f};
  det.score = 0.95f;

  std::vector<Detection> detections {det};
  tracker.Step(detections, static_cast<std::uint64_t>(1000));
  tracker.Step(detections, static_cast<std::uint64_t>(1033));
  tracker.Step(detections, static_cast<std::uint64_t>(1066));

  if (tracker.GetTracks().size() != 1U) {
    return 1;
  }

  const int original_id = tracker.GetTracks()[0].track_id;
  if (tracker.GetTracks()[0].state != TrackState::kConfirmed) {
    return 2;
  }

  tracker.Step({}, static_cast<std::uint64_t>(1099));
  if (tracker.GetTracks().size() != 1U ||
      tracker.GetTracks()[0].state != TrackState::kLost) {
    return 3;
  }

  Detection reacquire = det;
  reacquire.bbox = RectF {104.0f, 122.0f, 80.0f, 96.0f};
  tracker.Step(std::vector<Detection> {reacquire}, static_cast<std::uint64_t>(1132));

  if (tracker.GetTracks().size() != 1U) {
    return 4;
  }
  if (tracker.GetTracks()[0].track_id != original_id) {
    return 5;
  }
  if (tracker.GetTracks()[0].state != TrackState::kConfirmed) {
    return 6;
  }

  return 0;
}
