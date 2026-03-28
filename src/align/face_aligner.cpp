#include "align/face_aligner.hpp"

#include <cmath>

namespace sentriface::align {
namespace {

using sentriface::tracker::Point2f;

Point2f MeanPoint(const std::array<Point2f, 5>& points) {
  Point2f mean {};
  for (const auto& p : points) {
    mean.x += p.x;
    mean.y += p.y;
  }
  mean.x /= 5.0f;
  mean.y /= 5.0f;
  return mean;
}

float SumSquaredNorm(const std::array<Point2f, 5>& points, const Point2f& mean) {
  float sum = 0.0f;
  for (const auto& p : points) {
    const float dx = p.x - mean.x;
    const float dy = p.y - mean.y;
    sum += dx * dx + dy * dy;
  }
  return sum;
}

}  // namespace

FaceAligner::FaceAligner(const AlignmentConfig& config) : config_(config) {}

AlignmentResult FaceAligner::Estimate(
    const sentriface::tracker::Landmark5& landmarks) const {
  AlignmentResult result;
  result.output_width = config_.output_width;
  result.output_height = config_.output_height;

  const auto& src = landmarks.points;
  const auto& dst = config_.template_landmarks;

  const Point2f src_mean = MeanPoint(src);
  const Point2f dst_mean = MeanPoint(dst);

  const float src_norm = SumSquaredNorm(src, src_mean);
  if (src_norm < 1e-6f) {
    return result;
  }

  float a_num = 0.0f;
  float b_num = 0.0f;
  for (int i = 0; i < 5; ++i) {
    const float sx = src[i].x - src_mean.x;
    const float sy = src[i].y - src_mean.y;
    const float dx = dst[i].x - dst_mean.x;
    const float dy = dst[i].y - dst_mean.y;

    a_num += sx * dx + sy * dy;
    b_num += sx * dy - sy * dx;
  }

  const float a = a_num / src_norm;
  const float b = b_num / src_norm;
  const float tx = dst_mean.x - a * src_mean.x + b * src_mean.y;
  const float ty = dst_mean.y - b * src_mean.x - a * src_mean.y;

  result.ok = true;
  result.image_to_aligned.m = {
      a, -b, tx,
      b,  a, ty,
  };
  return result;
}

}  // namespace sentriface::align
