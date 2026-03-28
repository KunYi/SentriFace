#include <fstream>
#include <string>

#include "camera/offline_sequence_source.hpp"

int main() {
  const std::string manifest = "offline_sequence_test_manifest.txt";
  {
    std::ofstream out(manifest);
    out << "# path timestamp width height channels pixel_format\n";
    out << "frame_000.jpg 1000 640 640 3 rgb888\n";
    out << "frame_001.jpg 1033 640 640 3 rgb888\n";
  }

  sentriface::camera::OfflineSequenceFrameSource source(manifest, "offline_test");
  if (!source.Open()) {
    return 1;
  }

  const auto info = source.GetInfo();
  if (info.name != "offline_test" || info.is_live) {
    return 2;
  }

  sentriface::camera::Frame frame;
  if (!source.Read(frame)) {
    return 3;
  }
  if (frame.timestamp_ms != 1000 || frame.width != 640 || frame.height != 640) {
    return 4;
  }

  if (!source.Read(frame)) {
    return 5;
  }
  if (frame.timestamp_ms != 1033) {
    return 6;
  }

  if (source.Read(frame)) {
    return 7;
  }

  source.Close();
  return 0;
}
