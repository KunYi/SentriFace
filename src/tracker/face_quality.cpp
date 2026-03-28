#include "tracker/face_quality.hpp"

#include <algorithm>

namespace sentriface::tracker {
namespace {

float Clamp01(float value) {
  return std::max(0.0f, std::min(1.0f, value));
}

}  // namespace

FaceQualityEvaluator::FaceQualityEvaluator(const TrackerConfig& config)
    : config_(config) {}

float FaceQualityEvaluator::Evaluate(const Detection& detection) const {
  const float score_span = std::max(1e-6f, 1.0f - config_.det_score_thresh);
  const float det_score_norm =
      Clamp01((detection.score - config_.det_score_thresh) / score_span);

  const float width_ratio = detection.bbox.w /
                            std::max(1.0f, config_.embed_min_face_width);
  const float height_ratio = detection.bbox.h /
                             std::max(1.0f, config_.embed_min_face_height);
  const float size_score = Clamp01(std::min(width_ratio, height_ratio));

  return Clamp01(0.6f * det_score_norm + 0.4f * size_score);
}

}  // namespace sentriface::tracker
