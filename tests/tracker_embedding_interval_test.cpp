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
  config.embed_det_score_thresh = 0.80f;
  config.embed_min_face_width = 90.0f;
  config.embed_min_face_height = 90.0f;
  config.embed_interval_ms = 500;

  Tracker tracker(config);

  Detection det;
  det.bbox = RectF {100.0f, 120.0f, 96.0f, 104.0f};
  det.score = 0.95f;

  tracker.Step(std::vector<Detection> {det}, static_cast<std::uint64_t>(1000));
  tracker.Step(std::vector<Detection> {det}, static_cast<std::uint64_t>(1033));

  const auto first_candidates =
      tracker.GetEmbeddingTriggerCandidates(static_cast<std::uint64_t>(1033));
  if (first_candidates.size() != 1U) {
    return 1;
  }

  if (!tracker.MarkEmbeddingTriggered(first_candidates[0].track_id,
                                      static_cast<std::uint64_t>(1033))) {
    return 2;
  }

  const auto blocked_candidates =
      tracker.GetEmbeddingTriggerCandidates(static_cast<std::uint64_t>(1200));
  if (!blocked_candidates.empty()) {
    return 3;
  }

  const auto allowed_again =
      tracker.GetEmbeddingTriggerCandidates(static_cast<std::uint64_t>(1600));
  if (allowed_again.size() != 1U) {
    return 4;
  }

  return 0;
}
