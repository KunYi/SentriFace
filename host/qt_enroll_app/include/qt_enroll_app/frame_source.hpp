#ifndef SENTRIFACE_HOST_QT_ENROLL_APP_FRAME_SOURCE_HPP_
#define SENTRIFACE_HOST_QT_ENROLL_APP_FRAME_SOURCE_HPP_

#include <functional>
#include <memory>

#include <QImage>
#include <QString>
#include <QTimer>

namespace sentriface::host {

enum class PreviewInputMode {
  kMock,
  kLocalWebcam,
  kRtsp,
};

struct PreviewFrameSourceConfig {
  PreviewInputMode mode = PreviewInputMode::kMock;
  int webcam_index = 0;
  QString rtsp_url;
  int width = 1280;
  int height = 720;
  bool mirror_preview = false;
  bool swap_lateral_guidance = false;
};

class IPreviewFrameSource {
 public:
  using FrameCallback = std::function<void(const QImage&)>;

  virtual ~IPreviewFrameSource() = default;

  virtual bool Start() = 0;
  virtual void Stop() = 0;
  virtual void SetFrameCallback(FrameCallback callback) = 0;
  virtual QString Description() const = 0;
  virtual PreviewInputMode mode() const = 0;
};

class LocalWebcamPreviewFrameSource : public IPreviewFrameSource {
 public:
  explicit LocalWebcamPreviewFrameSource(
      const PreviewFrameSourceConfig& config = PreviewFrameSourceConfig {});
  ~LocalWebcamPreviewFrameSource() override;

  bool Start() override;
  void Stop() override;
  QString Description() const override;
 PreviewInputMode mode() const override;
  bool IsSupported() const;
  void SetFrameCallback(FrameCallback callback) override;

 private:
  class Impl;
  PreviewFrameSourceConfig config_;
  bool running_ = false;
  bool supported_ = false;
  FrameCallback callback_;
  std::unique_ptr<Impl> impl_;
};

class MockPreviewFrameSource : public IPreviewFrameSource {
 public:
  explicit MockPreviewFrameSource(
      const PreviewFrameSourceConfig& config = PreviewFrameSourceConfig {});

  bool Start() override;
  void Stop() override;
  void SetFrameCallback(FrameCallback callback) override;
  QString Description() const override;
  PreviewInputMode mode() const override;

 private:
  PreviewFrameSourceConfig config_;
  bool running_ = false;
  int frame_index_ = 0;
  std::unique_ptr<QTimer> timer_;
  FrameCallback callback_;
};

PreviewFrameSourceConfig LoadPreviewFrameSourceConfigFromArgs(int argc, char* argv[]);
std::unique_ptr<IPreviewFrameSource> CreatePreviewFrameSource(
    const PreviewFrameSourceConfig& config);
QString PreviewInputModeToString(PreviewInputMode mode);

}  // namespace sentriface::host

#endif  // SENTRIFACE_HOST_QT_ENROLL_APP_FRAME_SOURCE_HPP_
