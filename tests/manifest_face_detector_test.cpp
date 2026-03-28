#include <cstdint>
#include <fstream>
#include <string>

#include "camera/frame_source.hpp"
#include "detector/face_detector.hpp"

int main() {
  using sentriface::camera::Frame;
  using sentriface::camera::PixelFormat;
  using sentriface::detector::DetectionConfig;
  using sentriface::detector::ManifestFaceDetector;

  const std::string manifest_path = "manifest_face_detector_test.txt";
  {
    std::ofstream out(manifest_path);
    out << "1000 100 120 80 96 0.95 120 140 156 140 138 160 124 184 152 184\n";
    out << "1000 60 80 120 132 0.80 90 110 150 110 120 140 100 180 150 180\n";
    out << "1000 360 80 180 180 0.99 390 110 480 110 435 160 410 220 470 220\n";
    out << "1000 220 120 80 96 0.50 240 140 276 140 258 160 244 184 272 184\n";
    out << "1000 320 140 40 48 0.92 332 152 348 152 340 164 336 180 352 180\n";
    out << "1033 104 121 80 96 0.97 124 141 160 141 142 161 128 185 156 185\n";
  }

  DetectionConfig config;
  config.input_width = 640;
  config.input_height = 640;
  config.input_channels = 3;
  config.score_threshold = 0.60f;
  config.min_face_width = 64.0f;
  config.min_face_height = 64.0f;
  config.max_output_detections = 1;
  config.enable_roi = true;
  config.roi = sentriface::tracker::RectF {0.0f, 0.0f, 260.0f, 260.0f};

  ManifestFaceDetector detector(manifest_path, config);
  if (!detector.IsLoaded()) {
    return 1;
  }

  Frame frame;
  frame.width = 640;
  frame.height = 640;
  frame.channels = 3;
  frame.pixel_format = PixelFormat::kRgb888;
  frame.timestamp_ms = static_cast<std::uint64_t>(1000);

  const auto batch0 = detector.Detect(frame);
  if (batch0.detections.size() != 1U) {
    return 2;
  }
  if (batch0.detections[0].timestamp_ms != static_cast<std::uint64_t>(1000)) {
    return 3;
  }
  if (batch0.detections[0].bbox.w != 120.0f ||
      batch0.detections[0].bbox.h != 132.0f) {
    return 6;
  }

  frame.timestamp_ms = static_cast<std::uint64_t>(1016);
  const auto batch1 = detector.Detect(frame);
  if (!batch1.detections.empty()) {
    return 4;
  }

  frame.timestamp_ms = static_cast<std::uint64_t>(1033);
  const auto batch2 = detector.Detect(frame);
  if (batch2.detections.size() != 1U) {
    return 5;
  }

  return 0;
}
