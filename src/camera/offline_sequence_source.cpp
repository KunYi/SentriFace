#include "camera/offline_sequence_source.hpp"

#include <fstream>
#include <sstream>

namespace sentriface::camera {

OfflineSequenceFrameSource::OfflineSequenceFrameSource(
    const std::string& manifest_path, const std::string& name)
    : manifest_path_(manifest_path), name_(name) {}

bool OfflineSequenceFrameSource::Open() {
  records_.clear();
  if (!LoadManifest()) {
    is_open_ = false;
    return false;
  }
  is_open_ = true;
  return true;
}

void OfflineSequenceFrameSource::Close() {
  records_.clear();
  is_open_ = false;
}

bool OfflineSequenceFrameSource::IsOpen() const { return is_open_; }

FrameSourceInfo OfflineSequenceFrameSource::GetInfo() const {
  FrameSourceInfo info;
  info.name = name_;
  info.is_live = false;
  return info;
}

bool OfflineSequenceFrameSource::Read(Frame& frame) {
  if (!is_open_ || records_.empty()) {
    return false;
  }

  const OfflineFrameRecord record = records_.front();
  records_.pop_front();

  frame.width = record.width;
  frame.height = record.height;
  frame.channels = record.channels;
  frame.pixel_format = record.pixel_format;
  frame.timestamp_ms = record.timestamp_ms;
  frame.data.assign(
      static_cast<std::size_t>(record.width * record.height * record.channels), 0U);
  return true;
}

bool OfflineSequenceFrameSource::LoadManifest() {
  std::ifstream in(manifest_path_);
  if (!in.is_open()) {
    return false;
  }

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    OfflineFrameRecord record;
    if (!ParseLine(line, record)) {
      return false;
    }
    records_.push_back(record);
  }
  return !records_.empty();
}

bool OfflineSequenceFrameSource::ParseLine(const std::string& line,
                                           OfflineFrameRecord& record) const {
  std::istringstream iss(line);
  std::string pixel_format_token;
  if (!(iss >> record.path >> record.timestamp_ms >> record.width >>
        record.height >> record.channels >> pixel_format_token)) {
    return false;
  }

  record.pixel_format = ParsePixelFormat(pixel_format_token);
  return record.width > 0 && record.height > 0 && record.channels > 0 &&
         record.pixel_format != PixelFormat::kUnknown;
}

PixelFormat OfflineSequenceFrameSource::ParsePixelFormat(
    const std::string& token) const {
  if (token == "rgb888") {
    return PixelFormat::kRgb888;
  }
  if (token == "bgr888") {
    return PixelFormat::kBgr888;
  }
  if (token == "gray8") {
    return PixelFormat::kGray8;
  }
  return PixelFormat::kUnknown;
}

}  // namespace sentriface::camera
