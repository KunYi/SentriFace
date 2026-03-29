#include "align/face_aligner.hpp"

#include <cmath>
#include <cstddef>

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

bool InvertAffine(const AffineTransform2D& transform,
                  std::array<float, 6>* out_inverse) {
  if (out_inverse == nullptr) {
    return false;
  }
  const float a = transform.m[0];
  const float b = transform.m[1];
  const float c = transform.m[2];
  const float d = transform.m[3];
  const float e = transform.m[4];
  const float f = transform.m[5];
  const float det = a * e - b * d;
  if (std::fabs(det) < 1e-9f) {
    return false;
  }
  const float inv_det = 1.0f / det;
  (*out_inverse)[0] = e * inv_det;
  (*out_inverse)[1] = -b * inv_det;
  (*out_inverse)[2] = (b * f - e * c) * inv_det;
  (*out_inverse)[3] = -d * inv_det;
  (*out_inverse)[4] = a * inv_det;
  (*out_inverse)[5] = (d * c - a * f) * inv_det;
  return true;
}

float SampleChannel(const sentriface::camera::Frame& image,
                    float x,
                    float y,
                    int channel) {
  if (image.width <= 0 || image.height <= 0 || image.channels <= 0 ||
      image.data.empty() || channel < 0 || channel >= image.channels) {
    return 0.0f;
  }

  const float clamped_x = std::max(0.0f, std::min(x, image.width - 1.0f));
  const float clamped_y = std::max(0.0f, std::min(y, image.height - 1.0f));
  const int x0 = static_cast<int>(std::floor(clamped_x));
  const int y0 = static_cast<int>(std::floor(clamped_y));
  const int x1 = std::min(x0 + 1, image.width - 1);
  const int y1 = std::min(y0 + 1, image.height - 1);
  const float wx = clamped_x - x0;
  const float wy = clamped_y - y0;

  auto read = [&](int px, int py) -> float {
    const std::size_t index =
        static_cast<std::size_t>((py * image.width + px) * image.channels + channel);
    return static_cast<float>(image.data[index]);
  };

  const float v00 = read(x0, y0);
  const float v10 = read(x1, y0);
  const float v01 = read(x0, y1);
  const float v11 = read(x1, y1);

  const float top = v00 * (1.0f - wx) + v10 * wx;
  const float bottom = v01 * (1.0f - wx) + v11 * wx;
  return top * (1.0f - wy) + bottom * wy;
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

sentriface::camera::Frame FaceAligner::Warp(
    const sentriface::camera::Frame& image,
    const AlignmentResult& alignment) const {
  sentriface::camera::Frame aligned;
  aligned.width = alignment.output_width;
  aligned.height = alignment.output_height;
  aligned.channels = image.channels;
  aligned.pixel_format = image.pixel_format;
  aligned.timestamp_ms = image.timestamp_ms;
  if (!alignment.ok || image.width <= 0 || image.height <= 0 ||
      image.channels <= 0 || image.data.empty() ||
      alignment.output_width <= 0 || alignment.output_height <= 0) {
    return aligned;
  }

  std::array<float, 6> inverse {};
  if (!InvertAffine(alignment.image_to_aligned, &inverse)) {
    return aligned;
  }

  aligned.data.resize(static_cast<std::size_t>(
                          alignment.output_width * alignment.output_height *
                          image.channels),
                      0u);
  for (int y = 0; y < alignment.output_height; ++y) {
    for (int x = 0; x < alignment.output_width; ++x) {
      const float src_x =
          inverse[0] * (x + 0.5f) + inverse[1] * (y + 0.5f) + inverse[2] - 0.5f;
      const float src_y =
          inverse[3] * (x + 0.5f) + inverse[4] * (y + 0.5f) + inverse[5] - 0.5f;
      for (int channel = 0; channel < image.channels; ++channel) {
        const float value = SampleChannel(image, src_x, src_y, channel);
        const std::size_t index = static_cast<std::size_t>(
            (y * alignment.output_width + x) * image.channels + channel);
        aligned.data[index] = static_cast<std::uint8_t>(
            std::max(0.0f, std::min(255.0f, value + 0.5f)));
      }
    }
  }
  return aligned;
}

sentriface::camera::Frame FaceAligner::Align(
    const sentriface::camera::Frame& image,
    const sentriface::tracker::Landmark5& landmarks) const {
  return Warp(image, Estimate(landmarks));
}

}  // namespace sentriface::align
