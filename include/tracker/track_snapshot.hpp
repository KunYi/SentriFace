#ifndef SENTRIFACE_TRACKER_TRACK_SNAPSHOT_HPP_
#define SENTRIFACE_TRACKER_TRACK_SNAPSHOT_HPP_

#include <cstdint>

#include "tracker/types.hpp"

namespace sentriface::tracker {

struct TrackSnapshot {
  int track_id = -1;
  TrackState state = TrackState::kTentative;

  RectF bbox {};
  Landmark5 landmarks {};

  int age = 0;
  int hits = 0;
  int miss = 0;

  float last_detection_score = 0.0f;
  float face_quality = 0.0f;
  float best_face_quality = 0.0f;

  std::uint64_t last_timestamp_ms = 0;
  std::uint64_t last_embedding_timestamp_ms = 0;
};

}  // namespace sentriface::tracker

#endif  // SENTRIFACE_TRACKER_TRACK_SNAPSHOT_HPP_
