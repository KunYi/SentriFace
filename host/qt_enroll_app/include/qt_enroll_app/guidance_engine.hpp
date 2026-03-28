#ifndef SENTRIFACE_HOST_QT_ENROLL_APP_GUIDANCE_ENGINE_HPP_
#define SENTRIFACE_HOST_QT_ENROLL_APP_GUIDANCE_ENGINE_HPP_

#include <QString>

namespace sentriface::host {

enum class GuidanceKey {
  kReady,
  kMoveLeft,
  kMoveRight,
  kMoveUp,
  kMoveDown,
  kMoveCloser,
  kMoveBack,
  kCenterFace,
  kKeepStill,
  kFaceForward,
};

struct FacePoseSample {
  bool has_face = false;
  float bbox_x = 0.0f;
  float bbox_y = 0.0f;
  float bbox_w = 0.0f;
  float bbox_h = 0.0f;
  float yaw_deg = 0.0f;
  float pitch_deg = 0.0f;
  float roll_deg = 0.0f;
  float quality_score = 0.0f;
};

struct GuidanceResult {
  GuidanceKey key = GuidanceKey::kCenterFace;
  bool enrollment_ready = false;
};

class GuidanceEngine {
 public:
  GuidanceEngine() = default;

  GuidanceResult Evaluate(const FacePoseSample& sample,
                          int frame_width,
                          int frame_height) const;
  static QString ToI18nKey(GuidanceKey key);
};

}  // namespace sentriface::host

#endif  // SENTRIFACE_HOST_QT_ENROLL_APP_GUIDANCE_ENGINE_HPP_
