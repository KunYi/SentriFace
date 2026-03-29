#ifndef SENTRIFACE_ALIGN_FACE_ALIGNER_HPP_
#define SENTRIFACE_ALIGN_FACE_ALIGNER_HPP_

#include <array>
#include <cstdint>

#include "camera/frame_source.hpp"
#include "tracker/types.hpp"

namespace sentriface::align {

struct AlignmentConfig {
  int output_width = 112;
  int output_height = 112;
  std::array<sentriface::tracker::Point2f, 5> template_landmarks {{
      {38.2946f, 51.6963f},
      {73.5318f, 51.5014f},
      {56.0252f, 71.7366f},
      {41.5493f, 92.3655f},
      {70.7299f, 92.2041f},
  }};
};

struct AffineTransform2D {
  std::array<float, 6> m {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
};

struct AlignmentResult {
  bool ok = false;
  AffineTransform2D image_to_aligned {};
  int output_width = 0;
  int output_height = 0;
};

class FaceAligner {
 public:
  explicit FaceAligner(const AlignmentConfig& config = AlignmentConfig {});

  AlignmentResult Estimate(
      const sentriface::tracker::Landmark5& landmarks) const;

  sentriface::camera::Frame Warp(
      const sentriface::camera::Frame& image,
      const AlignmentResult& alignment) const;

  sentriface::camera::Frame Align(
      const sentriface::camera::Frame& image,
      const sentriface::tracker::Landmark5& landmarks) const;

 private:
  AlignmentConfig config_;
};

}  // namespace sentriface::align

#endif  // SENTRIFACE_ALIGN_FACE_ALIGNER_HPP_
