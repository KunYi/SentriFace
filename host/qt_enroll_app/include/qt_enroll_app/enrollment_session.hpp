#ifndef SENTRIFACE_HOST_QT_ENROLL_APP_ENROLLMENT_SESSION_HPP_
#define SENTRIFACE_HOST_QT_ENROLL_APP_ENROLLMENT_SESSION_HPP_

#include <QString>

#include "qt_enroll_app/guidance_engine.hpp"

namespace sentriface::host {

enum class EnrollmentSessionState {
  kIdle,
  kSearchingFace,
  kGuidingPose,
  kReadyCountdown,
  kCapturing,
  kReview,
};

struct EnrollmentSessionConfig {
  int target_samples = 3;
  int stable_frames_required = 5;
};

struct EnrollmentSessionSnapshot {
  EnrollmentSessionState state = EnrollmentSessionState::kIdle;
  GuidanceResult guidance {};
  int stable_ready_frames = 0;
  int captured_samples = 0;
  int target_samples = 0;
  bool review_ready = false;
};

class EnrollmentSessionController {
 public:
  explicit EnrollmentSessionController(
      const EnrollmentSessionConfig& config = EnrollmentSessionConfig {});

  EnrollmentSessionSnapshot Advance(const FacePoseSample& sample,
                                    int frame_width,
                                    int frame_height);
  void SetCapturedSamples(int captured_samples);
  void Reset();
  const EnrollmentSessionSnapshot& snapshot() const { return snapshot_; }

  static QString StateToString(EnrollmentSessionState state);

 private:
  EnrollmentSessionConfig config_;
  GuidanceEngine guidance_;
  EnrollmentSessionSnapshot snapshot_ {};
};

}  // namespace sentriface::host

#endif  // SENTRIFACE_HOST_QT_ENROLL_APP_ENROLLMENT_SESSION_HPP_
