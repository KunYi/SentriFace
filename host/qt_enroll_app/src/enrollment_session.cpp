#include "qt_enroll_app/enrollment_session.hpp"

namespace sentriface::host {

EnrollmentSessionController::EnrollmentSessionController(
    const EnrollmentSessionConfig& config)
    : config_(config) {
  snapshot_.state = EnrollmentSessionState::kSearchingFace;
  snapshot_.target_samples = config_.target_samples;
}

EnrollmentSessionSnapshot EnrollmentSessionController::Advance(
    const FacePoseSample& sample,
    int frame_width,
    int frame_height) {
  snapshot_.guidance = guidance_.Evaluate(sample, frame_width, frame_height);
  snapshot_.target_samples = config_.target_samples;

  if (!sample.has_face) {
    snapshot_.state = EnrollmentSessionState::kSearchingFace;
    snapshot_.stable_ready_frames = 0;
    return snapshot_;
  }

  if (!snapshot_.guidance.enrollment_ready) {
    snapshot_.state = EnrollmentSessionState::kGuidingPose;
    snapshot_.stable_ready_frames = 0;
    return snapshot_;
  }

  snapshot_.stable_ready_frames += 1;
  if (snapshot_.stable_ready_frames < config_.stable_frames_required) {
    snapshot_.state = EnrollmentSessionState::kGuidingPose;
    return snapshot_;
  }

  snapshot_.state = EnrollmentSessionState::kCapturing;
  return snapshot_;
}

void EnrollmentSessionController::SetCapturedSamples(int captured_samples) {
  snapshot_.captured_samples = captured_samples;
  snapshot_.review_ready = captured_samples >= config_.target_samples;
  if (snapshot_.review_ready) {
    snapshot_.state = EnrollmentSessionState::kReview;
  }
}

void EnrollmentSessionController::Reset() {
  snapshot_ = EnrollmentSessionSnapshot {};
  snapshot_.state = EnrollmentSessionState::kSearchingFace;
  snapshot_.target_samples = config_.target_samples;
}

QString EnrollmentSessionController::StateToString(EnrollmentSessionState state) {
  switch (state) {
    case EnrollmentSessionState::kIdle:
      return QStringLiteral("idle");
    case EnrollmentSessionState::kSearchingFace:
      return QStringLiteral("searching_face");
    case EnrollmentSessionState::kGuidingPose:
      return QStringLiteral("guiding_pose");
    case EnrollmentSessionState::kReadyCountdown:
      return QStringLiteral("ready_countdown");
    case EnrollmentSessionState::kCapturing:
      return QStringLiteral("capturing");
    case EnrollmentSessionState::kReview:
      return QStringLiteral("review");
  }
  return QStringLiteral("idle");
}

}  // namespace sentriface::host
