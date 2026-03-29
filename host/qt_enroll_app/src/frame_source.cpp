#include "qt_enroll_app/frame_source.hpp"

#define SENTRIFACE_LOG_TAG "host_preview"

#include <cstdlib>
#include <cstring>

#ifdef SENTRIFACE_HOST_HAS_QT_MULTIMEDIA
#include <QAbstractVideoBuffer>
#include <QCamera>
#include <QCameraInfo>
#include <QCameraViewfinderSettings>
#include <QVideoFrame>
#include <QVideoProbe>
#endif
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QPainter>
#include <QProcess>

#include "logging/system_logger.hpp"

namespace sentriface::host {

namespace {

sentriface::logging::SystemLogger& GetHostPreviewLogger() {
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

bool StartsWith(const char* value, const char* prefix) {
  return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

bool PreferQtMultimediaWebcamBackend() {
  const char* value = std::getenv("SENTRIFACE_HOST_QT_ENROLL_WEBCAM_BACKEND");
  return value != nullptr && std::strcmp(value, "qt") == 0;
}

QImage MakeMockPreviewImage(int width, int height, int frame_index) {
  const int image_width = std::max(320, width);
  const int image_height = std::max(240, height);
  QImage image(image_width, image_height, QImage::Format_RGB32);
  image.fill(QColor(24, 28, 36));

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QLinearGradient background(0.0, 0.0, image_width, image_height);
  background.setColorAt(0.0, QColor(32, 44, 58));
  background.setColorAt(1.0, QColor(18, 24, 30));
  painter.fillRect(image.rect(), background);

  const float shift =
      static_cast<float>((frame_index % 60) - 30) / 30.0f;
  const QRectF face_oval(image_width * (0.50f + shift * 0.04f) - image_width * 0.11f,
                         image_height * 0.38f - image_height * 0.19f,
                         image_width * 0.22f,
                         image_height * 0.38f);
  painter.setBrush(QColor(220, 188, 156));
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(face_oval);

  painter.setBrush(QColor(42, 34, 30));
  painter.drawEllipse(QRectF(face_oval.center().x() - face_oval.width() * 0.22f,
                             face_oval.center().y() - face_oval.height() * 0.12f,
                             face_oval.width() * 0.10f,
                             face_oval.height() * 0.06f));
  painter.drawEllipse(QRectF(face_oval.center().x() + face_oval.width() * 0.12f,
                             face_oval.center().y() - face_oval.height() * 0.12f,
                             face_oval.width() * 0.10f,
                             face_oval.height() * 0.06f));
  painter.setPen(QPen(QColor(120, 68, 58), 2.0));
  painter.drawArc(QRectF(face_oval.center().x() - face_oval.width() * 0.18f,
                         face_oval.center().y() + face_oval.height() * 0.05f,
                         face_oval.width() * 0.36f,
                         face_oval.height() * 0.18f),
                  200 * 16,
                  140 * 16);

  painter.setPen(QPen(QColor(245, 247, 250), 2.0));
  painter.drawText(QRectF(24.0, 18.0, image_width - 48.0, 40.0),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Mock enrollment preview"));
  painter.setPen(QPen(QColor(180, 190, 205), 1.0));
  painter.drawText(QRectF(24.0, image_height - 54.0, image_width - 48.0, 30.0),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("frame=%1").arg(frame_index));
  return image;
}

QString WebcamDevicePath(int webcam_index) {
  return QStringLiteral("/dev/video%1").arg(webcam_index);
}

int FindJpegStart(const QByteArray& buffer) {
  for (int index = 0; index + 1 < buffer.size(); ++index) {
    if (static_cast<unsigned char>(buffer[index]) == 0xff &&
        static_cast<unsigned char>(buffer[index + 1]) == 0xd8) {
      return index;
    }
  }
  return -1;
}

int FindJpegEnd(const QByteArray& buffer, int start_index) {
  for (int index = start_index; index + 1 < buffer.size(); ++index) {
    if (static_cast<unsigned char>(buffer[index]) == 0xff &&
        static_cast<unsigned char>(buffer[index + 1]) == 0xd9) {
      return index + 2;
    }
  }
  return -1;
}

#ifdef SENTRIFACE_HOST_HAS_QT_MULTIMEDIA
const char* PixelFormatName(QVideoFrame::PixelFormat format) {
  switch (format) {
    case QVideoFrame::Format_Invalid:
      return "invalid";
    case QVideoFrame::Format_ARGB32:
      return "argb32";
    case QVideoFrame::Format_RGB32:
      return "rgb32";
    case QVideoFrame::Format_RGB24:
      return "rgb24";
    case QVideoFrame::Format_BGR24:
      return "bgr24";
    case QVideoFrame::Format_YUYV:
      return "yuyv";
    case QVideoFrame::Format_UYVY:
      return "uyvy";
    case QVideoFrame::Format_NV12:
      return "nv12";
    case QVideoFrame::Format_NV21:
      return "nv21";
    case QVideoFrame::Format_Jpeg:
      return "jpeg";
    default:
      return "other";
  }
}

const char* CameraStatusName(QCamera::Status status) {
  switch (status) {
    case QCamera::UnavailableStatus:
      return "unavailable";
    case QCamera::UnloadedStatus:
      return "unloaded";
    case QCamera::LoadingStatus:
      return "loading";
    case QCamera::UnloadingStatus:
      return "unloading";
    case QCamera::LoadedStatus:
      return "loaded";
    case QCamera::StandbyStatus:
      return "standby";
    case QCamera::StartingStatus:
      return "starting";
    case QCamera::StoppingStatus:
      return "stopping";
    case QCamera::ActiveStatus:
      return "active";
  }
  return "unknown";
}

const char* CameraErrorName(QCamera::Error error) {
  switch (error) {
    case QCamera::NoError:
      return "no_error";
    case QCamera::CameraError:
      return "camera_error";
    case QCamera::InvalidRequestError:
      return "invalid_request";
    case QCamera::ServiceMissingError:
      return "service_missing";
    case QCamera::NotSupportedFeatureError:
      return "not_supported_feature";
  }
  return "unknown";
}

QVideoFrame::PixelFormat ChoosePreferredQtPixelFormat(
    const QList<QVideoFrame::PixelFormat>& formats) {
  const QVideoFrame::PixelFormat preferences[] = {
      QVideoFrame::Format_RGB32,
      QVideoFrame::Format_ARGB32,
      QVideoFrame::Format_RGB24,
      QVideoFrame::Format_BGR24,
      QVideoFrame::Format_Jpeg,
  };
  for (const auto preferred : preferences) {
    if (formats.contains(preferred)) {
      return preferred;
    }
  }
  return QVideoFrame::Format_Invalid;
}

QImage VideoFrameToImage(const QVideoFrame& frame) {
  QVideoFrame copy(frame);
  if (!copy.isValid() || !copy.map(QAbstractVideoBuffer::ReadOnly)) {
    return QImage();
  }

  if (copy.pixelFormat() == QVideoFrame::Format_Jpeg) {
    QImage image;
    image.loadFromData(copy.bits(), copy.mappedBytes(), "JPEG");
    copy.unmap();
    return image;
  }

  const QImage::Format format =
      QVideoFrame::imageFormatFromPixelFormat(copy.pixelFormat());
  if (format == QImage::Format_Invalid) {
    copy.unmap();
    return QImage();
  }

  const QImage image(
      copy.bits(),
      copy.width(),
      copy.height(),
      copy.bytesPerLine(),
      format);
  const QImage result = image.copy();
  copy.unmap();
  return result;
}
#endif

}  // namespace

class LocalWebcamPreviewFrameSource::Impl {
 public:
  std::unique_ptr<QProcess> ffmpeg_process;
  QByteArray jpeg_buffer;
  QString backend = QStringLiteral("none");
  QString last_error = QStringLiteral("idle");
  int decoded_frames = 0;
  int dropped_jpegs = 0;
  int stdout_bytes = 0;
#ifdef SENTRIFACE_HOST_HAS_QT_MULTIMEDIA
  std::unique_ptr<QCamera> camera;
  std::unique_ptr<QVideoProbe> probe;
  QCamera::Status qt_status = QCamera::UnloadedStatus;
  QCamera::Error qt_error = QCamera::NoError;
  QString qt_error_string;
#endif
};

LocalWebcamPreviewFrameSource::LocalWebcamPreviewFrameSource(
    const PreviewFrameSourceConfig& config)
    : config_(config),
      impl_(std::make_unique<Impl>()) {
#if defined(Q_OS_LINUX) || defined(SENTRIFACE_HOST_HAS_QT_MULTIMEDIA)
  supported_ = true;
#else
  supported_ = false;
#endif
}

LocalWebcamPreviewFrameSource::~LocalWebcamPreviewFrameSource() = default;

bool LocalWebcamPreviewFrameSource::Start() {
  if (!supported_) {
    running_ = false;
    SENTRIFACE_LOGW(GetHostPreviewLogger(), "local_webcam_unsupported");
    return false;
  }
  if (impl_) {
    impl_->decoded_frames = 0;
    impl_->dropped_jpegs = 0;
    impl_->stdout_bytes = 0;
    impl_->jpeg_buffer.clear();
    impl_->backend = QStringLiteral("none");
    impl_->last_error = QStringLiteral("starting");
  }
#if defined(Q_OS_LINUX)
  const bool prefer_qt_backend = PreferQtMultimediaWebcamBackend();
  if (!prefer_qt_backend && impl_) {
    impl_->last_error = QStringLiteral("starting_ffmpeg_primary");
    impl_->ffmpeg_process = std::make_unique<QProcess>();
    impl_->ffmpeg_process->setProcessChannelMode(QProcess::SeparateChannels);

    QObject::connect(
        impl_->ffmpeg_process.get(),
        &QProcess::readyReadStandardOutput,
        [this]() {
          if (!impl_ || !impl_->ffmpeg_process) {
            return;
          }
          const QByteArray chunk =
              impl_->ffmpeg_process->readAllStandardOutput();
          impl_->stdout_bytes += chunk.size();
          impl_->jpeg_buffer.append(chunk);
          while (true) {
            const int start = FindJpegStart(impl_->jpeg_buffer);
            if (start < 0) {
              if (impl_->jpeg_buffer.size() > (1 << 20)) {
                impl_->jpeg_buffer.clear();
                impl_->dropped_jpegs += 1;
                impl_->last_error = QStringLiteral("jpeg_buffer_reset");
              }
              break;
            }
            const int end = FindJpegEnd(impl_->jpeg_buffer, start);
            if (end < 0) {
              if (start > 0) {
                impl_->jpeg_buffer.remove(0, start);
              }
              break;
            }
            const QByteArray jpeg = impl_->jpeg_buffer.mid(start, end - start);
            impl_->jpeg_buffer.remove(0, end);
            if (!callback_) {
              continue;
            }
            QImage image;
            image.loadFromData(jpeg, "JPEG");
            if (!image.isNull()) {
              impl_->decoded_frames += 1;
              impl_->last_error = QStringLiteral("ok");
              if ((impl_->decoded_frames % 30) == 0) {
                SENTRIFACE_LOGD(GetHostPreviewLogger(),
                             "webcam_frame backend=ffmpeg frames="
                                 << impl_->decoded_frames
                                 << " bytes=" << impl_->stdout_bytes
                                 << " drops=" << impl_->dropped_jpegs);
              }
              callback_(image);
            } else {
              impl_->dropped_jpegs += 1;
              impl_->last_error = QStringLiteral("jpeg_decode_failed");
              SENTRIFACE_LOGW(GetHostPreviewLogger(),
                           "webcam_frame_decode_failed backend=ffmpeg drops="
                               << impl_->dropped_jpegs);
            }
          }
        });
    QObject::connect(
        impl_->ffmpeg_process.get(),
        &QProcess::readyReadStandardError,
        [this]() {
          if (!impl_ || !impl_->ffmpeg_process) {
            return;
          }
          const QString stderr_text = QString::fromLocal8Bit(
              impl_->ffmpeg_process->readAllStandardError()).trimmed();
          if (!stderr_text.isEmpty()) {
            impl_->last_error = stderr_text.section('\n', -1);
            SENTRIFACE_LOGW(GetHostPreviewLogger(),
                         "webcam_backend_stderr backend=ffmpeg msg="
                             << impl_->last_error.toStdString());
          }
        });

    QStringList args;
    args << QStringLiteral("-hide_banner")
         << QStringLiteral("-loglevel") << QStringLiteral("error")
         << QStringLiteral("-f") << QStringLiteral("video4linux2")
         << QStringLiteral("-framerate") << QStringLiteral("15")
         << QStringLiteral("-video_size")
         << QStringLiteral("%1x%2").arg(config_.width).arg(config_.height)
         << QStringLiteral("-input_format") << QStringLiteral("mjpeg")
         << QStringLiteral("-i") << WebcamDevicePath(config_.webcam_index)
         << QStringLiteral("-f") << QStringLiteral("image2pipe")
         << QStringLiteral("-vcodec") << QStringLiteral("mjpeg")
         << QStringLiteral("-");
    SENTRIFACE_LOGI(GetHostPreviewLogger(),
                 "webcam_start_try backend=ffmpeg device="
                     << WebcamDevicePath(config_.webcam_index).toStdString()
                     << " size=" << config_.width << "x" << config_.height);
    impl_->ffmpeg_process->start(QStringLiteral("ffmpeg"), args);
    if (impl_->ffmpeg_process->waitForStarted(1200)) {
      impl_->backend = QStringLiteral("ffmpeg");
      impl_->last_error = QStringLiteral("ffmpeg_started");
      running_ = true;
      SENTRIFACE_LOGI(GetHostPreviewLogger(), "webcam_start_ok backend=ffmpeg");
      return true;
    }
    impl_->last_error = QStringLiteral("ffmpeg_start_failed");
    SENTRIFACE_LOGW(GetHostPreviewLogger(), "webcam_start_failed backend=ffmpeg");
    impl_->ffmpeg_process.reset();
  }
#endif
#ifdef SENTRIFACE_HOST_HAS_QT_MULTIMEDIA
  SENTRIFACE_LOGI(GetHostPreviewLogger(),
               "webcam_start_try backend=qt_multimedia index="
                   << config_.webcam_index
                   << " size=" << config_.width << "x" << config_.height);
  const QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  if (config_.webcam_index < 0 || config_.webcam_index >= cameras.size()) {
    if (impl_) {
      impl_->last_error = QStringLiteral("qt_camera_index_invalid");
    }
    SENTRIFACE_LOGW(GetHostPreviewLogger(),
                 "webcam_start_failed backend=qt_multimedia reason=index_invalid");
  } else {
    impl_->camera = std::make_unique<QCamera>(cameras.at(config_.webcam_index));
    QObject::connect(
        impl_->camera.get(),
        &QCamera::statusChanged,
        [this](QCamera::Status status) {
          if (!impl_) {
            return;
          }
          impl_->qt_status = status;
          SENTRIFACE_LOGI(GetHostPreviewLogger(),
                       "webcam_qt_status status=" << CameraStatusName(status));
        });
    QObject::connect(
        impl_->camera.get(),
        &QCamera::errorOccurred,
        [this](QCamera::Error error) {
          if (!impl_ || !impl_->camera) {
            return;
          }
          impl_->qt_error = error;
          impl_->qt_error_string = impl_->camera->errorString();
          impl_->last_error =
              QStringLiteral("qt_camera_error:%1")
                  .arg(QString::fromUtf8(CameraErrorName(error)));
          SENTRIFACE_LOGW(GetHostPreviewLogger(),
                       "webcam_qt_error error=" << CameraErrorName(error)
                           << " msg=" << impl_->qt_error_string.toStdString());
        });
    impl_->camera->load();
    {
      QElapsedTimer timer;
      timer.start();
      while (timer.elapsed() < 800 &&
             impl_->qt_error == QCamera::NoError &&
             impl_->qt_status != QCamera::LoadedStatus &&
             impl_->qt_status != QCamera::ActiveStatus &&
             impl_->qt_status != QCamera::StandbyStatus) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
      }
    }
    QCameraViewfinderSettings settings;
    settings.setResolution(config_.width, config_.height);
    settings.setMinimumFrameRate(15.0);
    settings.setMaximumFrameRate(15.0);
    const auto supported_formats =
        impl_->camera->supportedViewfinderPixelFormats(settings);
    const auto supported_resolutions =
        impl_->camera->supportedViewfinderResolutions(settings);
    const auto preferred_format = ChoosePreferredQtPixelFormat(supported_formats);
    if (preferred_format != QVideoFrame::Format_Invalid) {
      settings.setPixelFormat(preferred_format);
    }
    impl_->camera->setViewfinderSettings(settings);
    SENTRIFACE_LOGI(GetHostPreviewLogger(),
                 "webcam_qt_viewfinder preferred_format="
                     << PixelFormatName(preferred_format)
                     << " supported_count=" << supported_formats.size()
                     << " resolution_count=" << supported_resolutions.size()
                     << " status=" << CameraStatusName(impl_->qt_status));
    impl_->probe = std::make_unique<QVideoProbe>();
    const bool probe_ok = impl_->probe->setSource(impl_->camera.get());
    if (!probe_ok) {
      impl_->probe.reset();
      impl_->camera.reset();
      impl_->last_error = QStringLiteral("qt_video_probe_failed");
      SENTRIFACE_LOGW(GetHostPreviewLogger(),
                   "webcam_start_failed backend=qt_multimedia reason=probe");
    } else {
      QObject::connect(
          impl_->probe.get(),
          &QVideoProbe::videoFrameProbed,
          [this](const QVideoFrame& frame) {
            if (!callback_) {
              return;
            }
            const QImage image = VideoFrameToImage(frame);
            if (!image.isNull()) {
              if (impl_) {
                impl_->decoded_frames += 1;
                impl_->last_error = QStringLiteral("ok");
                if ((impl_->decoded_frames % 30) == 0) {
                  SENTRIFACE_LOGD(GetHostPreviewLogger(),
                               "webcam_frame backend=qt_multimedia frames="
                                   << impl_->decoded_frames);
                }
              }
              callback_(image);
            } else if (impl_) {
              impl_->dropped_jpegs += 1;
              impl_->last_error = QStringLiteral("qt_frame_to_image_failed");
              SENTRIFACE_LOGW(GetHostPreviewLogger(),
                           "webcam_frame_decode_failed backend=qt_multimedia drops="
                               << impl_->dropped_jpegs
                               << " format=" << PixelFormatName(frame.pixelFormat())
                               << " bytes=" << frame.mappedBytes());
            }
          });

      impl_->camera->start();
      {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 2000 &&
               impl_->decoded_frames == 0 &&
               impl_->qt_error == QCamera::NoError) {
          QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
      }
      if (impl_->decoded_frames > 0) {
        impl_->backend = QStringLiteral("qt_multimedia");
        impl_->last_error = QStringLiteral("qt_camera_started");
        running_ = true;
        SENTRIFACE_LOGI(GetHostPreviewLogger(),
                     "webcam_start_ok backend=qt_multimedia");
        return true;
      }
      impl_->last_error = impl_->qt_error == QCamera::NoError
          ? QStringLiteral("qt_no_frames_after_start")
          : impl_->last_error;
      SENTRIFACE_LOGW(GetHostPreviewLogger(),
                   "webcam_start_failed backend=qt_multimedia reason=no_frames"
                       << " status=" << CameraStatusName(impl_->qt_status)
                       << " error=" << CameraErrorName(impl_->qt_error)
                       << " msg=" << impl_->qt_error_string.toStdString());
      impl_->probe.reset();
      impl_->camera->stop();
      impl_->camera.reset();
    }
  }
#endif

#if defined(Q_OS_LINUX)
  if (impl_) {
    impl_->last_error = QStringLiteral("starting_ffmpeg_fallback");
    impl_->ffmpeg_process = std::make_unique<QProcess>();
    impl_->ffmpeg_process->setProcessChannelMode(QProcess::SeparateChannels);

    QObject::connect(
        impl_->ffmpeg_process.get(),
        &QProcess::readyReadStandardOutput,
        [this]() {
          if (!impl_ || !impl_->ffmpeg_process) {
            return;
          }
          const QByteArray chunk =
              impl_->ffmpeg_process->readAllStandardOutput();
          impl_->stdout_bytes += chunk.size();
          impl_->jpeg_buffer.append(chunk);
          while (true) {
            const int start = FindJpegStart(impl_->jpeg_buffer);
            if (start < 0) {
              if (impl_->jpeg_buffer.size() > (1 << 20)) {
                impl_->jpeg_buffer.clear();
                impl_->dropped_jpegs += 1;
                impl_->last_error = QStringLiteral("jpeg_buffer_reset");
              }
              break;
            }
            const int end = FindJpegEnd(impl_->jpeg_buffer, start);
            if (end < 0) {
              if (start > 0) {
                impl_->jpeg_buffer.remove(0, start);
              }
              break;
            }
            const QByteArray jpeg = impl_->jpeg_buffer.mid(start, end - start);
            impl_->jpeg_buffer.remove(0, end);
            if (!callback_) {
              continue;
            }
            QImage image;
            image.loadFromData(jpeg, "JPEG");
            if (!image.isNull()) {
              impl_->decoded_frames += 1;
              impl_->last_error = QStringLiteral("ok");
              if ((impl_->decoded_frames % 30) == 0) {
                SENTRIFACE_LOGD(GetHostPreviewLogger(),
                             "webcam_frame backend=ffmpeg frames="
                                 << impl_->decoded_frames
                                 << " bytes=" << impl_->stdout_bytes
                                 << " drops=" << impl_->dropped_jpegs);
              }
              callback_(image);
            } else {
              impl_->dropped_jpegs += 1;
              impl_->last_error = QStringLiteral("jpeg_decode_failed");
              SENTRIFACE_LOGW(GetHostPreviewLogger(),
                           "webcam_frame_decode_failed backend=ffmpeg drops="
                               << impl_->dropped_jpegs);
            }
          }
        });
    QObject::connect(
        impl_->ffmpeg_process.get(),
        &QProcess::readyReadStandardError,
        [this]() {
          if (!impl_ || !impl_->ffmpeg_process) {
            return;
          }
          const QString stderr_text = QString::fromLocal8Bit(
              impl_->ffmpeg_process->readAllStandardError()).trimmed();
          if (!stderr_text.isEmpty()) {
            impl_->last_error = stderr_text.section('\n', -1);
            SENTRIFACE_LOGW(GetHostPreviewLogger(),
                         "webcam_backend_stderr backend=ffmpeg msg="
                             << impl_->last_error.toStdString());
          }
        });

    QStringList args;
    args << QStringLiteral("-hide_banner")
         << QStringLiteral("-loglevel") << QStringLiteral("error")
         << QStringLiteral("-f") << QStringLiteral("video4linux2")
         << QStringLiteral("-framerate") << QStringLiteral("15")
         << QStringLiteral("-video_size")
         << QStringLiteral("%1x%2").arg(config_.width).arg(config_.height)
         << QStringLiteral("-input_format") << QStringLiteral("mjpeg")
         << QStringLiteral("-i") << WebcamDevicePath(config_.webcam_index)
         << QStringLiteral("-f") << QStringLiteral("image2pipe")
         << QStringLiteral("-vcodec") << QStringLiteral("mjpeg")
         << QStringLiteral("-");
    SENTRIFACE_LOGI(GetHostPreviewLogger(),
                 "webcam_start_try backend=ffmpeg device="
                     << WebcamDevicePath(config_.webcam_index).toStdString()
                     << " size=" << config_.width << "x" << config_.height);
    impl_->ffmpeg_process->start(QStringLiteral("ffmpeg"), args);
    if (impl_->ffmpeg_process->waitForStarted(1200)) {
      impl_->backend = QStringLiteral("ffmpeg");
      impl_->last_error = QStringLiteral("ffmpeg_started");
      running_ = true;
      SENTRIFACE_LOGI(GetHostPreviewLogger(), "webcam_start_ok backend=ffmpeg");
      return true;
    }
    impl_->last_error = QStringLiteral("ffmpeg_start_failed");
    SENTRIFACE_LOGW(GetHostPreviewLogger(), "webcam_start_failed backend=ffmpeg");
    impl_->ffmpeg_process.reset();
  }
#endif

  SENTRIFACE_LOGW(GetHostPreviewLogger(), "webcam_start_failed backend=none");
  running_ = false;
  return false;
}

void LocalWebcamPreviewFrameSource::Stop() {
  SENTRIFACE_LOGI(GetHostPreviewLogger(),
               "webcam_stop backend="
                   << (impl_ ? impl_->backend.toStdString() : std::string("none"))
                   << " frames=" << (impl_ ? impl_->decoded_frames : 0)
                   << " drops=" << (impl_ ? impl_->dropped_jpegs : 0)
                   << " bytes=" << (impl_ ? impl_->stdout_bytes : 0)
                   << " last="
                   << (impl_ ? impl_->last_error.toStdString() : std::string("none")));
  if (impl_ && impl_->ffmpeg_process) {
    impl_->ffmpeg_process->kill();
    impl_->ffmpeg_process->waitForFinished(500);
    impl_->ffmpeg_process.reset();
  }
#ifdef SENTRIFACE_HOST_HAS_QT_MULTIMEDIA
  if (impl_ && impl_->camera) {
    impl_->camera->stop();
  }
  if (impl_) {
    impl_->probe.reset();
    impl_->camera.reset();
  }
#endif
  running_ = false;
}

QString LocalWebcamPreviewFrameSource::Description() const {
  if (!supported_) {
    return QStringLiteral("local_webcam:unsupported_without_qt_multimedia");
  }
  const QString state = running_ ? QStringLiteral("running") : QStringLiteral("stopped");
  const QString backend = impl_ ? impl_->backend : QStringLiteral("none");
  const QString last_error = impl_ ? impl_->last_error : QStringLiteral("none");
  const int decoded_frames = impl_ ? impl_->decoded_frames : 0;
  const int dropped_jpegs = impl_ ? impl_->dropped_jpegs : 0;
  const int stdout_bytes = impl_ ? impl_->stdout_bytes : 0;
  return QStringLiteral("local_webcam:%1:%2:index=%3:%4x%5:frames=%6:drops=%7:bytes=%8:last=%9")
      .arg(state)
      .arg(backend)
      .arg(config_.webcam_index)
      .arg(config_.width)
      .arg(config_.height)
      .arg(decoded_frames)
      .arg(dropped_jpegs)
      .arg(stdout_bytes)
      .arg(last_error);
}

PreviewInputMode LocalWebcamPreviewFrameSource::mode() const {
  return PreviewInputMode::kLocalWebcam;
}

bool LocalWebcamPreviewFrameSource::IsSupported() const {
  return supported_;
}

void LocalWebcamPreviewFrameSource::SetFrameCallback(FrameCallback callback) {
  callback_ = std::move(callback);
}

MockPreviewFrameSource::MockPreviewFrameSource(
    const PreviewFrameSourceConfig& config)
    : config_(config) {}

bool MockPreviewFrameSource::Start() {
  running_ = true;
  frame_index_ = 0;
  if (!timer_) {
    timer_ = std::make_unique<QTimer>();
    timer_->setInterval(100);
    QObject::connect(timer_.get(), &QTimer::timeout, [this]() {
      if (!running_ || !callback_) {
        return;
      }
      frame_index_ += 1;
      callback_(MakeMockPreviewImage(config_.width, config_.height, frame_index_));
    });
  }
  timer_->start();
  if (callback_) {
    callback_(MakeMockPreviewImage(config_.width, config_.height, frame_index_));
  }
  return true;
}

void MockPreviewFrameSource::Stop() {
  running_ = false;
  if (timer_) {
    timer_->stop();
  }
}

void MockPreviewFrameSource::SetFrameCallback(FrameCallback callback) {
  callback_ = std::move(callback);
}

QString MockPreviewFrameSource::Description() const {
  const QString state = running_ ? QStringLiteral("running") : QStringLiteral("stopped");
  return QStringLiteral("mock_preview_source:%1:%2x%3")
      .arg(state)
      .arg(config_.width)
      .arg(config_.height);
}

PreviewInputMode MockPreviewFrameSource::mode() const {
  return PreviewInputMode::kMock;
}

PreviewFrameSourceConfig LoadPreviewFrameSourceConfigFromArgs(int argc, char* argv[]) {
  PreviewFrameSourceConfig config;
  bool mirror_override = false;
  bool guidance_override = false;
  for (int index = 1; index < argc; ++index) {
    const char* arg = argv[index];
    if (StartsWith(arg, "--input=")) {
      const char* value = arg + std::strlen("--input=");
      if (std::strcmp(value, "webcam") == 0) {
        config.mode = PreviewInputMode::kLocalWebcam;
      } else if (std::strcmp(value, "rtsp") == 0) {
        config.mode = PreviewInputMode::kRtsp;
      } else {
        config.mode = PreviewInputMode::kMock;
      }
    } else if (StartsWith(arg, "--webcam-index=")) {
      config.webcam_index = std::atoi(arg + std::strlen("--webcam-index="));
    } else if (StartsWith(arg, "--width=")) {
      config.width = std::atoi(arg + std::strlen("--width="));
    } else if (StartsWith(arg, "--height=")) {
      config.height = std::atoi(arg + std::strlen("--height="));
    } else if (StartsWith(arg, "--rtsp-url=")) {
      config.rtsp_url = QString::fromUtf8(arg + std::strlen("--rtsp-url="));
    } else if (std::strcmp(arg, "--mirror-preview") == 0) {
      config.mirror_preview = true;
      mirror_override = true;
    } else if (std::strcmp(arg, "--no-mirror-preview") == 0) {
      config.mirror_preview = false;
      mirror_override = true;
    } else if (std::strcmp(arg, "--swap-lateral-guidance") == 0) {
      config.swap_lateral_guidance = true;
      guidance_override = true;
    } else if (std::strcmp(arg, "--no-swap-lateral-guidance") == 0) {
      config.swap_lateral_guidance = false;
      guidance_override = true;
    }
  }
  if (config.mode == PreviewInputMode::kLocalWebcam) {
    if (!mirror_override) {
      config.mirror_preview = true;
    }
    if (!guidance_override) {
      config.swap_lateral_guidance = true;
    }
  }
  return config;
}

std::unique_ptr<IPreviewFrameSource> CreatePreviewFrameSource(
    const PreviewFrameSourceConfig& config) {
  switch (config.mode) {
    case PreviewInputMode::kLocalWebcam:
      return std::make_unique<LocalWebcamPreviewFrameSource>(config);
    case PreviewInputMode::kRtsp:
      return std::make_unique<MockPreviewFrameSource>(config);
    case PreviewInputMode::kMock:
    default:
      return std::make_unique<MockPreviewFrameSource>(config);
  }
}

QString PreviewInputModeToString(PreviewInputMode mode) {
  switch (mode) {
    case PreviewInputMode::kMock:
      return QStringLiteral("mock");
    case PreviewInputMode::kLocalWebcam:
      return QStringLiteral("local_webcam");
    case PreviewInputMode::kRtsp:
      return QStringLiteral("rtsp");
  }
  return QStringLiteral("mock");
}

}  // namespace sentriface::host
