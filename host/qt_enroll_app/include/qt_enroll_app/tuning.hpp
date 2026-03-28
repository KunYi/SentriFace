#ifndef SENTRIFACE_HOST_QT_ENROLL_APP_TUNING_HPP_
#define SENTRIFACE_HOST_QT_ENROLL_APP_TUNING_HPP_

namespace sentriface::host::tuning {

constexpr float kEllipseXRatio = 0.28f;
constexpr float kEllipseYRatio = 0.18f;
constexpr float kEllipseWRatio = 0.44f;
constexpr float kEllipseHRatio = 0.58f;

constexpr float kGuidanceMinSizeRatio = 0.31f;
constexpr float kGuidanceMaxSizeRatio = 0.86f;
constexpr float kGuidanceMaxNormDx = 0.12f;
constexpr float kGuidanceMaxNormDy = 0.16f;
constexpr float kGuidanceMaxAbsYawDeg = 14.0f;
constexpr float kGuidanceMaxAbsPitchDeg = 12.0f;
constexpr float kGuidanceMaxAbsRollDeg = 12.0f;
constexpr float kGuidanceMinQualityScore = 0.62f;

constexpr float kPoseFrontalMaxAbsYawDeg = 8.0f;
constexpr float kPoseQuarterMinAbsYawDeg = 7.0f;
constexpr float kPoseQuarterMaxAbsYawDeg = 22.0f;
constexpr float kTargetFrontalYawDeg = 0.0f;
constexpr float kTargetLeftQuarterYawDeg = -12.0f;
constexpr float kTargetRightQuarterYawDeg = 12.0f;

constexpr int kCaptureCollectionWindowMs = 3000;

}  // namespace sentriface::host::tuning

#endif  // SENTRIFACE_HOST_QT_ENROLL_APP_TUNING_HPP_
