#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "camera/frame_source.hpp"
#include "detector/face_detector.hpp"
#include "detector/scrfd_runtime.hpp"

namespace {

class FakeScrfdRuntime final : public sentriface::detector::IScrfdRuntime {
 public:
  explicit FakeScrfdRuntime(std::vector<sentriface::detector::ScrfdRawCandidate> candidates)
      : candidates_(std::move(candidates)) {}

  bool Initialize(const sentriface::detector::ScrfdDetectorConfig&) override {
    ready_ = true;
    return true;
  }

  bool IsReady() const override { return ready_; }

  bool Run(const sentriface::detector::ScrfdInputTensor& input,
           std::vector<sentriface::detector::ScrfdRawCandidate>& candidates) override {
    if (!ready_ || input.data.empty()) {
      return false;
    }
    candidates = candidates_;
    return true;
  }

 private:
  std::vector<sentriface::detector::ScrfdRawCandidate> candidates_;
  bool ready_ = false;
};

}  // namespace

int main() {
  using sentriface::camera::Frame;
  using sentriface::camera::PixelFormat;
  using sentriface::detector::DetectorBackendType;
  using sentriface::detector::ScrfdInputTensor;
  using sentriface::detector::ScrfdInferenceOutputs;
  using sentriface::detector::ScrfdRawCandidate;
  using sentriface::detector::ScrfdDetectorConfig;
  using sentriface::detector::ScrfdFaceDetector;
  using sentriface::detector::ScrfdTensorView;
  using sentriface::detector::ScrfdRuntime;

  ScrfdDetectorConfig invalid_config;
  invalid_config.detection.input_width = 640;
  invalid_config.detection.input_height = 640;
  invalid_config.detection.input_channels = 3;
  invalid_config.model_path = "";
  ScrfdFaceDetector invalid_detector(invalid_config);
  if (invalid_detector.IsInitialized()) {
    return 1;
  }
  if (invalid_detector.GetInfo().is_ready) {
    return 2;
  }

  ScrfdDetectorConfig cpu_config;
  cpu_config.detection.input_width = 640;
  cpu_config.detection.input_height = 640;
  cpu_config.detection.input_channels = 3;
  cpu_config.runtime = ScrfdRuntime::kCpu;
#ifdef SENTRIFACE_HAS_ONNXRUNTIME
  cpu_config.model_path =
      std::string(SENTRIFACE_SOURCE_DIR) + "/third_party/models/buffalo_sc/det_500m.onnx";
#else
  cpu_config.model_path = "models/scrfd_500m.onnx";
#endif
  cpu_config.nms_threshold = 0.45f;
  cpu_config.max_detections = 16;

  ScrfdFaceDetector cpu_detector(cpu_config, "scrfd_cpu");
  if (!cpu_detector.IsInitialized()) {
    return 3;
  }
  if (cpu_detector.GetBackendType() != DetectorBackendType::kScrfdCpuStub) {
    return 4;
  }
  if (!cpu_detector.HasRuntime()) {
    return 5;
  }

  const auto cpu_info = cpu_detector.GetInfo();
  if (cpu_info.is_mock || !cpu_info.is_ready || cpu_info.name != "scrfd_cpu") {
    return 6;
  }

  Frame frame;
  frame.width = 640;
  frame.height = 640;
  frame.channels = 3;
  frame.pixel_format = PixelFormat::kRgb888;
  frame.timestamp_ms = static_cast<std::uint64_t>(1000);
  frame.data.assign(static_cast<std::size_t>(640 * 640 * 3), 0U);
  frame.data[0] = 255U;
  frame.data[1] = 128U;
  frame.data[2] = 64U;
  if (!cpu_detector.ValidateInputFrame(frame)) {
    return 7;
  }

  ScrfdInputTensor tensor;
  if (!cpu_detector.PrepareInputTensor(frame, tensor)) {
    return 8;
  }
  if (tensor.width != 640 || tensor.height != 640 || tensor.channels != 3) {
    return 9;
  }
  if (tensor.original_width != 640 || tensor.original_height != 640) {
    return 10;
  }
  if (tensor.data.size() != static_cast<std::size_t>(640 * 640 * 3)) {
    return 11;
  }
  if (tensor.data[0] != (64.0f - 127.5f) / 128.0f) {
    return 12;
  }
  if (tensor.data[640 * 640] != (128.0f - 127.5f) / 128.0f) {
    return 13;
  }
  if (tensor.data[2 * 640 * 640] != (255.0f - 127.5f) / 128.0f) {
    return 14;
  }

  Frame wide_frame;
  wide_frame.width = 1280;
  wide_frame.height = 720;
  wide_frame.channels = 3;
  wide_frame.pixel_format = PixelFormat::kRgb888;
  wide_frame.timestamp_ms = static_cast<std::uint64_t>(1001);
  wide_frame.data.assign(static_cast<std::size_t>(1280 * 720 * 3), 32U);
  if (!cpu_detector.ValidateInputFrame(wide_frame)) {
    return 15;
  }
  ScrfdInputTensor wide_tensor;
  if (!cpu_detector.PrepareInputTensor(wide_frame, wide_tensor)) {
    return 16;
  }
  if (wide_tensor.width != 640 || wide_tensor.height != 640 ||
      wide_tensor.original_width != 1280 || wide_tensor.original_height != 720) {
    return 17;
  }
  if (wide_tensor.resized_width != 640 || wide_tensor.resized_height != 360) {
    return 18;
  }
  if (wide_tensor.pad_x != 0.0f || wide_tensor.pad_y != 140.0f) {
    return 19;
  }

  const auto locations = cpu_detector.ComputeExpectedLocations(640, 640);
  if (locations.size() != 3U) {
    return 20;
  }
  if (locations[0] != 12800 || locations[1] != 3200 || locations[2] != 800) {
    return 21;
  }

  const float head_scores[] = {0.95f, 0.20f};
  const float head_bbox[] = {
      1.0f, 2.0f, 3.0f, 4.0f,
      1.0f, 1.0f, 1.0f, 1.0f,
  };
  const float head_landmarks[] = {
      1.0f, 1.0f, 2.0f, 1.0f, 1.5f, 2.0f, 1.2f, 2.5f, 1.8f, 2.5f,
      0.5f, 0.5f, 0.7f, 0.5f, 0.6f, 0.8f, 0.5f, 1.0f, 0.7f, 1.0f,
  };
  sentriface::detector::ScrfdHeadOutputView head_view;
  head_view.scores = head_scores;
  head_view.bbox = head_bbox;
  head_view.landmarks = head_landmarks;
  head_view.locations = 2;
  head_view.score_dim = 1;
  head_view.bbox_dim = 4;
  head_view.landmark_dim = 10;

  auto decoded = cpu_detector.DecodeHeadCandidates(head_view, 8, 8, 8);
  if (decoded.size() != 1U) {
    return 22;
  }
  if (decoded[0].bbox.x != 0.0f || decoded[0].bbox.y != 0.0f) {
    return 23;
  }
  if (decoded[0].bbox.w != 8.0f || decoded[0].bbox.h != 8.0f) {
    return 24;
  }
  if (decoded[0].landmarks.points[0].x != 8.0f ||
      decoded[0].landmarks.points[0].y != 8.0f) {
    return 25;
  }

  const float level0_scores[] = {0.93f, 0.10f};
  const float level0_bbox[] = {
      0.5f, 0.5f, 0.5f, 0.5f,
      0.5f, 0.5f, 0.5f, 0.5f,
  };
  const float level1_scores[] = {0.91f, 0.20f};
  const float level1_bbox[] = {
      0.5f, 0.25f, 0.5f, 0.25f,
      0.5f, 0.25f, 0.5f, 0.25f,
  };
  const float level2_scores[] = {0.88f, 0.30f};
  const float level2_bbox[] = {
      0.25f, 0.25f, 0.25f, 0.25f,
      0.25f, 0.25f, 0.25f, 0.25f,
  };
  ScrfdDetectorConfig multi_level_config = cpu_config;
  multi_level_config.feature_strides = {8, 8, 8};
  multi_level_config.enable_landmarks = false;
  ScrfdFaceDetector multi_level_detector(multi_level_config, "scrfd_cpu_multi");
  if (!multi_level_detector.IsInitialized()) {
    return 26;
  }
  sentriface::detector::ScrfdHeadOutputView level0_view;
  level0_view.scores = level0_scores;
  level0_view.bbox = level0_bbox;
  level0_view.locations = 2;
  sentriface::detector::ScrfdHeadOutputView level1_view;
  level1_view.scores = level1_scores;
  level1_view.bbox = level1_bbox;
  level1_view.locations = 2;
  sentriface::detector::ScrfdHeadOutputView level2_view;
  level2_view.scores = level2_scores;
  level2_view.bbox = level2_bbox;
  level2_view.locations = 2;

  const auto merged = multi_level_detector.DecodeMultiLevelCandidates(
      std::vector<sentriface::detector::ScrfdHeadOutputView> {
          level0_view, level1_view, level2_view},
      8,
      8);
  if (merged.size() != 3U) {
    return 27;
  }
  if (merged[0].bbox.w != 4.0f || merged[1].bbox.h != 2.0f ||
      merged[2].bbox.w != 2.0f) {
    return 28;
  }

  const auto invalid_merged = multi_level_detector.DecodeMultiLevelCandidates(
      std::vector<sentriface::detector::ScrfdHeadOutputView> {level0_view, level1_view},
      8,
      8);
  if (!invalid_merged.empty()) {
    return 29;
  }

  ScrfdInferenceOutputs outputs;
  outputs.score_heads = {
      ScrfdTensorView {level0_scores, 2, 1},
      ScrfdTensorView {level1_scores, 2, 1},
      ScrfdTensorView {level2_scores, 2, 1},
  };
  outputs.bbox_heads = {
      ScrfdTensorView {level0_bbox, 2, 4},
      ScrfdTensorView {level1_bbox, 2, 4},
      ScrfdTensorView {level2_bbox, 2, 4},
  };
  if (!sentriface::detector::ValidateScrfdInferenceOutputs(
          multi_level_config, outputs, 8, 8)) {
    return 30;
  }
  const auto head_views =
      sentriface::detector::BuildScrfdHeadViews(multi_level_config, outputs, 8, 8);
  if (head_views.size() != 3U || head_views[1].locations != 2 ||
      head_views[2].bbox_dim != 4) {
    return 31;
  }
  const auto rebuilt_merged =
      multi_level_detector.DecodeMultiLevelCandidates(head_views, 8, 8);
  if (rebuilt_merged.size() != 3U) {
    return 32;
  }

  ScrfdInferenceOutputs invalid_outputs = outputs;
  invalid_outputs.bbox_heads[1] = ScrfdTensorView {level1_bbox, 1, 4};
  if (sentriface::detector::ValidateScrfdInferenceOutputs(
          multi_level_config, invalid_outputs, 8, 8)) {
    return 33;
  }

  const auto result = cpu_detector.Detect(frame);
  if (result.timestamp_ms != frame.timestamp_ms || !result.detections.empty()) {
    return 34;
  }

  ScrfdRawCandidate remap_candidate;
  remap_candidate.bbox = {160.0f, 180.0f, 320.0f, 180.0f};
  remap_candidate.score = 0.9f;
  remap_candidate.landmarks.points = {{
      {220.0f, 240.0f}, {420.0f, 240.0f}, {320.0f, 280.0f},
      {250.0f, 330.0f}, {390.0f, 330.0f},
  }};
  const auto remapped_batch = cpu_detector.PostprocessCandidates(
      std::vector<ScrfdRawCandidate> {remap_candidate}, wide_tensor, wide_frame.timestamp_ms);
  if (remapped_batch.detections.size() != 1U) {
    return 35;
  }
  if (remapped_batch.detections[0].bbox.x < 319.0f ||
      remapped_batch.detections[0].bbox.x > 321.0f) {
    return 36;
  }
  if (remapped_batch.detections[0].bbox.w < 639.0f ||
      remapped_batch.detections[0].bbox.w > 641.0f) {
    return 37;
  }

  ScrfdRawCandidate best;
  best.bbox = {100.0f, 120.0f, 96.0f, 104.0f};
  best.score = 0.95f;
  best.landmarks.points = {{
      {128.0f, 156.0f}, {168.0f, 155.0f}, {148.0f, 176.0f},
      {132.0f, 200.0f}, {166.0f, 199.0f},
  }};

  ScrfdRawCandidate overlap = best;
  overlap.score = 0.92f;
  overlap.bbox.x = 102.0f;
  overlap.bbox.y = 121.0f;

  ScrfdRawCandidate second;
  second.bbox = {300.0f, 200.0f, 90.0f, 90.0f};
  second.score = 0.90f;

  ScrfdRawCandidate low_score;
  low_score.bbox = {400.0f, 100.0f, 88.0f, 96.0f};
  low_score.score = 0.20f;

  const auto batch = cpu_detector.PostprocessCandidates(
      std::vector<ScrfdRawCandidate> {overlap, best, second, low_score},
      frame.timestamp_ms);
  if (batch.detections.size() != 2U) {
    return 38;
  }
  if (batch.detections[0].score != 0.95f) {
    return 39;
  }
  if (batch.detections[1].bbox.x != 300.0f) {
    return 40;
  }

  auto fake_runtime = std::make_shared<FakeScrfdRuntime>(
      std::vector<ScrfdRawCandidate> {overlap, best, second, low_score});
  if (!fake_runtime->Initialize(cpu_config)) {
    return 41;
  }
  ScrfdFaceDetector integrated_detector(cpu_config, fake_runtime, "scrfd_fake");
  if (!integrated_detector.IsInitialized() || !integrated_detector.HasRuntime()) {
    return 42;
  }
  const auto integrated_batch = integrated_detector.Detect(frame);
  if (integrated_batch.detections.size() != 2U) {
    return 43;
  }
  if (integrated_batch.detections[0].score != 0.95f ||
      integrated_batch.detections[1].bbox.x != 300.0f) {
    return 44;
  }

  ScrfdDetectorConfig rknn_config = cpu_config;
  rknn_config.runtime = ScrfdRuntime::kRknn;
  rknn_config.model_path = "models/scrfd_500m.rknn";
  ScrfdFaceDetector rknn_detector(rknn_config, "scrfd_rknn");
  if (rknn_detector.GetBackendType() != DetectorBackendType::kScrfdRknnStub) {
    return 45;
  }

#ifdef SENTRIFACE_HAS_ONNXRUNTIME
  ScrfdDetectorConfig ort_config = cpu_config;
  ort_config.detection.score_threshold = 0.99f;
  ort_config.model_path =
      std::string(SENTRIFACE_SOURCE_DIR) + "/third_party/models/buffalo_sc/det_500m.onnx";
  std::ifstream ort_model(ort_config.model_path);
  if (!ort_model.is_open()) {
    return 46;
  }
  ScrfdFaceDetector ort_detector(ort_config, "scrfd_onnxruntime");
  if (!ort_detector.IsInitialized() || !ort_detector.HasRuntime()) {
    return 47;
  }
  const auto ort_batch = ort_detector.Detect(frame);
  if (ort_batch.timestamp_ms != frame.timestamp_ms) {
    return 48;
  }
#endif

  return 0;
}
