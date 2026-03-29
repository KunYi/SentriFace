#include <cstdint>
#include <filesystem>
#include <fstream>

#include "camera/image_io.hpp"

int main() {
  const std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() / "sentriface_image_io_test";
  std::filesystem::create_directories(temp_dir);
  const std::filesystem::path bmp_path = temp_dir / "tiny.bmp";

  const unsigned char bmp_bytes[] = {
      0x42, 0x4d, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
      0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00,
      0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x13, 0x0b,
      0x00, 0x00, 0x13, 0x0b, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
  };
  {
    std::ofstream out(bmp_path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bmp_bytes),
              static_cast<std::streamsize>(sizeof(bmp_bytes)));
  }

  sentriface::camera::Frame frame;
  if (!sentriface::camera::LoadBmpFrame(bmp_path.string(), &frame)) {
    return 1;
  }
  if (frame.width != 2 || frame.height != 1 || frame.channels != 3) {
    return 2;
  }
  if (frame.pixel_format != sentriface::camera::PixelFormat::kBgr888) {
    return 3;
  }
  if (frame.data.size() != 6U) {
    return 4;
  }
  if (frame.data[0] != 0x00 || frame.data[1] != 0x00 || frame.data[2] != 0xff) {
    return 5;
  }
  if (frame.data[3] != 0x00 || frame.data[4] != 0xff || frame.data[5] != 0x00) {
    return 6;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}
