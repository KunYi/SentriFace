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
  config.min_confirm_hits = 2;
  config.max_miss = 2;
  Tracker tracker(config);

  Detection left_1;
  left_1.bbox = RectF {100.0f, 120.0f, 80.0f, 96.0f};
  left_1.score = 0.95f;

  Detection right_1;
  right_1.bbox = RectF {320.0f, 120.0f, 80.0f, 96.0f};
  right_1.score = 0.96f;

  tracker.Step(std::vector<Detection> {left_1, right_1}, static_cast<std::uint64_t>(1000));
  tracker.Step(std::vector<Detection> {left_1, right_1}, static_cast<std::uint64_t>(1033));

  if (tracker.GetTracks().size() != 2U) {
    return 1;
  }

  const int id_a = tracker.GetTracks()[0].track_id;
  const int id_b = tracker.GetTracks()[1].track_id;
  if (id_a == id_b) {
    return 2;
  }

  Detection left_2 = left_1;
  left_2.bbox = RectF {106.0f, 121.0f, 80.0f, 96.0f};
  Detection right_2 = right_1;
  right_2.bbox = RectF {314.0f, 119.0f, 80.0f, 96.0f};

  tracker.Step(std::vector<Detection> {left_2, right_2}, static_cast<std::uint64_t>(1066));

  if (tracker.GetTracks().size() != 2U) {
    return 3;
  }

  bool saw_original_a = false;
  bool saw_original_b = false;
  for (const auto& track : tracker.GetTracks()) {
    if (track.track_id == id_a) {
      saw_original_a = true;
    }
    if (track.track_id == id_b) {
      saw_original_b = true;
    }
  }

  if (!saw_original_a || !saw_original_b) {
    return 4;
  }

  return 0;
}
