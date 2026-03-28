#ifndef SENTRIFACE_DETECTOR_FACE_DETECTOR_HPP_
#define SENTRIFACE_DETECTOR_FACE_DETECTOR_HPP_

#include <deque>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "camera/frame_source.hpp"
#include "tracker/detection.hpp"

namespace sentriface::detector {

class IScrfdRuntime;

struct DetectionConfig {
  int input_width = 640;
  int input_height = 640;
  int input_channels = 3;
  float score_threshold = 0.55f;
  float min_face_width = 0.0f;
  float min_face_height = 0.0f;
  bool prefer_larger_faces = true;
  int max_output_detections = 0;
  bool enable_roi = false;
  sentriface::tracker::RectF roi = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct DetectionBatch {
  std::uint64_t timestamp_ms = 0;
  std::vector<sentriface::tracker::Detection> detections;
};

struct DetectorInfo {
  std::string name;
  bool is_mock = false;
  bool is_ready = false;
};

enum class DetectorBackendType : std::uint8_t {
  kSequence = 0,
  kManifest = 1,
  kScrfdCpuStub = 2,
  kScrfdRknnStub = 3,
};

enum class ScrfdRuntime : std::uint8_t {
  kCpu = 0,
  kRknn = 1,
};

struct ScrfdDetectorConfig {
  DetectionConfig detection {};
  ScrfdRuntime runtime = ScrfdRuntime::kCpu;
  std::string model_path;
  bool enable_landmarks = true;
  bool normalize_to_unit_range = true;
  bool use_mean_std_normalization = true;
  bool input_expects_bgr = true;
  float input_mean = 127.5f;
  float input_std = 128.0f;
  float nms_threshold = 0.45f;
  int max_detections = 32;
  std::vector<int> feature_strides {8, 16, 32};
  int anchors_per_location = 2;
};

struct ScrfdInputTensor {
  int width = 0;
  int height = 0;
  int channels = 0;
  int original_width = 0;
  int original_height = 0;
  int resized_width = 0;
  int resized_height = 0;
  float resize_scale = 1.0f;
  float pad_x = 0.0f;
  float pad_y = 0.0f;
  bool chw_layout = true;
  std::vector<float> data;
};

struct ScrfdRawCandidate {
  sentriface::tracker::RectF bbox {};
  float score = 0.0f;
  sentriface::tracker::Landmark5 landmarks {};
};

struct ScrfdHeadOutputView {
  const float* scores = nullptr;
  const float* bbox = nullptr;
  const float* landmarks = nullptr;
  int locations = 0;
  int score_dim = 1;
  int bbox_dim = 4;
  int landmark_dim = 10;
};

class IFaceDetector {
 public:
  virtual ~IFaceDetector() = default;

  virtual bool ValidateInputFrame(const sentriface::camera::Frame& frame) const = 0;
  virtual DetectorInfo GetInfo() const = 0;
  virtual DetectionBatch Detect(const sentriface::camera::Frame& frame) = 0;
  virtual DetectorBackendType GetBackendType() const = 0;
};

class SequenceFaceDetector : public IFaceDetector {
 public:
  explicit SequenceFaceDetector(std::vector<DetectionBatch> batches,
                                const DetectionConfig& config = DetectionConfig {},
                                const std::string& name = "sequence_detector");

  bool ValidateInputFrame(const sentriface::camera::Frame& frame) const override;
  DetectorInfo GetInfo() const override;
  DetectionBatch Detect(const sentriface::camera::Frame& frame) override;
  DetectorBackendType GetBackendType() const override;

 private:
  std::vector<sentriface::tracker::Detection> FilterDetections(
      const std::vector<sentriface::tracker::Detection>& detections) const;

  std::deque<DetectionBatch> batches_;
  DetectionConfig config_;
  std::string name_;
};

class ManifestFaceDetector : public IFaceDetector {
 public:
  explicit ManifestFaceDetector(const std::string& manifest_path,
                                const DetectionConfig& config = DetectionConfig {},
                                const std::string& name = "manifest_detector");

  bool ValidateInputFrame(const sentriface::camera::Frame& frame) const override;
  DetectorInfo GetInfo() const override;
  DetectionBatch Detect(const sentriface::camera::Frame& frame) override;
  DetectorBackendType GetBackendType() const override;

  bool IsLoaded() const;

 private:
  bool LoadManifest();
  bool ParseLine(const std::string& line,
                 std::uint64_t& timestamp_ms,
                 sentriface::tracker::Detection& detection) const;
  std::vector<sentriface::tracker::Detection> FilterDetections(
      const std::vector<sentriface::tracker::Detection>& detections) const;

  std::string manifest_path_;
  DetectionConfig config_;
  std::string name_;
  std::deque<DetectionBatch> batches_;
  bool loaded_ = false;
};

class ScrfdFaceDetector : public IFaceDetector {
 public:
  explicit ScrfdFaceDetector(const ScrfdDetectorConfig& config = ScrfdDetectorConfig {},
                             const std::string& name = "scrfd_detector");
  ScrfdFaceDetector(const ScrfdDetectorConfig& config,
                    std::shared_ptr<IScrfdRuntime> runtime,
                    const std::string& name = "scrfd_detector");

  bool ValidateInputFrame(const sentriface::camera::Frame& frame) const override;
  DetectorInfo GetInfo() const override;
  DetectionBatch Detect(const sentriface::camera::Frame& frame) override;
  DetectorBackendType GetBackendType() const override;

  const ScrfdDetectorConfig& GetConfig() const;
  bool IsInitialized() const;
  bool IsRuntimeSupported() const;
  bool HasRuntime() const;
  bool PrepareInputTensor(const sentriface::camera::Frame& frame,
                          ScrfdInputTensor& tensor) const;
  DetectionBatch PostprocessCandidates(
      const std::vector<ScrfdRawCandidate>& candidates,
      std::uint64_t timestamp_ms) const;
  DetectionBatch PostprocessCandidates(
      const std::vector<ScrfdRawCandidate>& candidates,
      const ScrfdInputTensor& tensor,
      std::uint64_t timestamp_ms) const;
  std::vector<int> ComputeExpectedLocations(int input_width,
                                            int input_height) const;
  std::vector<ScrfdRawCandidate> DecodeHeadCandidates(
      const ScrfdHeadOutputView& head,
      int input_width,
      int input_height,
      int stride) const;
  std::vector<ScrfdRawCandidate> DecodeMultiLevelCandidates(
      const std::vector<ScrfdHeadOutputView>& heads,
      int input_width,
      int input_height) const;

 private:
  bool ValidateConfig() const;
  float ComputeIoU(const sentriface::tracker::RectF& a,
                   const sentriface::tracker::RectF& b) const;
  float ReadScore(const ScrfdHeadOutputView& head, int index) const;

  ScrfdDetectorConfig config_;
  std::shared_ptr<IScrfdRuntime> runtime_;
  std::string name_;
  bool initialized_ = false;
};

}  // namespace sentriface::detector

#endif  // SENTRIFACE_DETECTOR_FACE_DETECTOR_HPP_
