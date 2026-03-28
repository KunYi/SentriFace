#ifndef SENTRIFACE_HOST_QT_ENROLL_APP_OBSERVATION_SOURCE_HPP_
#define SENTRIFACE_HOST_QT_ENROLL_APP_OBSERVATION_SOURCE_HPP_

#include <functional>
#include <memory>

#include <QImage>
#include <QString>

#include "qt_enroll_app/guidance_engine.hpp"

namespace sentriface::host {

enum class ObservationBackend {
  kNone,
  kMock,
  kScrfd,
};

struct ObservationSourceConfig {
  ObservationBackend backend = ObservationBackend::kNone;
  QString model_path;
  int interval_ms = 250;
  float score_threshold = 0.55f;
  int min_face_width = 120;
  int min_face_height = 120;
};

struct FaceObservation {
  bool has_face = false;
  bool backend_ready = false;
  float detection_score = 0.0f;
  QString backend_label;
  FacePoseSample sample {};
};

class IObservationSource {
 public:
  using ObservationCallback = std::function<void(const FaceObservation&)>;

  virtual ~IObservationSource() = default;

  virtual bool Start() = 0;
  virtual void Stop() = 0;
  virtual bool IsRunning() const = 0;
  virtual bool IsReady() const = 0;
  virtual void ObserveFrame(const QImage& image) = 0;
  virtual void SetObservationCallback(ObservationCallback callback) = 0;
  virtual QString Description() const = 0;
  virtual ObservationBackend backend() const = 0;
};

std::unique_ptr<IObservationSource> CreateObservationSource(
    const ObservationSourceConfig& config);
ObservationSourceConfig LoadObservationSourceConfigFromArgs(int argc,
                                                            char* argv[]);
QString ObservationBackendToString(ObservationBackend backend);

}  // namespace sentriface::host

#endif  // SENTRIFACE_HOST_QT_ENROLL_APP_OBSERVATION_SOURCE_HPP_
