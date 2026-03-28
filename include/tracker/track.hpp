#ifndef SENTRIFACE_TRACKER_TRACK_HPP_
#define SENTRIFACE_TRACKER_TRACK_HPP_

#include <array>
#include <cstdint>

#include "tracker/types.hpp"

namespace sentriface::tracker {

struct Track {
  int track_id = -1;
  TrackState state = TrackState::kTentative;

  int age = 0;
  int hits = 0;
  int miss = 0;

  std::uint64_t last_timestamp_ms = 0;
  std::uint64_t last_embedding_timestamp_ms = 0;

  std::array<float, 8> x {};
  std::array<float, 64> p {};

  RectF bbox_latest {};
  RectF bbox_pred {};
  Landmark5 landmarks_latest {};
  float last_detection_score = 0.0f;

  float face_quality = 0.0f;
  float best_face_quality = 0.0f;

  int identity_id = -1;
  float identity_score = 0.0f;
};

}  // namespace sentriface::tracker

#endif  // SENTRIFACE_TRACKER_TRACK_HPP_
