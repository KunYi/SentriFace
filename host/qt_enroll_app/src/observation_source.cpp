#include "qt_enroll_app/observation_source.hpp"

#define SENTRIFACE_LOG_TAG "host_observation"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "camera/frame_source.hpp"
#include "detector/face_detector.hpp"
#include "logging/system_logger.hpp"

namespace sentriface::host {

namespace {

sentriface::logging::SystemLogger& GetHostObservationLogger() {
  static const sentriface::logging::LoggerConfig config =
      sentriface::logging::LoadLoggerConfigFromEnv(
          sentriface::logging::LoggerConfig {
              true,
              sentriface::logging::LogLevel::kInfo,
              sentriface::logging::LogBackend::kStdout,
              sentriface::logging::LogCompression::kNone,
              "",
              "",
              "",
          });
  static sentriface::logging::SystemLogger logger(config);
  return logger;
}

bool StartsWith(const char* value, const char* prefix) {
  return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

float ClampUnit(float value) {
  return std::max(0.0f, std::min(1.0f, value));
}

float ComputeRollDeg(const sentriface::tracker::Landmark5& landmarks) {
  const auto& left_eye = landmarks.points[0];
  const auto& right_eye = landmarks.points[1];
  return std::atan2(right_eye.y - left_eye.y, right_eye.x - left_eye.x) *
         (180.0f / 3.14159265f);
}

float ComputeYawDeg(const sentriface::tracker::Landmark5& landmarks) {
  const auto& left_eye = landmarks.points[0];
  const auto& right_eye = landmarks.points[1];
  const auto& nose = landmarks.points[2];
  const float span = std::max(1.0f, right_eye.x - left_eye.x);
  const float mid_x = (left_eye.x + right_eye.x) * 0.5f;
  const float normalized = (nose.x - mid_x) / span;
  return std::max(-30.0f, std::min(30.0f, normalized * 90.0f));
}

float ComputePitchDeg(const sentriface::tracker::Landmark5& landmarks) {
  const auto& left_eye = landmarks.points[0];
  const auto& right_eye = landmarks.points[1];
  const auto& nose = landmarks.points[2];
  const auto& left_mouth = landmarks.points[3];
  const auto& right_mouth = landmarks.points[4];
  const float eye_y = (left_eye.y + right_eye.y) * 0.5f;
  const float mouth_y = (left_mouth.y + right_mouth.y) * 0.5f;
  const float center_y = (eye_y + mouth_y) * 0.5f;
  const float span = std::max(1.0f, mouth_y - eye_y);
  const float normalized = (nose.y - center_y) / span;
  return std::max(-20.0f, std::min(20.0f, normalized * 60.0f));
}

FaceObservation MakeObservationFromDetection(
    const sentriface::tracker::Detection& detection,
    int frame_height,
    const QString& backend_label,
    bool backend_ready) {
  FaceObservation observation;
  observation.has_face = true;
  observation.backend_ready = backend_ready;
  observation.detection_score = detection.score;
  observation.backend_label = backend_label;
  observation.sample.has_face = true;
  observation.sample.bbox_x = detection.bbox.x;
  observation.sample.bbox_y = detection.bbox.y;
  observation.sample.bbox_w = detection.bbox.w;
  observation.sample.bbox_h = detection.bbox.h;
  observation.sample.roll_deg = ComputeRollDeg(detection.landmarks);
  observation.sample.yaw_deg = ComputeYawDeg(detection.landmarks);
  observation.sample.pitch_deg = ComputePitchDeg(detection.landmarks);
  const float size_ratio =
      frame_height > 0 ? detection.bbox.h / static_cast<float>(frame_height) : 0.0f;
  observation.sample.quality_score =
      ClampUnit(detection.score * 0.65f + size_ratio * 0.35f);
  return observation;
}

class MockObservationSource : public IObservationSource {
 public:
  explicit MockObservationSource(const ObservationSourceConfig& config)
      : config_(config) {}

  bool Start() override {
    running_ = true;
    SENTRIFACE_LOGI(GetHostObservationLogger(),
                 "observation_start backend=mock ok=1");
    return true;
  }

  void Stop() override { running_ = false; }
  bool IsRunning() const override { return running_; }
  bool IsReady() const override { return true; }

  void ObserveFrame(const QImage& image) override {
    if (!running_ || !callback_ || image.isNull()) {
      return;
    }
    FaceObservation observation;
    observation.has_face = true;
    observation.backend_ready = true;
    observation.backend_label = QStringLiteral("mock");
    observation.detection_score = 0.95f;
    observation.sample.has_face = true;
    observation.sample.bbox_w = image.width() * 0.30f;
    observation.sample.bbox_h = image.height() * 0.48f;
    observation.sample.bbox_x = (image.width() - observation.sample.bbox_w) * 0.5f;
    observation.sample.bbox_y = (image.height() - observation.sample.bbox_h) * 0.20f;
    observation.sample.quality_score = 0.92f;
    callback_(observation);
  }

  void SetObservationCallback(ObservationCallback callback) override {
    callback_ = std::move(callback);
  }

  QString Description() const override {
    return QStringLiteral("mock_observation:interval_ms=%1").arg(config_.interval_ms);
  }

  ObservationBackend backend() const override {
    return ObservationBackend::kMock;
  }

 private:
  ObservationSourceConfig config_;
  bool running_ = false;
  ObservationCallback callback_;
};

class ScrfdObservationSource : public IObservationSource {
 public:
  explicit ScrfdObservationSource(const ObservationSourceConfig& config)
      : config_(config) {
    sentriface::detector::ScrfdDetectorConfig detector_config;
    detector_config.model_path = config.model_path.toStdString();
    detector_config.runtime = sentriface::detector::ScrfdRuntime::kCpu;
    detector_config.detection.score_threshold = config.score_threshold;
    detector_config.detection.min_face_width =
        static_cast<float>(config.min_face_width);
    detector_config.detection.min_face_height =
        static_cast<float>(config.min_face_height);
    detector_config.detection.max_output_detections = 1;
    detector_config.detection.prefer_larger_faces = true;
    detector_ = std::make_unique<sentriface::detector::ScrfdFaceDetector>(
        detector_config, "host_qt_scrfd");
    ready_ = detector_ != nullptr && detector_->GetInfo().is_ready;
    SENTRIFACE_LOGI(GetHostObservationLogger(),
                 "observation_init backend=scrfd ready=" << (ready_ ? 1 : 0)
                     << " model=" << config.model_path.toStdString()
                     << " interval_ms=" << config.interval_ms);
  }

  bool Start() override {
    running_ = ready_;
    SENTRIFACE_LOGI(GetHostObservationLogger(),
                 "observation_start backend=scrfd ok=" << (running_ ? 1 : 0));
    return running_;
  }

  void Stop() override { running_ = false; }
  bool IsRunning() const override { return running_; }
  bool IsReady() const override { return ready_; }

  void ObserveFrame(const QImage& image) override {
    if (!running_ || !callback_ || detector_ == nullptr || image.isNull()) {
      return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (last_run_time_.time_since_epoch().count() != 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - last_run_time_);
      if (elapsed.count() < config_.interval_ms) {
        return;
      }
    }
    last_run_time_ = now;

    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    sentriface::camera::Frame frame;
    frame.width = rgb.width();
    frame.height = rgb.height();
    frame.channels = 3;
    frame.pixel_format = sentriface::camera::PixelFormat::kRgb888;
    frame.timestamp_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch())
            .count());
    frame.data.resize(static_cast<std::size_t>(rgb.sizeInBytes()));
    std::memcpy(frame.data.data(), rgb.constBits(), rgb.sizeInBytes());

    FaceObservation observation;
    observation.backend_ready = ready_;
    observation.backend_label = QStringLiteral("scrfd");

    const sentriface::detector::DetectionBatch batch = detector_->Detect(frame);
    if (!batch.detections.empty()) {
      observation = MakeObservationFromDetection(batch.detections.front(),
                                                 frame.height,
                                                 QStringLiteral("scrfd"),
                                                 ready_);
      SENTRIFACE_LOGD(GetHostObservationLogger(),
                   "observation_result backend=scrfd face=1 score="
                       << observation.detection_score);
    } else {
      SENTRIFACE_LOGD(GetHostObservationLogger(),
                   "observation_result backend=scrfd face=0");
    }
    callback_(observation);
  }

  void SetObservationCallback(ObservationCallback callback) override {
    callback_ = std::move(callback);
  }

  QString Description() const override {
    return QStringLiteral("scrfd_observation:%1:model=%2:interval_ms=%3")
        .arg(ready_ ? QStringLiteral("ready") : QStringLiteral("not_ready"))
        .arg(config_.model_path.isEmpty() ? QStringLiteral("unset")
                                          : config_.model_path)
        .arg(config_.interval_ms);
  }

  ObservationBackend backend() const override {
    return ObservationBackend::kScrfd;
  }

 private:
  ObservationSourceConfig config_;
  bool running_ = false;
  bool ready_ = false;
  ObservationCallback callback_;
  std::chrono::steady_clock::time_point last_run_time_ {};
  std::unique_ptr<sentriface::detector::ScrfdFaceDetector> detector_;
};

}  // namespace

std::unique_ptr<IObservationSource> CreateObservationSource(
    const ObservationSourceConfig& config) {
  switch (config.backend) {
    case ObservationBackend::kMock:
      return std::make_unique<MockObservationSource>(config);
    case ObservationBackend::kScrfd:
      return std::make_unique<ScrfdObservationSource>(config);
    case ObservationBackend::kNone:
    default:
      return nullptr;
  }
}

ObservationSourceConfig LoadObservationSourceConfigFromArgs(int argc,
                                                            char* argv[]) {
  ObservationSourceConfig config;
  for (int index = 1; index < argc; ++index) {
    const char* arg = argv[index];
    if (StartsWith(arg, "--observation=")) {
      const char* value = arg + std::strlen("--observation=");
      if (std::strcmp(value, "mock") == 0) {
        config.backend = ObservationBackend::kMock;
      } else if (std::strcmp(value, "scrfd") == 0) {
        config.backend = ObservationBackend::kScrfd;
      } else {
        config.backend = ObservationBackend::kNone;
      }
    } else if (StartsWith(arg, "--detector-model=")) {
      config.model_path =
          QString::fromUtf8(arg + std::strlen("--detector-model="));
    } else if (StartsWith(arg, "--detector-interval-ms=")) {
      config.interval_ms =
          std::max(1, std::atoi(arg + std::strlen("--detector-interval-ms=")));
    } else if (StartsWith(arg, "--detector-score=")) {
      config.score_threshold =
          std::max(0.0f, std::min(1.0f, static_cast<float>(
              std::atof(arg + std::strlen("--detector-score=")))));
    } else if (StartsWith(arg, "--detector-min-face-width=")) {
      config.min_face_width = std::max(
          0, std::atoi(arg + std::strlen("--detector-min-face-width=")));
    } else if (StartsWith(arg, "--detector-min-face-height=")) {
      config.min_face_height = std::max(
          0, std::atoi(arg + std::strlen("--detector-min-face-height=")));
    }
  }
  return config;
}

QString ObservationBackendToString(ObservationBackend backend) {
  switch (backend) {
    case ObservationBackend::kMock:
      return QStringLiteral("mock");
    case ObservationBackend::kScrfd:
      return QStringLiteral("scrfd");
    case ObservationBackend::kNone:
    default:
      return QStringLiteral("none");
  }
}

}  // namespace sentriface::host
