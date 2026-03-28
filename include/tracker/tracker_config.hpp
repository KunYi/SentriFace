#ifndef SENTRIFACE_TRACKER_TRACKER_CONFIG_HPP_
#define SENTRIFACE_TRACKER_TRACKER_CONFIG_HPP_

#include <cstdint>

namespace sentriface::tracker {

struct TrackerConfig {
  float det_score_thresh = 0.55f;
  float min_face_width = 50.0f;
  float min_face_height = 50.0f;

  float iou_min = 0.20f;
  float center_gate_ratio = 0.60f;
  float area_ratio_min = 0.50f;
  float area_ratio_max = 2.00f;

  int min_confirm_hits = 3;
  int max_miss = 8;
  int max_tracks = 5;

  float embed_det_score_thresh = 0.75f;
  float embed_min_face_width = 80.0f;
  float embed_min_face_height = 80.0f;
  float embed_min_face_quality = 0.0f;
  std::uint64_t embed_interval_ms = 500;

  float q_pos = 4.0f;
  float q_size = 4.0f;
  float q_vel = 9.0f;
  float q_vsize = 9.0f;

  float r_pos = 16.0f;
  float r_size = 25.0f;
};

}  // namespace sentriface::tracker

#endif  // SENTRIFACE_TRACKER_TRACKER_CONFIG_HPP_
