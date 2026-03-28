#ifndef SENTRIFACE_HOST_QT_ENROLL_APP_PREVIEW_WIDGET_HPP_
#define SENTRIFACE_HOST_QT_ENROLL_APP_PREVIEW_WIDGET_HPP_

#include <QImage>
#include <QString>
#include <QWidget>

#include <optional>

#include "qt_enroll_app/capture_plan.hpp"
#include "qt_enroll_app/guidance_engine.hpp"

namespace sentriface::host {

class PreviewWidget : public QWidget {
  Q_OBJECT

 public:
  explicit PreviewWidget(QWidget* parent = nullptr);

  void SetFaceSample(const FacePoseSample& sample);
  void SetPreviewImage(const QImage& image);
  const QImage& preview_image() const { return preview_image_; }
  void SetMirrorPreview(bool enabled);
  void SetOverlayTexts(const QString& top_text,
                       const QString& bottom_text,
                       bool active);
  void SetCaptureTarget(std::optional<CaptureSlotKind> target,
                        bool collecting);
  void SetDebugVisualsEnabled(bool enabled);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  FacePoseSample sample_ {};
  QImage preview_image_;
  QString overlay_top_text_;
  QString overlay_bottom_text_;
  bool overlay_active_ = false;
  bool mirror_preview_ = false;
  std::optional<CaptureSlotKind> capture_target_;
  bool collection_active_ = false;
  bool debug_visuals_enabled_ = false;
};

}  // namespace sentriface::host

#endif  // SENTRIFACE_HOST_QT_ENROLL_APP_PREVIEW_WIDGET_HPP_
