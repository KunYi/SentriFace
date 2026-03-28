#include "qt_enroll_app/preview_widget.hpp"

#include <QFont>
#include <QPainter>

#include "qt_enroll_app/tuning.hpp"

namespace sentriface::host {

namespace {

QRectF MapSourceRectToWidget(const QRectF& source_rect,
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
  const float offset_x =
      (target_size.width() - rendered_width) * 0.5f;
  const float offset_y =
      (target_size.height() - rendered_height) * 0.5f;

  QRectF mapped(offset_x + source_rect.x() * scale,
                offset_y + source_rect.y() * scale,
                source_rect.width() * scale,
                source_rect.height() * scale);
  if (mirror_preview) {
    mapped.moveLeft(target_size.width() - mapped.left() - mapped.width());
  }
  return mapped;
}

QString PoseGuideLabel(CaptureSlotKind slot_kind) {
  switch (slot_kind) {
    case CaptureSlotKind::kFrontal:
      return QObject::tr("Front");
    case CaptureSlotKind::kLeftQuarter:
      return QObject::tr("Left 15 deg");
    case CaptureSlotKind::kRightQuarter:
      return QObject::tr("Right 15 deg");
  }
  return QString();
}

void DrawPoseGuideCard(QPainter& painter,
                       const QRectF& rect,
                       CaptureSlotKind slot_kind,
                       bool active,
                       bool collecting) {
  const QColor border =
      active ? (collecting ? QColor(80, 210, 130) : QColor(230, 190, 80))
             : QColor(90, 100, 110);
  const QColor fill = active ? QColor(16, 24, 30, 230) : QColor(12, 16, 20, 180);

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(border, active ? 3.0 : 1.5));
  painter.setBrush(fill);
  painter.drawRoundedRect(rect, 12.0, 12.0);

  const QPointF center(rect.center().x(), rect.y() + rect.height() * 0.34);
  const float rx = rect.width() * 0.18f;
  const float ry = rect.height() * 0.17f;
  painter.setPen(QPen(QColor(240, 243, 246), 2.5));
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(center, rx, ry);

  const float yaw_sign =
      slot_kind == CaptureSlotKind::kLeftQuarter
          ? -1.0f
          : (slot_kind == CaptureSlotKind::kRightQuarter ? 1.0f : 0.0f);
  if (yaw_sign != 0.0f) {
    const float arrow_y = rect.y() + rect.height() * 0.62f;
    const float arrow_left = rect.x() + rect.width() * 0.25f;
    const float arrow_right = rect.x() + rect.width() * 0.75f;
    if (yaw_sign < 0.0f) {
      painter.drawLine(QPointF(arrow_right, arrow_y), QPointF(arrow_left, arrow_y));
      painter.drawLine(QPointF(arrow_left, arrow_y),
                       QPointF(arrow_left + 8.0f, arrow_y - 8.0f));
      painter.drawLine(QPointF(arrow_left, arrow_y),
                       QPointF(arrow_left + 8.0f, arrow_y + 8.0f));
    } else {
      painter.drawLine(QPointF(arrow_left, arrow_y), QPointF(arrow_right, arrow_y));
      painter.drawLine(QPointF(arrow_right, arrow_y),
                       QPointF(arrow_right - 8.0f, arrow_y - 8.0f));
      painter.drawLine(QPointF(arrow_right, arrow_y),
                       QPointF(arrow_right - 8.0f, arrow_y + 8.0f));
    }
  } else {
    painter.drawLine(QPointF(center.x(), center.y() + ry + 4.0f),
                     QPointF(center.x(), rect.y() + rect.height() * 0.64f));
  }

  QFont font = painter.font();
  font.setPointSize(10);
  font.setBold(true);
  painter.setFont(font);
  painter.setPen(QPen(QColor(250, 252, 255), 1.0));
  painter.drawText(rect.adjusted(6.0, rect.height() * 0.70f, -6.0, -8.0),
                   Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                   PoseGuideLabel(slot_kind));
  painter.restore();
}

}  // namespace

PreviewWidget::PreviewWidget(QWidget* parent)
    : QWidget(parent) {
  setAutoFillBackground(true);
}

void PreviewWidget::SetFaceSample(const FacePoseSample& sample) {
  sample_ = sample;
  update();
}

void PreviewWidget::SetPreviewImage(const QImage& image) {
  preview_image_ = image;
  update();
}

void PreviewWidget::SetMirrorPreview(bool enabled) {
  mirror_preview_ = enabled;
  update();
}

void PreviewWidget::SetOverlayTexts(const QString& top_text,
                                    const QString& bottom_text,
                                    bool active) {
  overlay_top_text_ = top_text;
  overlay_bottom_text_ = bottom_text;
  overlay_active_ = active;
  update();
}

void PreviewWidget::SetCaptureTarget(std::optional<CaptureSlotKind> target,
                                     bool collecting) {
  capture_target_ = target;
  collection_active_ = collecting;
  update();
}

void PreviewWidget::SetDebugVisualsEnabled(bool enabled) {
  debug_visuals_enabled_ = enabled;
  update();
}

void PreviewWidget::paintEvent(QPaintEvent* event) {
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(rect(), QColor(18, 22, 27));

  if (!preview_image_.isNull()) {
    const QImage display_image =
        mirror_preview_ ? preview_image_.mirrored(true, false) : preview_image_;
    const QImage scaled =
        display_image.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int x = (scaled.width() - width()) / 2;
    const int y = (scaled.height() - height()) / 2;
    painter.drawImage(rect(), scaled, QRect(x, y, width(), height()));
  } else {
    painter.setPen(QPen(QColor(185, 190, 196), 1.0));
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("Preview placeholder\nWebCAM-first validation path"));
  }

  const QRectF virtual_ellipse(width() * tuning::kEllipseXRatio,
                               height() * tuning::kEllipseYRatio,
                               width() * tuning::kEllipseWRatio,
                               height() * tuning::kEllipseHRatio);
  painter.setPen(QPen(overlay_active_ ? QColor(80, 210, 130)
                                      : QColor(210, 150, 80),
                       3.0));
  painter.drawEllipse(virtual_ellipse);

  if (debug_visuals_enabled_ && sample_.has_face) {
    const QRectF source_bbox(sample_.bbox_x, sample_.bbox_y, sample_.bbox_w, sample_.bbox_h);
    const QRectF bbox = MapSourceRectToWidget(
        source_bbox, preview_image_.size(), size(), mirror_preview_);
    painter.setPen(QPen(QColor(230, 190, 80), 3.0));
    painter.drawRoundedRect(bbox, 12.0, 12.0);
    painter.setPen(QPen(QColor(130, 140, 150), 1.0, Qt::DashLine));
    painter.drawLine(QPointF(width() * 0.5, 0.0),
                     QPointF(width() * 0.5, static_cast<float>(height())));
    painter.drawLine(QPointF(0.0, height() * 0.5),
                     QPointF(static_cast<float>(width()), height() * 0.5));
  }

  if (!overlay_top_text_.isEmpty()) {
    const QRectF top_banner(width() * 0.12, 20.0, width() * 0.76, 72.0);
    painter.fillRect(top_banner, QColor(8, 12, 18, 215));
    QFont top_font = painter.font();
    top_font.setPointSize(std::max(14, width() / 48));
    top_font.setBold(true);
    painter.setFont(top_font);
    const QRectF text_rect = top_banner.adjusted(18.0, 0.0, -18.0, 0.0);
    painter.setPen(QPen(QColor(0, 0, 0, 200), 1.0));
    painter.drawText(text_rect.translated(2.0, 2.0),
                     Qt::AlignCenter,
                     overlay_top_text_);
    painter.setPen(QPen(QColor(250, 252, 255), 1.0));
    painter.drawText(text_rect,
                     Qt::AlignCenter,
                     overlay_top_text_);
  }

  const float guide_title_w = 152.0f;
  const float guide_title_h = 30.0f;
  const float guide_panel_width = 372.0f;
  const float guide_panel_x = width() * 0.5f - guide_panel_width * 0.5f;
  const float guide_cards_y =
      std::min(height() - 118.0f,
               static_cast<float>(virtual_ellipse.bottom()) + 18.0f);
  const QRectF guide_title_rect(width() * 0.5f - guide_title_w * 0.5f,
                                guide_cards_y - 34.0f,
                                guide_title_w,
                                guide_title_h);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(8, 12, 18, 205));
  painter.drawRoundedRect(guide_title_rect, 12.0, 12.0);
  QFont panel_font = painter.font();
  panel_font.setPointSize(12);
  panel_font.setBold(true);
  painter.setFont(panel_font);
  painter.setPen(QPen(QColor(250, 252, 255), 1.0));
  painter.drawText(guide_title_rect,
                   Qt::AlignCenter,
                   tr("Pose guide"));

  const float card_y = guide_cards_y;
  const float card_w = 108.0f;
  const float card_h = 88.0f;
  const float gap = 12.0f;
  const QRectF front_rect(guide_panel_x + 8.0f, card_y, card_w, card_h);
  const QRectF left_rect(front_rect.right() + gap, card_y, card_w, card_h);
  const QRectF right_rect(left_rect.right() + gap, card_y, card_w, card_h);

  DrawPoseGuideCard(painter, front_rect, CaptureSlotKind::kFrontal,
                    capture_target_.has_value() &&
                        *capture_target_ == CaptureSlotKind::kFrontal,
                    collection_active_);
  DrawPoseGuideCard(painter, left_rect, CaptureSlotKind::kLeftQuarter,
                    capture_target_.has_value() &&
                        *capture_target_ == CaptureSlotKind::kLeftQuarter,
                    collection_active_);
  DrawPoseGuideCard(painter, right_rect, CaptureSlotKind::kRightQuarter,
                    capture_target_.has_value() &&
                        *capture_target_ == CaptureSlotKind::kRightQuarter,
                    collection_active_);
}

}  // namespace sentriface::host
