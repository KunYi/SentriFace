#ifndef SENTRIFACE_HOST_QT_ENROLL_APP_CAPTURE_PLAN_HPP_
#define SENTRIFACE_HOST_QT_ENROLL_APP_CAPTURE_PLAN_HPP_

#include <QString>
#include <optional>
#include <vector>

#include "qt_enroll_app/guidance_engine.hpp"

namespace sentriface::host {

enum class CaptureSlotKind {
  kFrontal,
  kLeftQuarter,
  kRightQuarter,
};

struct CaptureSlot {
  CaptureSlotKind kind = CaptureSlotKind::kFrontal;
  bool captured = false;
};

class CapturePlan {
 public:
  CapturePlan();

  void Reset();
  std::optional<CaptureSlotKind> MarkCaptured(CaptureSlotKind kind);
  bool IsComplete() const;
  QString SummaryText() const;
  QString NextPromptText() const;
  const std::vector<CaptureSlot>& capture_slots() const { return slots_; }
  std::optional<CaptureSlotKind> CurrentTarget() const;

  static CaptureSlotKind ClassifyPose(const FacePoseSample& sample);
  static bool IsPoseAccepted(CaptureSlotKind kind, const FacePoseSample& sample);
  static float PreferredPoseScore(CaptureSlotKind kind,
                                  const FacePoseSample& sample);
  static QString SlotLabel(CaptureSlotKind kind);

 private:
  std::vector<CaptureSlot> slots_;
};

}  // namespace sentriface::host

#endif  // SENTRIFACE_HOST_QT_ENROLL_APP_CAPTURE_PLAN_HPP_
