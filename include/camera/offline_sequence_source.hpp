#ifndef SENTRIFACE_CAMERA_OFFLINE_SEQUENCE_SOURCE_HPP_
#define SENTRIFACE_CAMERA_OFFLINE_SEQUENCE_SOURCE_HPP_

#include <deque>
#include <string>

#include "camera/frame_source.hpp"

namespace sentriface::camera {

struct OfflineFrameRecord {
  std::string path;
  std::uint64_t timestamp_ms = 0;
  int width = 0;
  int height = 0;
  int channels = 0;
  PixelFormat pixel_format = PixelFormat::kUnknown;
};

class OfflineSequenceFrameSource : public IFrameSource {
 public:
  explicit OfflineSequenceFrameSource(const std::string& manifest_path,
                                      const std::string& name = "offline_sequence");

  bool Open() override;
  void Close() override;
  bool IsOpen() const override;
  FrameSourceInfo GetInfo() const override;
  bool Read(Frame& frame) override;

 private:
  bool LoadManifest();
  bool ParseLine(const std::string& line, OfflineFrameRecord& record) const;
  PixelFormat ParsePixelFormat(const std::string& token) const;

  std::string manifest_path_;
  std::string name_;
  std::deque<OfflineFrameRecord> records_;
  bool is_open_ = false;
};

}  // namespace sentriface::camera

#endif  // SENTRIFACE_CAMERA_OFFLINE_SEQUENCE_SOURCE_HPP_
