#include "qt_enroll_app/main_window.hpp"

#define SENTRIFACE_LOG_TAG "host_ui"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCoreApplication>
#include <QMetaObject>
#include <QPushButton>
#include <QRectF>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdlib>
#include <string>

#include "logging/system_logger.hpp"
#include "qt_enroll_app/preview_widget.hpp"
#include "qt_enroll_app/tuning.hpp"

namespace {

sentriface::logging::SystemLogger& GetHostUiLogger() {
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

bool IsDebugUiEnabled() {
  const char* value = std::getenv("SENTRIFACE_HOST_QT_ENROLL_DEBUG_UI");
  return value != nullptr && std::string(value) == "1";
}

QString GuidanceText(sentriface::host::GuidanceKey key) {
  switch (key) {
    case sentriface::host::GuidanceKey::kReady:
      return QObject::tr("Ready to enroll");
    case sentriface::host::GuidanceKey::kMoveLeft:
      return QObject::tr("Move your face left inside the oval");
    case sentriface::host::GuidanceKey::kMoveRight:
      return QObject::tr("Move your face right inside the oval");
    case sentriface::host::GuidanceKey::kMoveUp:
      return QObject::tr("Raise your face inside the oval");
    case sentriface::host::GuidanceKey::kMoveDown:
      return QObject::tr("Lower your face inside the oval");
    case sentriface::host::GuidanceKey::kMoveCloser:
      return QObject::tr("Move closer to the camera");
    case sentriface::host::GuidanceKey::kMoveBack:
      return QObject::tr("Move slightly farther from the camera");
    case sentriface::host::GuidanceKey::kCenterFace:
      return QObject::tr("Center your face in the frame");
    case sentriface::host::GuidanceKey::kKeepStill:
      return QObject::tr("Hold still");
    case sentriface::host::GuidanceKey::kFaceForward:
      return QObject::tr("Face forward");
  }
  return QObject::tr("Center your face in the frame");
}

const char* GuidanceKeyName(sentriface::host::GuidanceKey key) {
  switch (key) {
    case sentriface::host::GuidanceKey::kReady:
      return "ready";
    case sentriface::host::GuidanceKey::kMoveLeft:
      return "move_left";
    case sentriface::host::GuidanceKey::kMoveRight:
      return "move_right";
    case sentriface::host::GuidanceKey::kMoveUp:
      return "move_up";
    case sentriface::host::GuidanceKey::kMoveDown:
      return "move_down";
    case sentriface::host::GuidanceKey::kMoveCloser:
      return "move_closer";
    case sentriface::host::GuidanceKey::kMoveBack:
      return "move_back";
    case sentriface::host::GuidanceKey::kCenterFace:
      return "center_face";
    case sentriface::host::GuidanceKey::kKeepStill:
      return "keep_still";
    case sentriface::host::GuidanceKey::kFaceForward:
      return "face_forward";
  }
  return "unknown";
}

QRectF MapSourceRectToDisplay(const QRectF& source_rect,
                              const QSize& source_size,
                              const QSize& target_size,
                              bool mirror_preview) {
  if (source_size.width() <= 0 || source_size.height() <= 0 ||
      target_size.width() <= 0 || target_size.height() <= 0) {
    return QRectF();
  }

  const float scale = std::max(
      target_size.width() / static_cast<float>(source_size.width()),
      target_size.height() / static_cast<float>(source_size.height()));
  const float rendered_width = source_size.width() * scale;
  const float rendered_height = source_size.height() * scale;
  const float offset_x = (target_size.width() - rendered_width) * 0.5f;
  const float offset_y = (target_size.height() - rendered_height) * 0.5f;

  QRectF mapped(offset_x + source_rect.x() * scale,
                offset_y + source_rect.y() * scale,
                source_rect.width() * scale,
                source_rect.height() * scale);
  if (mirror_preview) {
    mapped.moveLeft(target_size.width() - mapped.left() - mapped.width());
  }
  return mapped;
}

float ComputeFeatureConfidenceScore(const sentriface::host::FacePoseSample& display_sample,
                                    sentriface::host::CaptureSlotKind slot_kind,
                                    const QSize& preview_size,
                                    int stable_ready_frames,
                                    int candidate_count) {
  if (preview_size.width() <= 0 || preview_size.height() <= 0) {
    return 0.5f;
  }

  const float ellipse_x =
      preview_size.width() * sentriface::host::tuning::kEllipseXRatio;
  const float ellipse_y =
      preview_size.height() * sentriface::host::tuning::kEllipseYRatio;
  const float ellipse_w =
      preview_size.width() * sentriface::host::tuning::kEllipseWRatio;
  const float ellipse_h =
      preview_size.height() * sentriface::host::tuning::kEllipseHRatio;
  const float ellipse_cx = ellipse_x + ellipse_w * 0.5f;
  const float ellipse_cy = ellipse_y + ellipse_h * 0.5f;
  const float bbox_cx = display_sample.bbox_x + display_sample.bbox_w * 0.5f;
  const float bbox_cy = display_sample.bbox_y + display_sample.bbox_h * 0.5f;
  const float norm_dx = std::abs((bbox_cx - ellipse_cx) / ellipse_w);
  const float norm_dy = std::abs((bbox_cy - ellipse_cy) / ellipse_h);
  const float size_ratio = ellipse_h > 0.0f ? display_sample.bbox_h / ellipse_h : 0.0f;

  float target_yaw = 0.0f;
  switch (slot_kind) {
    case sentriface::host::CaptureSlotKind::kFrontal:
      target_yaw = sentriface::host::tuning::kTargetFrontalYawDeg;
      break;
    case sentriface::host::CaptureSlotKind::kLeftQuarter:
      target_yaw = sentriface::host::tuning::kTargetLeftQuarterYawDeg;
      break;
    case sentriface::host::CaptureSlotKind::kRightQuarter:
      target_yaw = sentriface::host::tuning::kTargetRightQuarterYawDeg;
      break;
  }

  const float center_score =
      std::max(0.0f, 1.0f - (norm_dx * 1.7f + norm_dy * 1.2f));
  const float size_score =
      std::max(0.0f, 1.0f - std::abs(size_ratio - 0.58f) * 1.8f);
  const float yaw_score =
      std::max(0.0f, 1.0f - std::abs(display_sample.yaw_deg - target_yaw) / 14.0f);
  const float pitch_score =
      std::max(0.0f, 1.0f - std::abs(display_sample.pitch_deg) / 10.0f);
  const float roll_score =
      std::max(0.0f, 1.0f - std::abs(display_sample.roll_deg) / 10.0f);
  const float quality_score =
      std::max(0.0f, std::min(1.0f, (display_sample.quality_score - 0.55f) / 0.35f));
  const float stability_bonus =
      std::min(0.15f, std::max(0, stable_ready_frames - 5) * 0.02f);
  const float candidate_bonus =
      std::min(0.08f, std::max(0, candidate_count - 1) * 0.008f);

  const float raw_score = quality_score * 0.28f + center_score * 0.22f +
                          size_score * 0.18f + yaw_score * 0.14f +
                          pitch_score * 0.08f + roll_score * 0.05f +
                          stability_bonus + candidate_bonus;
  const float clamped = std::max(0.4f, std::min(1.85f, raw_score * 1.45f));
  return clamped;
}

}  // namespace

namespace sentriface::host {

MainWindow::MainWindow(const PreviewFrameSourceConfig& source_config,
                       const ObservationSourceConfig& observation_config,
                       QWidget* parent)
    : QMainWindow(parent),
      source_config_(source_config),
      source_mode_(source_config.mode),
      debug_ui_enabled_(IsDebugUiEnabled()),
      frame_source_(CreatePreviewFrameSource(source_config)),
      observation_source_(CreateObservationSource(observation_config)) {
  setWindowTitle(tr("SentriFace Enrollment Tool"));
  resize(1080, 720);

  auto* central = new QWidget(this);
  auto* root = new QHBoxLayout(central);

  preview_ = new PreviewWidget(central);
  preview_->setMinimumSize(860, 620);
  preview_->SetMirrorPreview(source_config_.mirror_preview);
  preview_->SetDebugVisualsEnabled(debug_ui_enabled_);
  root->addWidget(preview_, 5);

  auto* panel = new QWidget(central);
  panel->setMinimumWidth(250);
  panel->setMaximumWidth(300);
  auto* panel_layout = new QVBoxLayout(panel);

  auto* title = new QLabel(tr("Standard Enrollment Flow"), panel);
  title->setWordWrap(true);
  source_label_ = new QLabel(tr("Input: mock preview source"), panel);
  source_label_->setWordWrap(true);
  preview_status_label_ = new QLabel(tr("Preview status: idle"), panel);
  preview_status_label_->setWordWrap(true);
  observation_status_label_ = new QLabel(tr("Observation status: idle"), panel);
  observation_status_label_->setWordWrap(true);
  identity_label_ = new QLabel(tr("Enrollment identity: not set"), panel);
  identity_label_->setWordWrap(true);
  review_label_ = new QLabel(tr("Review: no captured samples yet"), panel);
  review_label_->setWordWrap(true);
  session_label_ = new QLabel(tr("Session: searching_face"), panel);
  session_label_->setWordWrap(true);
  capture_plan_label_ = new QLabel(tr("Capture plan: frontal:pending | left_quarter:pending | right_quarter:pending"), panel);
  capture_plan_label_->setWordWrap(true);
  status_label_ = new QLabel(tr("Center your face in the frame"), panel);
  status_label_->setWordWrap(true);
  detail_label_ = new QLabel(
      tr("This host tool is intended for guided enrollment on a larger screen before syncing to the access-control device."),
      panel);
  detail_label_->setWordWrap(true);

  auto* user_id_label = new QLabel(tr("User ID"), panel);
  user_id_edit_ = new QLineEdit(panel);
  user_id_edit_->setPlaceholderText(tr("e.g. E1001"));

  auto* user_name_label = new QLabel(tr("Name"), panel);
  user_name_edit_ = new QLineEdit(panel);
  user_name_edit_->setPlaceholderText(tr("e.g. Alice Chen"));

  start_button_ = new QPushButton(tr("Start Enrollment"), panel);
  reset_button_ = new QPushButton(tr("Exit"), panel);
  mock_timer_ = new QTimer(this);
  mock_timer_->setInterval(900);
  status_timer_ = new QTimer(this);
  status_timer_->setInterval(500);
  runtime_clock_.start();
  review_label_->setText(tr("Review: no captured samples yet"));

  connect(start_button_, &QPushButton::clicked, this, &MainWindow::OnStartEnrollment);
  connect(reset_button_, &QPushButton::clicked, this, &MainWindow::OnResetEnrollment);
  connect(mock_timer_, &QTimer::timeout, this, &MainWindow::OnAdvanceMockSequence);
  connect(status_timer_, &QTimer::timeout, this, &MainWindow::OnStatusRefresh);

  panel_layout->addWidget(title);
  panel_layout->addSpacing(12);
  if (debug_ui_enabled_) {
    panel_layout->addWidget(source_label_);
    panel_layout->addWidget(preview_status_label_);
    panel_layout->addWidget(observation_status_label_);
  } else {
    source_label_->hide();
    preview_status_label_->hide();
    observation_status_label_->hide();
  }
  panel_layout->addWidget(user_id_label);
  panel_layout->addWidget(user_id_edit_);
  panel_layout->addWidget(user_name_label);
  panel_layout->addWidget(user_name_edit_);
  panel_layout->addWidget(status_label_);
  if (debug_ui_enabled_) {
    panel_layout->addWidget(identity_label_);
    panel_layout->addWidget(review_label_);
    panel_layout->addWidget(session_label_);
    panel_layout->addWidget(capture_plan_label_);
    panel_layout->addWidget(detail_label_);
  } else {
    identity_label_->hide();
    review_label_->hide();
    session_label_->hide();
    capture_plan_label_->hide();
    detail_label_->hide();
  }
  panel_layout->addSpacing(18);
  panel_layout->addWidget(start_button_);
  panel_layout->addWidget(reset_button_);
  panel_layout->addStretch(1);

  root->addWidget(panel, 1);
  setCentralWidget(central);

  if (frame_source_) {
    frame_source_->SetFrameCallback(
        [this](const QImage& image) {
          QMetaObject::invokeMethod(
              this,
              [this, image]() {
                last_preview_frame_elapsed_ms_ =
                    static_cast<int>(runtime_clock_.elapsed());
                preview_frame_count_ += 1;
                preview_->SetPreviewImage(image);
                if (observation_source_) {
                  observation_source_->ObserveFrame(image);
                }
              },
              Qt::QueuedConnection);
        });
  }
  if (observation_source_) {
    observation_source_->SetObservationCallback(
        [this](const FaceObservation& observation) {
          QMetaObject::invokeMethod(
              this,
              [this, observation]() {
                last_observation_elapsed_ms_ =
                    static_cast<int>(runtime_clock_.elapsed());
                observation_count_ += 1;
                last_observation_had_face_ = observation.has_face;
                ApplyObservation(observation);
              },
              Qt::QueuedConnection);
        });
  }
  preview_started_ = frame_source_ != nullptr && frame_source_->Start();
  observation_started_ =
      observation_source_ != nullptr && observation_source_->Start();
  const QString source_mode = frame_source_ != nullptr
      ? PreviewInputModeToString(frame_source_->mode())
      : QStringLiteral("unknown");
  source_description_ = frame_source_ != nullptr
      ? frame_source_->Description()
      : QStringLiteral("no_source");
  source_label_->setText(
      tr("Input: %1 | Preview: %2 | Observation: %3 | %4")
          .arg(source_mode)
          .arg(preview_started_ ? tr("yes") : tr("no"))
          .arg(observation_started_
                   ? tr("yes (%1)").arg(observation_source_->Description())
                   : tr("pending"))
          .arg(source_description_));
  status_timer_->start();
  OnStatusRefresh();
  ResetEnrollmentWorkflow();
}

MainWindow::~MainWindow() {
  if (status_timer_ != nullptr) {
    status_timer_->stop();
  }
  if (mock_timer_ != nullptr) {
    mock_timer_->stop();
  }
  if (observation_source_) {
    observation_source_->Stop();
  }
  if (frame_source_) {
    frame_source_->Stop();
  }
}

void MainWindow::OnStartEnrollment() {
  if (!HasRequiredEnrollmentFields()) {
    status_label_->setText(tr("Enter User ID and Name first"));
    detail_label_->setText(
        tr("For bring-up convenience, this host test flow combines identity entry and guided capture in one screen."));
    preview_->SetOverlayTexts(
        tr("Fill in User ID and Name"),
        tr("Then press Start to begin automatic guidance"),
        false);
    return;
  }

  enrollment_running_ = true;
  start_button_->setEnabled(false);
  reset_button_->setEnabled(true);
  user_id_edit_->setEnabled(false);
  user_name_edit_->setEnabled(false);
  identity_label_->setText(
      tr("Enrollment identity: %1").arg(EnrollmentIdentitySummary()));
  if (source_mode_ == PreviewInputMode::kMock) {
    mock_step_ = 0;
    mock_timer_->start();
    OnAdvanceMockSequence();
  } else {
    ApplyObservation(FaceObservation {});
  }
}

void MainWindow::OnResetEnrollment() {
  close();
}

void MainWindow::OnAdvanceMockSequence() {
  if (!enrollment_running_) {
    return;
  }
  ApplySample(MakeMockSampleForStep(mock_step_));
  mock_step_ += 1;
  if (capture_plan_.IsComplete()) {
    mock_timer_->stop();
  }
}

void MainWindow::OnStatusRefresh() {
  const QString source_mode = frame_source_ != nullptr
      ? PreviewInputModeToString(frame_source_->mode())
      : QStringLiteral("unknown");
  source_description_ = frame_source_ != nullptr
      ? frame_source_->Description()
      : QStringLiteral("no_source");
  const QString new_source_status = tr("Input: %1 | Preview: %2 | Observation: %3 | %4")
      .arg(source_mode)
      .arg(preview_started_ ? tr("yes") : tr("no"))
      .arg(observation_started_
               ? tr("yes (%1)").arg(observation_source_->Description())
               : tr("pending"))
      .arg(source_description_);
  const QString new_preview_status = PreviewRuntimeStatusText();
  const QString new_observation_status = ObservationRuntimeStatusText();
  if (source_label_ != nullptr) {
    source_label_->setText(new_source_status);
  }
  if (preview_status_label_ != nullptr) {
    preview_status_label_->setText(new_preview_status);
  }
  if (observation_status_label_ != nullptr) {
    observation_status_label_->setText(new_observation_status);
  }
  if (new_source_status != last_source_status_text_) {
    SENTRIFACE_LOGI(GetHostUiLogger(), "source_status " << new_source_status.toStdString());
    last_source_status_text_ = new_source_status;
  }
  if (new_preview_status != last_preview_status_text_) {
    SENTRIFACE_LOGD(GetHostUiLogger(), "preview_status " << new_preview_status.toStdString());
    last_preview_status_text_ = new_preview_status;
  }
  if (new_observation_status != last_observation_status_text_) {
    SENTRIFACE_LOGD(GetHostUiLogger(),
                 "observation_status " << new_observation_status.toStdString());
    last_observation_status_text_ = new_observation_status;
  }
}

void MainWindow::SetEnrollmentIdentity(const QString& user_id,
                                       const QString& user_name) {
  if (user_id_edit_ != nullptr) {
    user_id_edit_->setText(user_id);
  }
  if (user_name_edit_ != nullptr) {
    user_name_edit_->setText(user_name);
  }
  identity_label_->setText(
      HasRequiredEnrollmentFields()
          ? tr("Enrollment identity: %1").arg(EnrollmentIdentitySummary())
          : tr("Enrollment identity: not set"));
}

void MainWindow::ScheduleAutoStart() {
  QTimer::singleShot(0, this, &MainWindow::OnStartEnrollment);
}

FacePoseSample MainWindow::MakeMockSampleForStep(int step) const {
  FacePoseSample sample;
  sample.has_face = true;
  sample.bbox_x = 250.0f;
  sample.bbox_y = 120.0f;
  sample.bbox_w = 220.0f;
  sample.bbox_h = 300.0f;
  sample.quality_score = 0.92f;

  const int phase = step % 9;
  if (phase <= 2) {
    sample.yaw_deg = 0.0f;
  } else if (phase <= 5) {
    sample.yaw_deg = -12.0f;
    sample.bbox_x = 210.0f;
  } else {
    sample.yaw_deg = 12.0f;
    sample.bbox_x = 290.0f;
  }
  return sample;
}

void MainWindow::ResetEnrollmentWorkflow() {
  enrollment_running_ = false;
  mock_step_ = 0;
  captured_samples_.clear();
  last_artifact_dir_.clear();
  last_artifact_summary_path_.clear();
  if (mock_timer_ != nullptr) {
    mock_timer_->stop();
  }
  session_.Reset();
  ResetCollectionWindow();
  capture_plan_.Reset();
  start_button_->setEnabled(true);
  reset_button_->setEnabled(true);
  user_id_edit_->setEnabled(true);
  user_name_edit_->setEnabled(true);
  preview_->SetFaceSample(FacePoseSample {});
  preview_->SetCaptureTarget(capture_plan_.CurrentTarget(), false);
  preview_->SetOverlayTexts(
      tr("Press Start to begin enrollment"),
      tr("Follow the oval guide. The system will automatically prompt distance and pose."),
      false);
  status_label_->setText(tr("Press Start to begin"));
  detail_label_->setText(
      source_mode_ == PreviewInputMode::kMock
          ? tr("Mock mode will auto-run a guided capture sequence after Start.")
          : (observation_source_ != nullptr && observation_source_->IsReady()
                 ? tr("WebCAM preview and detector observation are ready. Press Start and follow the overlay guidance.")
                 : tr("WebCAM preview is ready. Detector-driven face observation is the next integration step.")));
  identity_label_->setText(
      HasRequiredEnrollmentFields()
          ? tr("Enrollment identity: %1").arg(EnrollmentIdentitySummary())
          : tr("Enrollment identity: not set"));
  review_label_->setText(tr("Review: no captured samples yet"));
  session_label_->setText(tr("Session: searching_face | Samples: 0/3 | Stable frames: 0 | Countdown: 0"));
  UpdateCapturePlanUi();
}

bool MainWindow::HasRequiredEnrollmentFields() const {
  return user_id_edit_ != nullptr && user_name_edit_ != nullptr &&
         !user_id_edit_->text().trimmed().isEmpty() &&
         !user_name_edit_->text().trimmed().isEmpty();
}

QString MainWindow::EnrollmentIdentitySummary() const {
  if (user_id_edit_ == nullptr || user_name_edit_ == nullptr) {
    return tr("unknown");
  }
  return tr("%1 / %2")
      .arg(user_id_edit_->text().trimmed(),
           user_name_edit_->text().trimmed());
}

QString MainWindow::PreviewRuntimeStatusText() const {
  if (!preview_started_) {
    return tr("Preview status: not started");
  }
  if (last_preview_frame_elapsed_ms_ < 0) {
    return tr("Preview status: waiting for first frame | %1")
        .arg(source_description_);
  }
  const int age_ms = static_cast<int>(runtime_clock_.elapsed()) -
                     last_preview_frame_elapsed_ms_;
  const QString freshness = age_ms <= 1200 ? tr("live") : tr("stale");
  return tr("Preview status: %1 | frames=%2 | last_frame_age=%3 ms")
      .arg(freshness)
      .arg(preview_frame_count_)
      .arg(std::max(0, age_ms));
}

QString MainWindow::ObservationRuntimeStatusText() const {
  if (observation_source_ == nullptr) {
    return tr("Observation status: disabled");
  }
  if (!observation_started_) {
    return tr("Observation status: pending | %1")
        .arg(observation_source_->Description());
  }
  if (last_observation_elapsed_ms_ < 0) {
    return tr("Observation status: waiting for first result | %1")
        .arg(observation_source_->Description());
  }
  const int age_ms = static_cast<int>(runtime_clock_.elapsed()) -
                     last_observation_elapsed_ms_;
  const QString freshness = age_ms <= 1500 ? tr("live") : tr("stale");
  return tr("Observation status: %1 | results=%2 | face=%3 | last_result_age=%4 ms")
      .arg(freshness)
      .arg(observation_count_)
      .arg(last_observation_had_face_ ? tr("yes") : tr("no"))
      .arg(std::max(0, age_ms));
}

FacePoseSample MainWindow::MakeUiOrientedSample(const FacePoseSample& sample) const {
  FacePoseSample ui_sample = sample;
  if (!source_config_.swap_lateral_guidance) {
    return ui_sample;
  }
  ui_sample.yaw_deg = -sample.yaw_deg;
  ui_sample.roll_deg = -sample.roll_deg;
  return ui_sample;
}

FacePoseSample MainWindow::MakeDisplaySpaceSample(
    const FacePoseSample& sample) const {
  FacePoseSample display_sample = MakeUiOrientedSample(sample);
  if (!display_sample.has_face || preview_ == nullptr ||
      preview_->preview_image().isNull() || preview_->width() <= 0 ||
      preview_->height() <= 0) {
    return display_sample;
  }

  const QRectF source_bbox(sample.bbox_x, sample.bbox_y,
                           sample.bbox_w, sample.bbox_h);
  const QRectF mapped_bbox = MapSourceRectToDisplay(
      source_bbox,
      preview_->preview_image().size(),
      preview_->size(),
      source_config_.mirror_preview);
  if (!mapped_bbox.isValid()) {
    return display_sample;
  }

  display_sample.bbox_x = mapped_bbox.x();
  display_sample.bbox_y = mapped_bbox.y();
  display_sample.bbox_w = mapped_bbox.width();
  display_sample.bbox_h = mapped_bbox.height();
  return display_sample;
}

void MainWindow::ApplySample(const FacePoseSample& sample) {
  preview_->SetFaceSample(sample);
  const FacePoseSample display_sample = MakeDisplaySpaceSample(sample);
  const GuidanceResult result =
      guidance_.Evaluate(display_sample, preview_->width(), preview_->height());
  const float bbox_center_x = display_sample.bbox_x + display_sample.bbox_w * 0.5f;
  const float bbox_center_y = display_sample.bbox_y + display_sample.bbox_h * 0.5f;
  const float ellipse_center_x = preview_->width() * 0.50f;
  const float ellipse_center_y = preview_->height() * 0.46f;
  SENTRIFACE_LOGD(GetHostUiLogger(),
               "guidance_eval key=" << GuidanceKeyName(result.key)
                   << " ready=" << (result.enrollment_ready ? 1 : 0)
                   << " bbox_cx=" << bbox_center_x
                   << " bbox_cy=" << bbox_center_y
                   << " bbox_w=" << display_sample.bbox_w
                   << " bbox_h=" << display_sample.bbox_h
                   << " ellipse_cx=" << ellipse_center_x
                   << " ellipse_cy=" << ellipse_center_y
                   << " yaw=" << display_sample.yaw_deg
                   << " pitch=" << display_sample.pitch_deg
                   << " roll=" << display_sample.roll_deg
                   << " quality=" << display_sample.quality_score);
  EnrollmentSessionSnapshot session_snapshot =
      session_.Advance(display_sample, preview_->width(), preview_->height());
  UpdateCollectionWindow(display_sample, sample, result, &session_snapshot);
  UpdateGuidanceUi(result);
  UpdateSessionUi(session_snapshot);
  UpdateCapturePlanUi();
  UpdatePreviewOverlay(result);
  if (session_snapshot.review_ready) {
    FinalizeEnrollmentArtifactIfReady();
  }
}

void MainWindow::ApplyObservation(const FaceObservation& observation) {
  preview_->SetFaceSample(observation.sample);
  if (!enrollment_running_) {
    if (source_mode_ != PreviewInputMode::kMock) {
      preview_->SetOverlayTexts(
          observation.has_face
              ? tr("Face detected. Press Start to begin enrollment")
              : tr("Align your face inside the oval"),
          observation.backend_ready
              ? tr("Observation backend: %1").arg(observation.backend_label)
              : tr("Observation backend pending"),
          observation.has_face);
    }
    return;
  }
  ApplySample(observation.sample);
}

void MainWindow::UpdateGuidanceUi(const GuidanceResult& result) {
  status_label_->setText(GuidanceText(result.key));
  if (result.enrollment_ready) {
    detail_label_->setText(tr("Hold the requested pose for a few seconds. The system will keep several candidates and save the best one."));
  } else {
    detail_label_->setText(tr("The final product should also support i18n text prompts and optional text-to-speech guidance for different deployment regions."));
  }
}

void MainWindow::UpdateSessionUi(const EnrollmentSessionSnapshot& snapshot) {
  session_label_->setText(
      tr("Session: %1 | Samples: %2/%3 | Stable frames: %4 | Window: %5")
          .arg(EnrollmentSessionController::StateToString(snapshot.state))
          .arg(snapshot.captured_samples)
          .arg(snapshot.target_samples)
          .arg(snapshot.stable_ready_frames)
          .arg(collection_active_
                   ? QStringLiteral("%1/%2s")
                         .arg(std::max(
                             0,
                             (static_cast<int>(runtime_clock_.elapsed()) -
                              collection_start_elapsed_ms_) / 1000))
                         .arg(capture_collection_window_ms_ / 1000)
                   : QStringLiteral("-")));
  if (collection_active_) {
    detail_label_->setText(
        tr("Collecting candidates for %1: %2 kept so far.")
            .arg(CapturePlan::SlotLabel(collection_slot_kind_))
            .arg(collection_candidate_count_));
  } else if (snapshot.review_ready) {
    detail_label_->setText(
        tr("Enough guided samples have been collected for %1. Review and artifact export are ready.")
            .arg(EnrollmentIdentitySummary()));
  } else if (source_mode_ != PreviewInputMode::kMock &&
             snapshot.state == EnrollmentSessionState::kSearchingFace) {
    detail_label_->setText(
        observation_source_ != nullptr && observation_source_->IsReady()
            ? tr("Live preview and detector observation are active. Align your face with the oval guide.")
            : tr("Live preview is active. Detector-driven face observation is the next integration step for webcam mode."));
  } else {
    detail_label_->setText(
        tr("Next preferred sample: %1")
            .arg(capture_plan_.NextPromptText()));
  }
}

void MainWindow::UpdateCapturePlanUi() {
  capture_plan_label_->setText(
      tr("Capture plan: %1").arg(capture_plan_.SummaryText()));
  if (preview_ != nullptr) {
    preview_->SetCaptureTarget(capture_plan_.CurrentTarget(), collection_active_);
  }
}

void MainWindow::UpdatePreviewOverlay(const GuidanceResult& result) {
  const QString top_text = enrollment_running_
      ? (capture_plan_.IsComplete()
             ? tr("Enrollment complete")
             : GuidanceText(result.key))
      : tr("Press Start to begin enrollment");
  const QString bottom_text = enrollment_running_
      ? QString()
      : QString();
  preview_->SetOverlayTexts(top_text, bottom_text,
                            enrollment_running_ && result.enrollment_ready);
}

void MainWindow::RecordCapturedSample(CaptureSlotKind slot_kind,
                                      const FacePoseSample& sample) {
  CapturedEnrollmentSample captured = MakeCapturedSample(slot_kind, sample);
  captured_samples_.push_back(captured);
  session_.SetCapturedSamples(static_cast<int>(captured_samples_.size()));

  review_label_->setText(
      tr("Review: captured %1/%2 | latest=%3")
          .arg(captured_samples_.size())
          .arg(capture_plan_.capture_slots().size())
          .arg(CapturePlan::SlotLabel(slot_kind)));
}

void MainWindow::ResetCollectionWindow() {
  collection_active_ = false;
  collection_start_elapsed_ms_ = -1;
  collection_candidate_count_ = 0;
  collection_best_score_ = -1.0e9f;
  best_collection_sample_.reset();
}

CapturedEnrollmentSample MainWindow::MakeCapturedSample(
    CaptureSlotKind slot_kind,
    const FacePoseSample& source_sample) const {
  CapturedEnrollmentSample captured;
  captured.sample_index = static_cast<int>(captured_samples_.size()) + 1;
  captured.elapsed_ms = static_cast<int>(runtime_clock_.elapsed());
  captured.candidate_count = collection_candidate_count_;
  captured.slot_kind = slot_kind;
  captured.face_sample = source_sample;
  captured.sample_weight = collection_best_score_ > 0.0f ? collection_best_score_ : 1.0f;
  if (preview_ != nullptr) {
    captured.preview_image = preview_->preview_image().copy();
  }
  return captured;
}

void MainWindow::UpdateCollectionWindow(
    const FacePoseSample& display_sample,
    const FacePoseSample& source_sample,
    const GuidanceResult& result,
    EnrollmentSessionSnapshot* session_snapshot) {
  if (session_snapshot == nullptr) {
    return;
  }

  const auto target_slot = capture_plan_.CurrentTarget();
  if (!target_slot.has_value()) {
    session_.SetCapturedSamples(static_cast<int>(captured_samples_.size()));
    *session_snapshot = session_.snapshot();
    return;
  }

  if (!display_sample.has_face || !result.enrollment_ready ||
      !CapturePlan::IsPoseAccepted(*target_slot, display_sample)) {
    ResetCollectionWindow();
    session_.SetCapturedSamples(static_cast<int>(captured_samples_.size()));
    *session_snapshot = session_.snapshot();
    return;
  }

  if (!collection_active_ || collection_slot_kind_ != *target_slot) {
    ResetCollectionWindow();
    collection_active_ = true;
    collection_slot_kind_ = *target_slot;
    collection_start_elapsed_ms_ = static_cast<int>(runtime_clock_.elapsed());
  }

  collection_candidate_count_ += 1;
  const float candidate_score =
      ComputeFeatureConfidenceScore(display_sample,
                                    collection_slot_kind_,
                                    preview_->size(),
                                    session_snapshot->stable_ready_frames,
                                    collection_candidate_count_);
  SENTRIFACE_LOGD(GetHostUiLogger(),
               "capture_candidate slot="
                   << CapturePlan::SlotLabel(collection_slot_kind_).toStdString()
                   << " score=" << candidate_score
                   << " quality=" << display_sample.quality_score
                   << " yaw=" << display_sample.yaw_deg
                   << " pitch=" << display_sample.pitch_deg
                   << " roll=" << display_sample.roll_deg
                   << " stable=" << session_snapshot->stable_ready_frames
                   << " candidates=" << collection_candidate_count_);
  if (!best_collection_sample_.has_value() ||
      candidate_score > collection_best_score_) {
    best_collection_sample_ =
        MakeCapturedSample(collection_slot_kind_, source_sample);
    collection_best_score_ = candidate_score;
  }

  const int elapsed_ms = static_cast<int>(runtime_clock_.elapsed()) -
                         collection_start_elapsed_ms_;
  if (elapsed_ms >= capture_collection_window_ms_ &&
      best_collection_sample_.has_value()) {
    auto captured_slot = capture_plan_.MarkCaptured(collection_slot_kind_);
    if (captured_slot.has_value()) {
      best_collection_sample_->sample_index =
          static_cast<int>(captured_samples_.size()) + 1;
      captured_samples_.push_back(*best_collection_sample_);
      session_.SetCapturedSamples(static_cast<int>(captured_samples_.size()));
      review_label_->setText(
          tr("Review: captured %1/%2 | latest=%3")
              .arg(captured_samples_.size())
              .arg(capture_plan_.capture_slots().size())
              .arg(CapturePlan::SlotLabel(*captured_slot)));
    }
    ResetCollectionWindow();
  } else {
    session_.SetCapturedSamples(static_cast<int>(captured_samples_.size()));
  }

  *session_snapshot = session_.snapshot();
}

void MainWindow::FinalizeEnrollmentArtifactIfReady() {
  if (!capture_plan_.IsComplete() || captured_samples_.empty()) {
    return;
  }
  if (!last_artifact_dir_.isEmpty()) {
    return;
  }

  EnrollmentArtifactRequest request;
  request.user_id = user_id_edit_ != nullptr ? user_id_edit_->text().trimmed() : QString {};
  request.user_name = user_name_edit_ != nullptr ? user_name_edit_->text().trimmed() : QString {};
  request.source_label = source_description_;
  request.observation_label =
      observation_source_ != nullptr ? observation_source_->Description()
                                     : QStringLiteral("disabled");
  request.capture_plan_summary = capture_plan_.SummaryText();
  request.captured_samples = captured_samples_;

  const EnrollmentArtifactResult result = ExportEnrollmentArtifact(request);
  if (result.ok) {
    last_artifact_dir_ = result.output_dir;
    last_artifact_summary_path_ = result.summary_path;
    enrollment_running_ = false;
    if (mock_timer_ != nullptr) {
      mock_timer_->stop();
    }
    start_button_->setEnabled(true);
    user_id_edit_->setEnabled(true);
    user_name_edit_->setEnabled(true);
    review_label_->setText(
        tr("Review: %1 samples captured | artifact=%2")
            .arg(captured_samples_.size())
            .arg(last_artifact_dir_));
    detail_label_->setText(
        tr("Enrollment artifact exported for %1. Summary: %2")
            .arg(EnrollmentIdentitySummary(), last_artifact_summary_path_));
    status_label_->setText(tr("Enrollment completed"));
    preview_->SetOverlayTexts(
        tr("Enrollment complete"),
        tr("Press Start for next user or Exit"),
        false);
    if (auto_close_on_review_) {
      QTimer::singleShot(250, qApp, &QCoreApplication::quit);
    }
    return;
  }

  review_label_->setText(tr("Review: export failed (%1)").arg(result.error_message));
}

}  // namespace sentriface::host
