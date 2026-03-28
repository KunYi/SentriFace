#include <cstdint>
#include <vector>

#include "camera/frame_source.hpp"
#include "detector/face_detector.hpp"

int main() {
  using sentriface::camera::Frame;
  using sentriface::camera::PixelFormat;
  using sentriface::detector::DetectionBatch;
  using sentriface::detector::DetectionConfig;
  using sentriface::detector::SequenceFaceDetector;
  using sentriface::tracker::Detection;
  using sentriface::tracker::RectF;

  DetectionConfig config;
  config.input_width = 640;
  config.input_height = 640;
  config.input_channels = 3;
  config.score_threshold = 0.60f;
  config.min_face_width = 64.0f;
  config.min_face_height = 64.0f;
  config.max_output_detections = 1;
  config.enable_roi = true;
  config.roi = RectF {0.0f, 0.0f, 260.0f, 260.0f};

  Detection keep;
  keep.bbox = RectF {100.0f, 120.0f, 80.0f, 96.0f};
  keep.score = 0.95f;

  Detection keep_large;
  keep_large.bbox = RectF {80.0f, 100.0f, 120.0f, 132.0f};
  keep_large.score = 0.80f;

  Detection drop;
  drop.bbox = RectF {200.0f, 120.0f, 80.0f, 96.0f};
  drop.score = 0.50f;

  Detection drop_small;
  drop_small.bbox = RectF {300.0f, 140.0f, 40.0f, 48.0f};
  drop_small.score = 0.90f;

  Detection drop_outside_roi;
  drop_outside_roi.bbox = RectF {360.0f, 80.0f, 180.0f, 180.0f};
  drop_outside_roi.score = 0.99f;

  DetectionBatch batch;
  batch.timestamp_ms = static_cast<std::uint64_t>(1000);
  batch.detections = {keep, keep_large, drop, drop_small, drop_outside_roi};

  SequenceFaceDetector detector(std::vector<DetectionBatch> {batch}, config);

  Frame frame;
  frame.width = 640;
  frame.height = 640;
  frame.channels = 3;
  frame.pixel_format = PixelFormat::kRgb888;
  frame.timestamp_ms = static_cast<std::uint64_t>(1234);

  if (!detector.ValidateInputFrame(frame)) {
    return 1;
  }

  const auto info = detector.GetInfo();
  if (!info.is_mock || info.name.empty()) {
    return 2;
  }

  const auto result = detector.Detect(frame);
  if (result.timestamp_ms != frame.timestamp_ms) {
    return 3;
  }
  if (result.detections.size() != 1U) {
    return 4;
  }
  if (result.detections[0].bbox.w != 120.0f ||
      result.detections[0].bbox.h != 132.0f) {
    return 5;
  }

  Frame invalid = frame;
  invalid.width = 320;
  const auto invalid_result = detector.Detect(invalid);
  if (!invalid_result.detections.empty()) {
    return 6;
  }

  return 0;
}
