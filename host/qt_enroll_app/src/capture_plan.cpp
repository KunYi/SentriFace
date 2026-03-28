#include "qt_enroll_app/capture_plan.hpp"

#include <QStringList>
#include <QtGlobal>

#include "qt_enroll_app/tuning.hpp"

namespace sentriface::host {

CapturePlan::CapturePlan() {
  Reset();
}

void CapturePlan::Reset() {
  slots_.clear();
  slots_.push_back(CaptureSlot {CaptureSlotKind::kFrontal, false});
  slots_.push_back(CaptureSlot {CaptureSlotKind::kLeftQuarter, false});
  slots_.push_back(CaptureSlot {CaptureSlotKind::kRightQuarter, false});
}

std::optional<CaptureSlotKind> CapturePlan::MarkCaptured(CaptureSlotKind kind) {
  for (auto& slot : slots_) {
    if (slot.kind == kind && !slot.captured) {
      slot.captured = true;
      return slot.kind;
    }
  }

  for (auto& slot : slots_) {
    if (!slot.captured) {
      slot.captured = true;
      return slot.kind;
    }
  }
  return std::nullopt;
}

std::optional<CaptureSlotKind> CapturePlan::CurrentTarget() const {
  for (const auto& slot : slots_) {
    if (!slot.captured) {
      return slot.kind;
    }
  }
  return std::nullopt;
}

bool CapturePlan::IsComplete() const {
  for (const auto& slot : slots_) {
    if (!slot.captured) {
      return false;
    }
  }
  return true;
}

QString CapturePlan::SummaryText() const {
  QStringList parts;
  for (const auto& slot : slots_) {
    parts.push_back(QStringLiteral("%1:%2")
                        .arg(SlotLabel(slot.kind))
                        .arg(slot.captured ? QStringLiteral("ok")
                                           : QStringLiteral("pending")));
  }
  return parts.join(QStringLiteral(" | "));
}

QString CapturePlan::NextPromptText() const {
  const auto target = CurrentTarget();
  if (!target.has_value()) {
    return QStringLiteral("Capture plan complete");
  }
  switch (*target) {
    case CaptureSlotKind::kFrontal:
      return QStringLiteral("Hold a frontal face pose");
    case CaptureSlotKind::kLeftQuarter:
      return QStringLiteral("Turn slightly left and hold");
    case CaptureSlotKind::kRightQuarter:
      return QStringLiteral("Turn slightly right and hold");
  }
  return QStringLiteral("Capture plan complete");
}

CaptureSlotKind CapturePlan::ClassifyPose(const FacePoseSample& sample) {
  if (sample.yaw_deg < -tuning::kPoseFrontalMaxAbsYawDeg) {
    return CaptureSlotKind::kLeftQuarter;
  }
  if (sample.yaw_deg > tuning::kPoseFrontalMaxAbsYawDeg) {
    return CaptureSlotKind::kRightQuarter;
  }
  return CaptureSlotKind::kFrontal;
}

bool CapturePlan::IsPoseAccepted(CaptureSlotKind kind,
                                 const FacePoseSample& sample) {
  switch (kind) {
    case CaptureSlotKind::kFrontal:
      return qAbs(sample.yaw_deg) <= tuning::kPoseFrontalMaxAbsYawDeg;
    case CaptureSlotKind::kLeftQuarter:
      return sample.yaw_deg <= -tuning::kPoseQuarterMinAbsYawDeg &&
             sample.yaw_deg >= -tuning::kPoseQuarterMaxAbsYawDeg;
    case CaptureSlotKind::kRightQuarter:
      return sample.yaw_deg >= tuning::kPoseQuarterMinAbsYawDeg &&
             sample.yaw_deg <= tuning::kPoseQuarterMaxAbsYawDeg;
  }
  return false;
}

float CapturePlan::PreferredPoseScore(CaptureSlotKind kind,
                                      const FacePoseSample& sample) {
  float target_yaw = 0.0f;
  switch (kind) {
    case CaptureSlotKind::kFrontal:
      target_yaw = tuning::kTargetFrontalYawDeg;
      break;
    case CaptureSlotKind::kLeftQuarter:
      target_yaw = tuning::kTargetLeftQuarterYawDeg;
      break;
    case CaptureSlotKind::kRightQuarter:
      target_yaw = tuning::kTargetRightQuarterYawDeg;
      break;
  }

  const float yaw_penalty = qAbs(sample.yaw_deg - target_yaw) * 0.02f;
  const float pitch_penalty = qAbs(sample.pitch_deg) * 0.015f;
  const float roll_penalty = qAbs(sample.roll_deg) * 0.015f;
  const float size_bonus = sample.bbox_h * 0.0015f;
  return sample.quality_score + size_bonus - yaw_penalty - pitch_penalty -
         roll_penalty;
}

QString CapturePlan::SlotLabel(CaptureSlotKind kind) {
  switch (kind) {
    case CaptureSlotKind::kFrontal:
      return QStringLiteral("frontal");
    case CaptureSlotKind::kLeftQuarter:
      return QStringLiteral("left_quarter");
    case CaptureSlotKind::kRightQuarter:
      return QStringLiteral("right_quarter");
  }
  return QStringLiteral("frontal");
}

}  // namespace sentriface::host
