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
  config.embed_det_score_thresh = 0.80f;
  config.embed_min_face_width = 90.0f;
  config.embed_min_face_height = 90.0f;

  Tracker tracker(config);

  Detection det;
  det.bbox = RectF {100.0f, 120.0f, 80.0f, 96.0f};
  det.score = 0.95f;

  const std::vector<Detection> detections {det};
  tracker.Step(detections, static_cast<std::uint64_t>(1000));
  tracker.Step(detections, static_cast<std::uint64_t>(1033));

  if (!tracker.GetConfirmedTracks().empty()) {
    return 1;
  }
  if (!tracker.GetEmbeddingCandidates().empty()) {
    return 2;
  }

  tracker.Step(detections, static_cast<std::uint64_t>(1066));

  const auto confirmed = tracker.GetConfirmedTracks();
  const auto embeddable = tracker.GetEmbeddingCandidates();
  if (confirmed.size() != 1U) {
    return 3;
  }
  if (embeddable.size() != 0U) {
    return 4;
  }
  if (confirmed[0].state != TrackState::kConfirmed) {
    return 5;
  }

  Detection large_det = det;
  large_det.bbox = RectF {100.0f, 120.0f, 96.0f, 104.0f};
  large_det.score = 0.92f;
  tracker.Step(std::vector<Detection> {large_det}, static_cast<std::uint64_t>(1099));

  const auto embeddable_after_large = tracker.GetEmbeddingCandidates();
  if (embeddable_after_large.size() != 1U) {
    return 6;
  }
  if (embeddable_after_large[0].state != TrackState::kConfirmed) {
    return 7;
  }

  tracker.Step({}, static_cast<std::uint64_t>(1132));
  if (tracker.GetConfirmedTracks().size() != 0U) {
    return 8;
  }
  if (tracker.GetEmbeddingCandidates().size() != 0U) {
    return 9;
  }

  return 0;
}
