#include <cstdint>
#include <vector>

#include "camera/frame_source.hpp"

int main() {
  using sentriface::camera::Frame;
  using sentriface::camera::PixelFormat;
  using sentriface::camera::SequenceFrameSource;

  Frame f1;
  f1.width = 2;
  f1.height = 2;
  f1.channels = 3;
  f1.pixel_format = PixelFormat::kRgb888;
  f1.timestamp_ms = static_cast<std::uint64_t>(1000);
  f1.data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

  Frame f2 = f1;
  f2.timestamp_ms = static_cast<std::uint64_t>(1033);

  SequenceFrameSource source(std::vector<Frame> {f1, f2}, "test_sequence");
  if (!source.Open()) {
    return 1;
  }
  if (!source.IsOpen()) {
    return 2;
  }

  const auto info = source.GetInfo();
  if (info.name != "test_sequence" || info.is_live) {
    return 3;
  }

  Frame out;
  if (!source.Read(out)) {
    return 4;
  }
  if (out.timestamp_ms != static_cast<std::uint64_t>(1000) ||
      out.width != 2 || out.height != 2) {
    return 5;
  }

  if (!source.Read(out)) {
    return 6;
  }
  if (out.timestamp_ms != static_cast<std::uint64_t>(1033)) {
    return 7;
  }

  if (source.Read(out)) {
    return 8;
  }

  source.Close();
  if (source.IsOpen()) {
    return 9;
  }

  return 0;
}
