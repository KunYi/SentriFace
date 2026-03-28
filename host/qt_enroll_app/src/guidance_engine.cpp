#include "qt_enroll_app/guidance_engine.hpp"

#include <QtGlobal>

#include "qt_enroll_app/tuning.hpp"

namespace sentriface::host {

GuidanceResult GuidanceEngine::Evaluate(const FacePoseSample& sample,
                                        int frame_width,
                                        int frame_height) const {
  GuidanceResult result;
  if (!sample.has_face || frame_width <= 0 || frame_height <= 0) {
    result.key = GuidanceKey::kCenterFace;
    return result;
  }

  const float center_x = sample.bbox_x + sample.bbox_w * 0.5f;
  const float center_y = sample.bbox_y + sample.bbox_h * 0.5f;
  const float ellipse_x = frame_width * tuning::kEllipseXRatio;
  const float ellipse_y = frame_height * tuning::kEllipseYRatio;
  const float ellipse_w = frame_width * tuning::kEllipseWRatio;
  const float ellipse_h = frame_height * tuning::kEllipseHRatio;
  const float ellipse_center_x = ellipse_x + ellipse_w * 0.5f;
  const float ellipse_center_y = ellipse_y + ellipse_h * 0.5f;
  const float norm_dx = (center_x - ellipse_center_x) / ellipse_w;
  const float norm_dy = (center_y - ellipse_center_y) / ellipse_h;
  const float size_ratio = sample.bbox_h / ellipse_h;

  if (size_ratio < tuning::kGuidanceMinSizeRatio) {
    result.key = GuidanceKey::kMoveCloser;
    return result;
  }
  if (size_ratio > tuning::kGuidanceMaxSizeRatio) {
    result.key = GuidanceKey::kMoveBack;
    return result;
  }
  if (norm_dx < -tuning::kGuidanceMaxNormDx) {
    result.key = GuidanceKey::kMoveLeft;
    return result;
  }
  if (norm_dx > tuning::kGuidanceMaxNormDx) {
    result.key = GuidanceKey::kMoveRight;
    return result;
  }
  if (norm_dy < -tuning::kGuidanceMaxNormDy) {
    result.key = GuidanceKey::kMoveUp;
    return result;
  }
  if (norm_dy > tuning::kGuidanceMaxNormDy) {
    result.key = GuidanceKey::kMoveDown;
    return result;
  }
  if (qAbs(sample.yaw_deg) > tuning::kGuidanceMaxAbsYawDeg ||
      qAbs(sample.roll_deg) > tuning::kGuidanceMaxAbsRollDeg ||
      qAbs(sample.pitch_deg) > tuning::kGuidanceMaxAbsPitchDeg) {
    result.key = GuidanceKey::kFaceForward;
    return result;
  }
  if (sample.quality_score < tuning::kGuidanceMinQualityScore) {
    result.key = GuidanceKey::kKeepStill;
    return result;
  }

  result.key = GuidanceKey::kReady;
  result.enrollment_ready = true;
  return result;
}

QString GuidanceEngine::ToI18nKey(GuidanceKey key) {
  switch (key) {
    case GuidanceKey::kReady:
      return QStringLiteral("guidance.ready");
    case GuidanceKey::kMoveLeft:
      return QStringLiteral("guidance.move_left");
    case GuidanceKey::kMoveRight:
      return QStringLiteral("guidance.move_right");
    case GuidanceKey::kMoveUp:
      return QStringLiteral("guidance.move_up");
    case GuidanceKey::kMoveDown:
      return QStringLiteral("guidance.move_down");
    case GuidanceKey::kMoveCloser:
      return QStringLiteral("guidance.move_closer");
    case GuidanceKey::kMoveBack:
      return QStringLiteral("guidance.move_back");
    case GuidanceKey::kCenterFace:
      return QStringLiteral("guidance.center_face");
    case GuidanceKey::kKeepStill:
      return QStringLiteral("guidance.keep_still");
    case GuidanceKey::kFaceForward:
      return QStringLiteral("guidance.face_forward");
  }
  return QStringLiteral("guidance.center_face");
}

}  // namespace sentriface::host
