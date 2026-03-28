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
  config.embed_det_score_thresh = 0.85f;
  config.embed_min_face_width = 90.0f;
  config.embed_min_face_height = 90.0f;
  config.embed_min_face_quality = 0.70f;

  Tracker tracker(config);

  Detection small_face;
  small_face.bbox = RectF {100.0f, 120.0f, 70.0f, 88.0f};
  small_face.score = 0.95f;

  tracker.Step(std::vector<Detection> {small_face}, static_cast<std::uint64_t>(1000));
  tracker.Step(std::vector<Detection> {small_face}, static_cast<std::uint64_t>(1033));
  if (tracker.GetConfirmedTracks().size() != 1U) {
    return 1;
  }
  if (!tracker.GetEmbeddingCandidates().empty()) {
    return 2;
  }

  tracker.Reset();

  Detection low_score;
  low_score.bbox = RectF {100.0f, 120.0f, 96.0f, 104.0f};
  low_score.score = 0.80f;
  tracker.Step(std::vector<Detection> {low_score}, static_cast<std::uint64_t>(2000));
  tracker.Step(std::vector<Detection> {low_score}, static_cast<std::uint64_t>(2033));
  if (tracker.GetConfirmedTracks().size() != 1U) {
    return 3;
  }
  if (!tracker.GetEmbeddingCandidates().empty()) {
    return 4;
  }

  tracker.Reset();

  Detection good_face;
  good_face.bbox = RectF {100.0f, 120.0f, 96.0f, 104.0f};
  good_face.score = 0.93f;
  tracker.Step(std::vector<Detection> {good_face}, static_cast<std::uint64_t>(3000));
  tracker.Step(std::vector<Detection> {good_face}, static_cast<std::uint64_t>(3033));
  if (tracker.GetEmbeddingCandidates().size() != 1U) {
    return 5;
  }

  return 0;
}
