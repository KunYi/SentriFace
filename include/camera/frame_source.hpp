#ifndef SENTRIFACE_CAMERA_FRAME_SOURCE_HPP_
#define SENTRIFACE_CAMERA_FRAME_SOURCE_HPP_

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace sentriface::camera {

enum class PixelFormat : std::uint8_t {
  kUnknown = 0,
  kRgb888 = 1,
  kBgr888 = 2,
  kGray8 = 3,
};

struct Frame {
  int width = 0;
  int height = 0;
  int channels = 0;
  PixelFormat pixel_format = PixelFormat::kUnknown;
  std::uint64_t timestamp_ms = 0;
  std::vector<std::uint8_t> data;
};

struct FrameSourceInfo {
  std::string name;
  bool is_live = false;
};

class IFrameSource {
 public:
  virtual ~IFrameSource() = default;

  virtual bool Open() = 0;
  virtual void Close() = 0;
  virtual bool IsOpen() const = 0;
  virtual FrameSourceInfo GetInfo() const = 0;
  virtual bool Read(Frame& frame) = 0;
};

class SequenceFrameSource : public IFrameSource {
 public:
  explicit SequenceFrameSource(std::vector<Frame> frames,
                               const std::string& name = "sequence");

  bool Open() override;
  void Close() override;
  bool IsOpen() const override;
  FrameSourceInfo GetInfo() const override;
  bool Read(Frame& frame) override;

 private:
  std::deque<Frame> frames_;
  std::string name_;
  bool is_open_ = false;
};

}  // namespace sentriface::camera

#endif  // SENTRIFACE_CAMERA_FRAME_SOURCE_HPP_
