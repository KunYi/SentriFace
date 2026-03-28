#ifndef SENTRIFACE_HOST_QT_ENROLL_APP_MAIN_WINDOW_HPP_
#define SENTRIFACE_HOST_QT_ENROLL_APP_MAIN_WINDOW_HPP_

#include <QMainWindow>
#include <QElapsedTimer>

#include <memory>
#include <optional>

#include <QTimer>

#include "qt_enroll_app/capture_plan.hpp"
#include "qt_enroll_app/enrollment_artifact.hpp"
#include "qt_enroll_app/enrollment_session.hpp"
#include "qt_enroll_app/frame_source.hpp"
#include "qt_enroll_app/guidance_engine.hpp"
#include "qt_enroll_app/observation_source.hpp"
#include "qt_enroll_app/tuning.hpp"

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
QT_END_NAMESPACE

namespace sentriface::host {

class PreviewWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(const PreviewFrameSourceConfig& source_config,
                      const ObservationSourceConfig& observation_config = ObservationSourceConfig {},
                      QWidget* parent = nullptr);
  ~MainWindow() override;

  void SetEnrollmentIdentity(const QString& user_id, const QString& user_name);
  void ScheduleAutoStart();
  void SetAutoCloseOnReview(bool enabled) { auto_close_on_review_ = enabled; }

 private slots:
  void OnStartEnrollment();
  void OnResetEnrollment();
  void OnAdvanceMockSequence();
  void OnStatusRefresh();

 private:
  void ApplyObservation(const FaceObservation& observation);
  void ApplySample(const FacePoseSample& sample);
  bool HasRequiredEnrollmentFields() const;
  QString EnrollmentIdentitySummary() const;
  QString PreviewRuntimeStatusText() const;
  QString ObservationRuntimeStatusText() const;
  void UpdateGuidanceUi(const GuidanceResult& result);
  void UpdateSessionUi(const EnrollmentSessionSnapshot& snapshot);
  void UpdateCapturePlanUi();
  void UpdatePreviewOverlay(const GuidanceResult& result);
  void RecordCapturedSample(CaptureSlotKind slot_kind, const FacePoseSample& sample);
  void ResetCollectionWindow();
  void UpdateCollectionWindow(const FacePoseSample& display_sample,
                              const FacePoseSample& source_sample,
                              const GuidanceResult& result,
                              EnrollmentSessionSnapshot* session_snapshot);
  CapturedEnrollmentSample MakeCapturedSample(CaptureSlotKind slot_kind,
                                              const FacePoseSample& source_sample) const;
  void FinalizeEnrollmentArtifactIfReady();
  FacePoseSample MakeMockSampleForStep(int step) const;
  void ResetEnrollmentWorkflow();
  FacePoseSample MakeUiOrientedSample(const FacePoseSample& sample) const;
  FacePoseSample MakeDisplaySpaceSample(const FacePoseSample& sample) const;

  PreviewWidget* preview_ = nullptr;
  QLabel* source_label_ = nullptr;
  QLabel* preview_status_label_ = nullptr;
  QLabel* observation_status_label_ = nullptr;
  QLabel* session_label_ = nullptr;
  QLabel* capture_plan_label_ = nullptr;
  QLabel* identity_label_ = nullptr;
  QLabel* review_label_ = nullptr;
  QLabel* status_label_ = nullptr;
  QLabel* detail_label_ = nullptr;
  QLineEdit* user_id_edit_ = nullptr;
  QLineEdit* user_name_edit_ = nullptr;
  QPushButton* start_button_ = nullptr;
  QPushButton* reset_button_ = nullptr;
  GuidanceEngine guidance_;
  EnrollmentSessionController session_;
  CapturePlan capture_plan_;
  PreviewFrameSourceConfig source_config_;
  PreviewInputMode source_mode_ = PreviewInputMode::kMock;
  QTimer* mock_timer_ = nullptr;
  QTimer* status_timer_ = nullptr;
  int mock_step_ = 0;
  bool enrollment_running_ = false;
  bool preview_started_ = false;
  bool observation_started_ = false;
  int preview_frame_count_ = 0;
  int observation_count_ = 0;
  int last_preview_frame_elapsed_ms_ = -1;
  int last_observation_elapsed_ms_ = -1;
  bool last_observation_had_face_ = false;
  QString source_description_;
  QString last_artifact_dir_;
  QString last_artifact_summary_path_;
  std::vector<CapturedEnrollmentSample> captured_samples_;
  bool auto_close_on_review_ = false;
  bool debug_ui_enabled_ = false;
  std::unique_ptr<IPreviewFrameSource> frame_source_;
  std::unique_ptr<IObservationSource> observation_source_;
  QElapsedTimer runtime_clock_;
  QString last_source_status_text_;
  QString last_preview_status_text_;
  QString last_observation_status_text_;
  int capture_collection_window_ms_ = tuning::kCaptureCollectionWindowMs;
  bool collection_active_ = false;
  int collection_start_elapsed_ms_ = -1;
  int collection_candidate_count_ = 0;
  CaptureSlotKind collection_slot_kind_ = CaptureSlotKind::kFrontal;
  float collection_best_score_ = -1.0e9f;
  std::optional<CapturedEnrollmentSample> best_collection_sample_;
};

}  // namespace sentriface::host

#endif  // SENTRIFACE_HOST_QT_ENROLL_APP_MAIN_WINDOW_HPP_
