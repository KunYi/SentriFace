#include <cmath>

#include "align/face_aligner.hpp"
#include "camera/frame_source.hpp"
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

  sentriface::camera::Frame image;
  image.width = 112;
  image.height = 112;
  image.channels = 3;
  image.pixel_format = sentriface::camera::PixelFormat::kRgb888;
  image.data.resize(static_cast<std::size_t>(112 * 112 * 3), 0u);
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const std::size_t index =
          static_cast<std::size_t>((y * image.width + x) * image.channels);
      image.data[index + 0] = static_cast<std::uint8_t>(x & 0xff);
      image.data[index + 1] = static_cast<std::uint8_t>(y & 0xff);
      image.data[index + 2] = static_cast<std::uint8_t>((x + y) & 0xff);
    }
  }

  const auto warped = aligner.Warp(image, identity_like);
  if (warped.width != 112 || warped.height != 112 || warped.data.empty()) {
    return 6;
  }
  const std::size_t center_index =
      static_cast<std::size_t>((56 * warped.width + 56) * warped.channels);
  if (std::abs(static_cast<int>(warped.data[center_index + 0]) - 56) > 1 ||
      std::abs(static_cast<int>(warped.data[center_index + 1]) - 56) > 1) {
    return 7;
  }

  const auto aligned = aligner.Align(image, same_as_template);
  if (aligned.data.empty()) {
    return 8;
  }

  return 0;
}
