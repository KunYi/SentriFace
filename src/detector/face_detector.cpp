#include "detector/face_detector.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "detector/scrfd_runtime.hpp"

namespace sentriface::detector {

namespace {

bool ValidateFrameShape(const sentriface::camera::Frame& frame,
                        const DetectionConfig& config) {
  return frame.width > 0 &&
         frame.height > 0 &&
         frame.channels == config.input_channels;
}

float ComputeRectArea(const sentriface::tracker::RectF& rect) {
  return rect.w > 0.0f && rect.h > 0.0f ? rect.w * rect.h : 0.0f;
}

bool DetectionPriorityLess(const sentriface::tracker::Detection& lhs,
                           const sentriface::tracker::Detection& rhs,
                           bool prefer_larger_faces) {
  if (prefer_larger_faces) {
    const float lhs_area = ComputeRectArea(lhs.bbox);
    const float rhs_area = ComputeRectArea(rhs.bbox);
    if (lhs_area != rhs_area) {
      return lhs_area > rhs_area;
    }
  }
  if (lhs.score != rhs.score) {
    return lhs.score > rhs.score;
  }
  if (lhs.bbox.y != rhs.bbox.y) {
    return lhs.bbox.y < rhs.bbox.y;
  }
  return lhs.bbox.x < rhs.bbox.x;
}

void SortAndTrimDetections(std::vector<sentriface::tracker::Detection>& detections,
                           const DetectionConfig& config) {
  std::sort(detections.begin(), detections.end(),
            [&](const sentriface::tracker::Detection& lhs,
                const sentriface::tracker::Detection& rhs) {
              return DetectionPriorityLess(lhs, rhs, config.prefer_larger_faces);
            });
  if (config.max_output_detections > 0 &&
      static_cast<int>(detections.size()) > config.max_output_detections) {
    detections.resize(static_cast<std::size_t>(config.max_output_detections));
  }
}

bool PassesMinFaceSize(const sentriface::tracker::RectF& rect,
                       const DetectionConfig& config) {
  if (rect.w <= 0.0f || rect.h <= 0.0f) {
    return false;
  }
  if (config.min_face_width > 0.0f && rect.w < config.min_face_width) {
    return false;
  }
  if (config.min_face_height > 0.0f && rect.h < config.min_face_height) {
    return false;
  }
  return true;
}

bool PassesDetectionRoi(const sentriface::tracker::RectF& rect,
                        const DetectionConfig& config) {
  if (!config.enable_roi) {
    return true;
  }
  if (config.roi.w <= 0.0f || config.roi.h <= 0.0f) {
    return true;
  }
  const float center_x = rect.x + rect.w * 0.5f;
  const float center_y = rect.y + rect.h * 0.5f;
  return center_x >= config.roi.x &&
         center_x <= (config.roi.x + config.roi.w) &&
         center_y >= config.roi.y &&
         center_y <= (config.roi.y + config.roi.h);
}

float ClampValue(float value, float min_value, float max_value) {
  return std::max(min_value, std::min(max_value, value));
}

int MapSourceChannel(const sentriface::camera::Frame& frame,
                     bool input_expects_bgr,
                     int model_channel) {
  if (frame.channels != 3) {
    return model_channel;
  }

  const bool frame_is_bgr = frame.pixel_format == sentriface::camera::PixelFormat::kBgr888;
  if (frame_is_bgr == input_expects_bgr) {
    return model_channel;
  }

  if (model_channel == 0) {
    return 2;
  }
  if (model_channel == 2) {
    return 0;
  }
  return 1;
}

float SampleBilinearChannel(const sentriface::camera::Frame& frame,
                           float x,
                           float y,
                           int channel) {
  const float clamped_x = ClampValue(x, 0.0f, static_cast<float>(frame.width - 1));
  const float clamped_y = ClampValue(y, 0.0f, static_cast<float>(frame.height - 1));
  const int x0 = static_cast<int>(clamped_x);
  const int y0 = static_cast<int>(clamped_y);
  const int x1 = std::min(x0 + 1, frame.width - 1);
  const int y1 = std::min(y0 + 1, frame.height - 1);
  const float dx = clamped_x - static_cast<float>(x0);
  const float dy = clamped_y - static_cast<float>(y0);

  auto pixel = [&](int px, int py) -> float {
    const std::size_t index = static_cast<std::size_t>(
        (py * frame.width + px) * frame.channels + channel);
    return static_cast<float>(frame.data[index]);
  };

  const float top = pixel(x0, y0) * (1.0f - dx) + pixel(x1, y0) * dx;
  const float bottom = pixel(x0, y1) * (1.0f - dx) + pixel(x1, y1) * dx;
  return top * (1.0f - dy) + bottom * dy;
}

float NormalizeInputValue(const ScrfdDetectorConfig& config, float value) {
  if (config.use_mean_std_normalization) {
    return (value - config.input_mean) / config.input_std;
  }
  if (config.normalize_to_unit_range) {
    return value * (1.0f / 255.0f);
  }
  return value;
}

void MapCandidateToOriginalFrame(ScrfdRawCandidate& candidate,
                                 const ScrfdInputTensor& tensor,
                                 bool enable_landmarks) {
  if (tensor.resize_scale <= 0.0f) {
    return;
  }

  auto map_x = [&](float x) {
    return ClampValue((x - tensor.pad_x) / tensor.resize_scale,
                      0.0f,
                      static_cast<float>(tensor.original_width));
  };
  auto map_y = [&](float y) {
    return ClampValue((y - tensor.pad_y) / tensor.resize_scale,
                      0.0f,
                      static_cast<float>(tensor.original_height));
  };

  const float x1 = map_x(candidate.bbox.x);
  const float y1 = map_y(candidate.bbox.y);
  const float x2 = map_x(candidate.bbox.x + candidate.bbox.w);
  const float y2 = map_y(candidate.bbox.y + candidate.bbox.h);

  candidate.bbox.x = x1;
  candidate.bbox.y = y1;
  candidate.bbox.w = std::max(0.0f, x2 - x1);
  candidate.bbox.h = std::max(0.0f, y2 - y1);

  if (enable_landmarks) {
    for (auto& point : candidate.landmarks.points) {
      point.x = map_x(point.x);
      point.y = map_y(point.y);
    }
  }
}

}  // namespace

SequenceFaceDetector::SequenceFaceDetector(std::vector<DetectionBatch> batches,
                                           const DetectionConfig& config,
                                           const std::string& name)
    : batches_(batches.begin(), batches.end()), config_(config), name_(name) {}

bool SequenceFaceDetector::ValidateInputFrame(
    const sentriface::camera::Frame& frame) const {
  return ValidateFrameShape(frame, config_);
}

DetectorInfo SequenceFaceDetector::GetInfo() const {
  DetectorInfo info;
  info.name = name_;
  info.is_mock = true;
  info.is_ready = true;
  return info;
}

DetectorBackendType SequenceFaceDetector::GetBackendType() const {
  return DetectorBackendType::kSequence;
}

DetectionBatch SequenceFaceDetector::Detect(const sentriface::camera::Frame& frame) {
  DetectionBatch out;
  out.timestamp_ms = frame.timestamp_ms;
  if (!ValidateInputFrame(frame) || batches_.empty()) {
    return out;
  }

  out = std::move(batches_.front());
  batches_.pop_front();
  out.timestamp_ms = frame.timestamp_ms;
  out.detections = FilterDetections(out.detections);
  return out;
}

std::vector<sentriface::tracker::Detection> SequenceFaceDetector::FilterDetections(
    const std::vector<sentriface::tracker::Detection>& detections) const {
  std::vector<sentriface::tracker::Detection> out;
  out.reserve(detections.size());
  for (const auto& det : detections) {
    if (det.score < config_.score_threshold) {
      continue;
    }
    if (!PassesMinFaceSize(det.bbox, config_)) {
      continue;
    }
    if (!PassesDetectionRoi(det.bbox, config_)) {
      continue;
    }
    out.push_back(det);
  }
  SortAndTrimDetections(out, config_);
  return out;
}

ManifestFaceDetector::ManifestFaceDetector(const std::string& manifest_path,
                                           const DetectionConfig& config,
                                           const std::string& name)
    : manifest_path_(manifest_path), config_(config), name_(name) {
  loaded_ = LoadManifest();
}

bool ManifestFaceDetector::ValidateInputFrame(
    const sentriface::camera::Frame& frame) const {
  return ValidateFrameShape(frame, config_);
}

DetectorInfo ManifestFaceDetector::GetInfo() const {
  DetectorInfo info;
  info.name = name_;
  info.is_mock = true;
  info.is_ready = loaded_;
  return info;
}

DetectorBackendType ManifestFaceDetector::GetBackendType() const {
  return DetectorBackendType::kManifest;
}

DetectionBatch ManifestFaceDetector::Detect(const sentriface::camera::Frame& frame) {
  DetectionBatch out;
  out.timestamp_ms = frame.timestamp_ms;
  if (!loaded_ || !ValidateInputFrame(frame)) {
    return out;
  }

  while (!batches_.empty() && batches_.front().timestamp_ms < frame.timestamp_ms) {
    batches_.pop_front();
  }

  if (batches_.empty() || batches_.front().timestamp_ms != frame.timestamp_ms) {
    return out;
  }

  out = std::move(batches_.front());
  batches_.pop_front();
  out.timestamp_ms = frame.timestamp_ms;
  out.detections = FilterDetections(out.detections);
  return out;
}

bool ManifestFaceDetector::IsLoaded() const { return loaded_; }

bool ManifestFaceDetector::LoadManifest() {
  std::ifstream in(manifest_path_);
  if (!in.is_open()) {
    return false;
  }

  std::unordered_map<std::uint64_t, std::vector<sentriface::tracker::Detection>> grouped;
  std::vector<std::uint64_t> order;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::uint64_t timestamp_ms = 0;
    sentriface::tracker::Detection detection;
    if (!ParseLine(line, timestamp_ms, detection)) {
      return false;
    }

    if (grouped.find(timestamp_ms) == grouped.end()) {
      order.push_back(timestamp_ms);
    }
    grouped[timestamp_ms].push_back(detection);
  }

  for (const auto timestamp_ms : order) {
    DetectionBatch batch;
    batch.timestamp_ms = timestamp_ms;
    batch.detections = grouped[timestamp_ms];
    batches_.push_back(std::move(batch));
  }

  return true;
}

bool ManifestFaceDetector::ParseLine(const std::string& line,
                                     std::uint64_t& timestamp_ms,
                                     sentriface::tracker::Detection& detection) const {
  std::istringstream iss(line);
  auto& lm = detection.landmarks.points;
  if (!(iss >> timestamp_ms >>
        detection.bbox.x >> detection.bbox.y >> detection.bbox.w >> detection.bbox.h >>
        detection.score >>
        lm[0].x >> lm[0].y >>
        lm[1].x >> lm[1].y >>
        lm[2].x >> lm[2].y >>
        lm[3].x >> lm[3].y >>
        lm[4].x >> lm[4].y)) {
    return false;
  }
  detection.timestamp_ms = timestamp_ms;
  return detection.bbox.w > 0.0f && detection.bbox.h > 0.0f;
}

std::vector<sentriface::tracker::Detection> ManifestFaceDetector::FilterDetections(
    const std::vector<sentriface::tracker::Detection>& detections) const {
  std::vector<sentriface::tracker::Detection> out;
  out.reserve(detections.size());
  for (const auto& det : detections) {
    if (det.score < config_.score_threshold) {
      continue;
    }
    if (!PassesMinFaceSize(det.bbox, config_)) {
      continue;
    }
    if (!PassesDetectionRoi(det.bbox, config_)) {
      continue;
    }
    out.push_back(det);
  }
  SortAndTrimDetections(out, config_);
  return out;
}

ScrfdFaceDetector::ScrfdFaceDetector(const ScrfdDetectorConfig& config,
                                     const std::string& name)
    : ScrfdFaceDetector(config, CreateDefaultScrfdRuntime(config), name) {}

ScrfdFaceDetector::ScrfdFaceDetector(const ScrfdDetectorConfig& config,
                                     std::shared_ptr<IScrfdRuntime> runtime,
                                     const std::string& name)
    : config_(config), runtime_(std::move(runtime)), name_(name) {
  initialized_ = ValidateConfig() && runtime_ != nullptr && runtime_->IsReady();
}

bool ScrfdFaceDetector::ValidateInputFrame(
    const sentriface::camera::Frame& frame) const {
  return ValidateFrameShape(frame, config_.detection);
}

DetectorInfo ScrfdFaceDetector::GetInfo() const {
  DetectorInfo info;
  info.name = name_;
  info.is_mock = false;
  info.is_ready = initialized_ && HasRuntime();
  return info;
}

DetectionBatch ScrfdFaceDetector::Detect(const sentriface::camera::Frame& frame) {
  DetectionBatch out;
  out.timestamp_ms = frame.timestamp_ms;
  if (!initialized_ || !ValidateInputFrame(frame)) {
    return out;
  }

  ScrfdInputTensor tensor;
  if (!PrepareInputTensor(frame, tensor)) {
    return out;
  }

  std::vector<ScrfdRawCandidate> candidates;
  if (!runtime_ || !runtime_->Run(tensor, candidates)) {
    return out;
  }
  return PostprocessCandidates(candidates, tensor, frame.timestamp_ms);
}

DetectorBackendType ScrfdFaceDetector::GetBackendType() const {
  return config_.runtime == ScrfdRuntime::kRknn
             ? DetectorBackendType::kScrfdRknnStub
             : DetectorBackendType::kScrfdCpuStub;
}

const ScrfdDetectorConfig& ScrfdFaceDetector::GetConfig() const { return config_; }

bool ScrfdFaceDetector::IsInitialized() const { return initialized_; }

bool ScrfdFaceDetector::IsRuntimeSupported() const {
  return initialized_ && runtime_ != nullptr;
}

bool ScrfdFaceDetector::HasRuntime() const {
  return runtime_ != nullptr && runtime_->IsReady();
}

bool ScrfdFaceDetector::PrepareInputTensor(const sentriface::camera::Frame& frame,
                                           ScrfdInputTensor& tensor) const {
  if (!initialized_ || !ValidateInputFrame(frame)) {
    return false;
  }

  const std::size_t expected_size = static_cast<std::size_t>(
      frame.width * frame.height * frame.channels);
  if (frame.data.size() != expected_size) {
    return false;
  }

  tensor.width = frame.width;
  tensor.height = frame.height;
  tensor.channels = frame.channels;
  tensor.original_width = frame.width;
  tensor.original_height = frame.height;
  tensor.chw_layout = true;
  tensor.width = config_.detection.input_width;
  tensor.height = config_.detection.input_height;
  tensor.resized_width = config_.detection.input_width;
  tensor.resized_height = config_.detection.input_height;
  tensor.pad_x = 0.0f;
  tensor.pad_y = 0.0f;
  tensor.resize_scale = 1.0f;

  const std::size_t tensor_size = static_cast<std::size_t>(
      tensor.width * tensor.height * tensor.channels);
  tensor.data.assign(tensor_size,
                     NormalizeInputValue(config_, 0.0f));

  const float scale_x = static_cast<float>(config_.detection.input_width) /
                        static_cast<float>(frame.width);
  const float scale_y = static_cast<float>(config_.detection.input_height) /
                        static_cast<float>(frame.height);
  tensor.resize_scale = std::min(scale_x, scale_y);
  tensor.resized_width = std::max(
      1, static_cast<int>(static_cast<float>(frame.width) * tensor.resize_scale));
  tensor.resized_height = std::max(
      1, static_cast<int>(static_cast<float>(frame.height) * tensor.resize_scale));
  tensor.pad_x = static_cast<float>(config_.detection.input_width - tensor.resized_width) / 2.0f;
  tensor.pad_y =
      static_cast<float>(config_.detection.input_height - tensor.resized_height) / 2.0f;

  const int plane_size = tensor.width * tensor.height;
  for (int y = 0; y < tensor.height; ++y) {
    for (int x = 0; x < tensor.width; ++x) {
      const float src_x = (static_cast<float>(x) - tensor.pad_x) / tensor.resize_scale;
      const float src_y = (static_cast<float>(y) - tensor.pad_y) / tensor.resize_scale;
      const bool inside_resized =
          x >= static_cast<int>(tensor.pad_x) &&
          x < static_cast<int>(tensor.pad_x) + tensor.resized_width &&
          y >= static_cast<int>(tensor.pad_y) &&
          y < static_cast<int>(tensor.pad_y) + tensor.resized_height;
      for (int c = 0; c < frame.channels; ++c) {
        const std::size_t chw_index = static_cast<std::size_t>(
            c * plane_size + y * tensor.width + x);
        if (!inside_resized) {
          continue;
        }
        const int source_channel = MapSourceChannel(frame, config_.input_expects_bgr, c);
        const float sampled =
            SampleBilinearChannel(frame, src_x, src_y, source_channel);
        const float value = NormalizeInputValue(config_, sampled);
        tensor.data[chw_index] = value;
      }
    }
  }
  return true;
}

DetectionBatch ScrfdFaceDetector::PostprocessCandidates(
    const std::vector<ScrfdRawCandidate>& candidates,
    std::uint64_t timestamp_ms) const {
  DetectionBatch out;
  out.timestamp_ms = timestamp_ms;
  if (!initialized_) {
    return out;
  }

  std::vector<ScrfdRawCandidate> filtered;
  filtered.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    if (candidate.score < config_.detection.score_threshold) {
      continue;
    }
    if (!PassesMinFaceSize(candidate.bbox, config_.detection)) {
      continue;
    }
    if (!PassesDetectionRoi(candidate.bbox, config_.detection)) {
      continue;
    }
    filtered.push_back(candidate);
  }

  std::sort(filtered.begin(), filtered.end(),
            [](const ScrfdRawCandidate& lhs, const ScrfdRawCandidate& rhs) {
              return lhs.score > rhs.score;
            });

  std::vector<ScrfdRawCandidate> kept;
  kept.reserve(filtered.size());
  for (const auto& candidate : filtered) {
    bool suppressed = false;
    for (const auto& selected : kept) {
      if (ComputeIoU(candidate.bbox, selected.bbox) > config_.nms_threshold) {
        suppressed = true;
        break;
      }
    }
    if (suppressed) {
      continue;
    }
    kept.push_back(candidate);
    if (static_cast<int>(kept.size()) >= config_.max_detections) {
      break;
    }
  }

  out.detections.reserve(kept.size());
  for (const auto& candidate : kept) {
    sentriface::tracker::Detection detection;
    detection.bbox = candidate.bbox;
    detection.score = candidate.score;
    detection.timestamp_ms = timestamp_ms;
    if (config_.enable_landmarks) {
      detection.landmarks = candidate.landmarks;
    }
    out.detections.push_back(detection);
  }
  SortAndTrimDetections(out.detections, config_.detection);
  if (config_.max_detections > 0 &&
      static_cast<int>(out.detections.size()) > config_.max_detections) {
    out.detections.resize(static_cast<std::size_t>(config_.max_detections));
  }
  return out;
}

DetectionBatch ScrfdFaceDetector::PostprocessCandidates(
    const std::vector<ScrfdRawCandidate>& candidates,
    const ScrfdInputTensor& tensor,
    std::uint64_t timestamp_ms) const {
  std::vector<ScrfdRawCandidate> remapped = candidates;
  for (auto& candidate : remapped) {
    MapCandidateToOriginalFrame(candidate, tensor, config_.enable_landmarks);
  }
  return PostprocessCandidates(remapped, timestamp_ms);
}

std::vector<int> ScrfdFaceDetector::ComputeExpectedLocations(
    int input_width, int input_height) const {
  std::vector<int> counts;
  counts.reserve(config_.feature_strides.size());
  for (const int stride : config_.feature_strides) {
    if (stride <= 0) {
      counts.push_back(0);
      continue;
    }
    const int grid_w = input_width / stride;
    const int grid_h = input_height / stride;
    counts.push_back(grid_w * grid_h * config_.anchors_per_location);
  }
  return counts;
}

std::vector<ScrfdRawCandidate> ScrfdFaceDetector::DecodeHeadCandidates(
    const ScrfdHeadOutputView& head,
    int input_width,
    int input_height,
    int stride) const {
  std::vector<ScrfdRawCandidate> candidates;
  if (!initialized_ || head.scores == nullptr || head.bbox == nullptr ||
      stride <= 0 || input_width <= 0 || input_height <= 0) {
    return candidates;
  }

  const int grid_w = input_width / stride;
  const int grid_h = input_height / stride;
  const int expected_locations = grid_w * grid_h * config_.anchors_per_location;
  if (head.locations != expected_locations) {
    return candidates;
  }

  candidates.reserve(static_cast<std::size_t>(head.locations));
  for (int index = 0; index < head.locations; ++index) {
    const float score = ReadScore(head, index);
    if (score < config_.detection.score_threshold) {
      continue;
    }

    const int cell_index = index / config_.anchors_per_location;
    const int grid_x = cell_index % grid_w;
    const int grid_y = cell_index / grid_w;
    const float center_x = static_cast<float>(grid_x * stride);
    const float center_y = static_cast<float>(grid_y * stride);

    const float left = head.bbox[index * head.bbox_dim + 0] * stride;
    const float top = head.bbox[index * head.bbox_dim + 1] * stride;
    const float right = head.bbox[index * head.bbox_dim + 2] * stride;
    const float bottom = head.bbox[index * head.bbox_dim + 3] * stride;

    const float x1 = ClampValue(center_x - left, 0.0f, static_cast<float>(input_width));
    const float y1 = ClampValue(center_y - top, 0.0f, static_cast<float>(input_height));
    const float x2 = ClampValue(center_x + right, 0.0f, static_cast<float>(input_width));
    const float y2 = ClampValue(center_y + bottom, 0.0f, static_cast<float>(input_height));

    if (x2 <= x1 || y2 <= y1) {
      continue;
    }

    ScrfdRawCandidate candidate;
    candidate.score = score;
    candidate.bbox.x = x1;
    candidate.bbox.y = y1;
    candidate.bbox.w = x2 - x1;
    candidate.bbox.h = y2 - y1;

    if (config_.enable_landmarks && head.landmarks != nullptr &&
        head.landmark_dim >= 10) {
      for (int point_index = 0; point_index < 5; ++point_index) {
        const float px = head.landmarks[index * head.landmark_dim + point_index * 2 + 0] *
                         stride;
        const float py = head.landmarks[index * head.landmark_dim + point_index * 2 + 1] *
                         stride;
        candidate.landmarks.points[point_index].x =
            ClampValue(center_x + px, 0.0f, static_cast<float>(input_width));
        candidate.landmarks.points[point_index].y =
            ClampValue(center_y + py, 0.0f, static_cast<float>(input_height));
      }
    }

    candidates.push_back(candidate);
  }
  return candidates;
}

std::vector<ScrfdRawCandidate> ScrfdFaceDetector::DecodeMultiLevelCandidates(
    const std::vector<ScrfdHeadOutputView>& heads,
    int input_width,
    int input_height) const {
  std::vector<ScrfdRawCandidate> candidates;
  if (!initialized_ || input_width <= 0 || input_height <= 0 ||
      heads.size() != config_.feature_strides.size()) {
    return candidates;
  }

  std::size_t total_expected_locations = 0U;
  const auto expected_locations = ComputeExpectedLocations(input_width, input_height);
  for (const int count : expected_locations) {
    if (count > 0) {
      total_expected_locations += static_cast<std::size_t>(count);
    }
  }
  candidates.reserve(total_expected_locations);

  for (std::size_t level = 0; level < heads.size(); ++level) {
    const auto level_candidates = DecodeHeadCandidates(
        heads[level], input_width, input_height, config_.feature_strides[level]);
    candidates.insert(candidates.end(), level_candidates.begin(), level_candidates.end());
  }
  return candidates;
}

bool ScrfdFaceDetector::ValidateConfig() const {
  return !config_.model_path.empty() &&
         config_.detection.input_width > 0 &&
         config_.detection.input_height > 0 &&
         config_.detection.input_channels > 0 &&
         config_.input_std > 0.0f &&
         config_.nms_threshold > 0.0f &&
         config_.nms_threshold <= 1.0f &&
         config_.max_detections > 0 &&
         config_.anchors_per_location > 0 &&
         !config_.feature_strides.empty();
}

float ScrfdFaceDetector::ComputeIoU(const sentriface::tracker::RectF& a,
                                    const sentriface::tracker::RectF& b) const {
  const float x1 = std::max(a.x, b.x);
  const float y1 = std::max(a.y, b.y);
  const float x2 = std::min(a.x + a.w, b.x + b.w);
  const float y2 = std::min(a.y + a.h, b.y + b.h);
  const float inter_w = std::max(0.0f, x2 - x1);
  const float inter_h = std::max(0.0f, y2 - y1);
  const float inter_area = inter_w * inter_h;
  const float union_area = ComputeRectArea(a) + ComputeRectArea(b) - inter_area;
  if (union_area <= 0.0f) {
    return 0.0f;
  }
  return inter_area / union_area;
}

float ScrfdFaceDetector::ReadScore(const ScrfdHeadOutputView& head, int index) const {
  if (head.score_dim <= 1) {
    return head.scores[index];
  }
  if (head.score_dim == 2) {
    return head.scores[index * head.score_dim + 1];
  }

  float max_score = head.scores[index * head.score_dim];
  for (int dim = 1; dim < head.score_dim; ++dim) {
    max_score = std::max(max_score, head.scores[index * head.score_dim + dim]);
  }
  return max_score;
}

}  // namespace sentriface::detector
