#include <cmath>

#include "align/face_aligner.hpp"
#include "tracker/types.hpp"

namespace {

bool AlmostEqual(float a, float b, float eps = 1e-2f) {
  return std::fabs(a - b) <= eps;
}

}  // namespace

int main() {
  using sentriface::align::FaceAligner;
  using sentriface::tracker::Landmark5;
  using sentriface::tracker::Point2f;

  FaceAligner aligner;

  Landmark5 same_as_template;
  same_as_template.points = {{
      {38.2946f, 51.6963f},
      {73.5318f, 51.5014f},
      {56.0252f, 71.7366f},
      {41.5493f, 92.3655f},
      {70.7299f, 92.2041f},
  }};

  const auto identity_like = aligner.Estimate(same_as_template);
  if (!identity_like.ok) {
    return 1;
  }
  if (!AlmostEqual(identity_like.image_to_aligned.m[0], 1.0f) ||
      !AlmostEqual(identity_like.image_to_aligned.m[1], 0.0f) ||
      !AlmostEqual(identity_like.image_to_aligned.m[3], 0.0f) ||
      !AlmostEqual(identity_like.image_to_aligned.m[4], 1.0f)) {
    return 2;
  }

  Landmark5 translated = same_as_template;
  for (auto& p : translated.points) {
    p.x += 10.0f;
    p.y += 20.0f;
  }

  const auto translated_result = aligner.Estimate(translated);
  if (!translated_result.ok) {
    return 3;
  }
  if (!(translated_result.image_to_aligned.m[2] < 0.0f &&
        translated_result.image_to_aligned.m[5] < 0.0f)) {
    return 4;
  }

  Landmark5 degenerate;
  degenerate.points = {{
      {10.0f, 10.0f},
      {10.0f, 10.0f},
      {10.0f, 10.0f},
      {10.0f, 10.0f},
      {10.0f, 10.0f},
  }};

  const auto invalid = aligner.Estimate(degenerate);
  if (invalid.ok) {
    return 5;
  }

  return 0;
}
