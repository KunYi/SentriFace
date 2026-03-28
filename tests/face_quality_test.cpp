#include <cmath>

#include "tracker/detection.hpp"
#include "tracker/face_quality.hpp"
#include "tracker/tracker_config.hpp"

int main() {
  using sentriface::tracker::Detection;
  using sentriface::tracker::FaceQualityEvaluator;
  using sentriface::tracker::RectF;
  using sentriface::tracker::TrackerConfig;

  TrackerConfig config;
  config.det_score_thresh = 0.55f;
  config.embed_min_face_width = 90.0f;
  config.embed_min_face_height = 90.0f;

  FaceQualityEvaluator evaluator(config);

  Detection weak;
  weak.bbox = RectF {0.0f, 0.0f, 60.0f, 70.0f};
  weak.score = 0.60f;

  Detection strong;
  strong.bbox = RectF {0.0f, 0.0f, 110.0f, 120.0f};
  strong.score = 0.95f;

  const float weak_score = evaluator.Evaluate(weak);
  const float strong_score = evaluator.Evaluate(strong);

  if (!(weak_score >= 0.0f && weak_score <= 1.0f)) {
    return 1;
  }
  if (!(strong_score >= 0.0f && strong_score <= 1.0f)) {
    return 2;
  }
  if (!(strong_score > weak_score)) {
    return 3;
  }

  return 0;
}
