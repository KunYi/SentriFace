#include "qt_enroll_app/enrollment_artifact.hpp"

#include <cmath>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QtGlobal>
#include <QTextStream>

namespace {

QString SanitizePathToken(const QString& value) {
  QString result = value.trimmed();
  for (int i = 0; i < result.size(); ++i) {
    const QChar ch = result.at(i);
    if (ch.isLetterOrNumber() || ch == QChar('_') || ch == QChar('-')) {
      continue;
    }
    result[i] = QChar('_');
  }
  while (result.contains(QStringLiteral("__"))) {
    result.replace(QStringLiteral("__"), QStringLiteral("_"));
  }
  if (result.isEmpty()) {
    return QStringLiteral("unknown");
  }
  return result;
}

QString FormatFloat(float value) {
  return QString::number(static_cast<double>(value), 'f', 2);
}

QImage MakeFaceCrop(const QImage& preview_image,
                    const sentriface::host::FacePoseSample& sample) {
  if (preview_image.isNull() || !sample.has_face) {
    return QImage();
  }

  const int preview_width = preview_image.width();
  const int preview_height = preview_image.height();
  if (preview_width <= 0 || preview_height <= 0) {
    return QImage();
  }

  const int left = qBound(
      0, static_cast<int>(std::floor(sample.bbox_x)), preview_width - 1);
  const int top = qBound(
      0, static_cast<int>(std::floor(sample.bbox_y)), preview_height - 1);
  const int right = qBound(
      left + 1,
      static_cast<int>(std::ceil(sample.bbox_x + sample.bbox_w)),
      preview_width);
  const int bottom = qBound(
      top + 1,
      static_cast<int>(std::ceil(sample.bbox_y + sample.bbox_h)),
      preview_height);
  const QRect crop_rect(left, top, right - left, bottom - top);
  if (!crop_rect.isValid()) {
    return QImage();
  }
  return preview_image.copy(crop_rect);
}

}  // namespace

namespace sentriface::host {

EnrollmentArtifactResult ExportEnrollmentArtifact(
    const EnrollmentArtifactRequest& request) {
  EnrollmentArtifactResult result;
  if (request.user_id.trimmed().isEmpty()) {
    result.error_message = QStringLiteral("missing_user_id");
    return result;
  }
  if (request.captured_samples.empty()) {
    result.error_message = QStringLiteral("no_captured_samples");
    return result;
  }

  QDir repo_root(QDir::currentPath());
  if (!repo_root.mkpath(QStringLiteral("artifacts/host_qt_enroll"))) {
    result.error_message = QStringLiteral("failed_to_create_artifact_root");
    return result;
  }

  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
  const QString folder_name = QStringLiteral("%1_%2")
                                  .arg(SanitizePathToken(request.user_id), timestamp);
  const QString relative_dir =
      QStringLiteral("artifacts/host_qt_enroll/%1").arg(folder_name);
  if (!repo_root.mkpath(relative_dir)) {
    result.error_message = QStringLiteral("failed_to_create_artifact_dir");
    return result;
  }

  QDir artifact_dir(repo_root.filePath(relative_dir));
  const QString summary_path = artifact_dir.filePath(QStringLiteral("enrollment_summary.txt"));
  QFile summary_file(summary_path);
  if (!summary_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    result.error_message = QStringLiteral("failed_to_open_summary_file");
    return result;
  }

  QTextStream stream(&summary_file);
  stream << "user_id=" << request.user_id << "\n";
  stream << "user_name=" << request.user_name << "\n";
  stream << "source=" << request.source_label << "\n";
  stream << "observation=" << request.observation_label << "\n";
  stream << "capture_plan=" << request.capture_plan_summary << "\n";
  stream << "captured_samples=" << request.captured_samples.size() << "\n";

  int saved_images = 0;
  int saved_crops = 0;
  for (const auto& sample : request.captured_samples) {
    const QString image_name = QStringLiteral("sample_%1_%2_frame.bmp")
                                   .arg(sample.sample_index, 2, 10, QChar('0'))
                                   .arg(CapturePlan::SlotLabel(sample.slot_kind));
    const QString crop_name = QStringLiteral("sample_%1_%2_face.bmp")
                                  .arg(sample.sample_index, 2, 10, QChar('0'))
                                  .arg(CapturePlan::SlotLabel(sample.slot_kind));
    const QString image_path = artifact_dir.filePath(image_name);
    const QString crop_path = artifact_dir.filePath(crop_name);
    bool image_saved = false;
    bool crop_saved = false;
    if (!sample.preview_image.isNull()) {
      image_saved = sample.preview_image.save(image_path, "BMP");
      if (image_saved) {
        saved_images += 1;
      }
    }
    const QImage face_crop = MakeFaceCrop(sample.preview_image, sample.face_sample);
    if (!face_crop.isNull()) {
      crop_saved = face_crop.save(crop_path, "BMP");
      if (crop_saved) {
        saved_crops += 1;
      }
    }

    stream << "\n[sample]\n";
    stream << "index=" << sample.sample_index << "\n";
    stream << "slot=" << CapturePlan::SlotLabel(sample.slot_kind) << "\n";
    stream << "elapsed_ms=" << sample.elapsed_ms << "\n";
    stream << "candidate_count=" << sample.candidate_count << "\n";
    stream << "has_face=" << (sample.face_sample.has_face ? 1 : 0) << "\n";
    stream << "bbox_x=" << FormatFloat(sample.face_sample.bbox_x) << "\n";
    stream << "bbox_y=" << FormatFloat(sample.face_sample.bbox_y) << "\n";
    stream << "bbox_w=" << FormatFloat(sample.face_sample.bbox_w) << "\n";
    stream << "bbox_h=" << FormatFloat(sample.face_sample.bbox_h) << "\n";
    stream << "yaw_deg=" << FormatFloat(sample.face_sample.yaw_deg) << "\n";
    stream << "pitch_deg=" << FormatFloat(sample.face_sample.pitch_deg) << "\n";
    stream << "roll_deg=" << FormatFloat(sample.face_sample.roll_deg) << "\n";
    stream << "quality_score=" << FormatFloat(sample.face_sample.quality_score)
           << "\n";
    stream << "sample_weight=" << FormatFloat(sample.sample_weight) << "\n";
    stream << "preview_width=" << sample.preview_image.width() << "\n";
    stream << "preview_height=" << sample.preview_image.height() << "\n";
    stream << "frame_image="
           << (image_saved ? image_name : QStringLiteral("none")) << "\n";
    stream << "face_crop=" << (crop_saved ? crop_name : QStringLiteral("none")) << "\n";
  }

  summary_file.close();
  result.ok = true;
  result.output_dir = artifact_dir.absolutePath();
  result.summary_path = summary_path;
  if (saved_images != static_cast<int>(request.captured_samples.size()) ||
      saved_crops != static_cast<int>(request.captured_samples.size())) {
    result.error_message = QStringLiteral("partial_image_export");
  }
  return result;
}

}  // namespace sentriface::host
