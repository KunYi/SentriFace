#include "camera/frame_source.hpp"

#include <utility>

namespace sentriface::camera {

SequenceFrameSource::SequenceFrameSource(std::vector<Frame> frames,
                                         const std::string& name)
    : frames_(std::make_move_iterator(frames.begin()),
              std::make_move_iterator(frames.end())),
      name_(name) {}

bool SequenceFrameSource::Open() {
  is_open_ = true;
  return true;
}

void SequenceFrameSource::Close() { is_open_ = false; }

bool SequenceFrameSource::IsOpen() const { return is_open_; }

FrameSourceInfo SequenceFrameSource::GetInfo() const {
  FrameSourceInfo info;
  info.name = name_;
  info.is_live = false;
  return info;
}

bool SequenceFrameSource::Read(Frame& frame) {
  if (!is_open_ || frames_.empty()) {
    return false;
  }
  frame = std::move(frames_.front());
  frames_.pop_front();
  return true;
}

}  // namespace sentriface::camera
