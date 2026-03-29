#include "camera/image_io.hpp"

#include <cstdint>
#include <fstream>
#include <vector>

namespace sentriface::camera {

namespace {

bool ReadU16(std::istream* in, std::uint16_t* value) {
  unsigned char bytes[2];
  in->read(reinterpret_cast<char*>(bytes), 2);
  if (!in->good()) {
    return false;
  }
  *value = static_cast<std::uint16_t>(bytes[0]) |
           (static_cast<std::uint16_t>(bytes[1]) << 8);
  return true;
}

bool ReadU32(std::istream* in, std::uint32_t* value) {
  unsigned char bytes[4];
  in->read(reinterpret_cast<char*>(bytes), 4);
  if (!in->good()) {
    return false;
  }
  *value = static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
  return true;
}

bool ReadS32(std::istream* in, std::int32_t* value) {
  std::uint32_t raw = 0;
  if (!ReadU32(in, &raw)) {
    return false;
  }
  *value = static_cast<std::int32_t>(raw);
  return true;
}

}  // namespace

bool LoadBmpFrame(const std::string& path, Frame* out_frame) {
  if (out_frame == nullptr) {
    return false;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in.good()) {
    return false;
  }

  std::uint16_t signature = 0;
  std::uint32_t file_size = 0;
  std::uint16_t reserved_a = 0;
  std::uint16_t reserved_b = 0;
  std::uint32_t pixel_offset = 0;
  std::uint32_t dib_size = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::uint16_t planes = 0;
  std::uint16_t bit_count = 0;
  std::uint32_t compression = 0;
  std::uint32_t image_size = 0;
  std::int32_t x_ppm = 0;
  std::int32_t y_ppm = 0;
  std::uint32_t colors_used = 0;
  std::uint32_t colors_important = 0;

  if (!ReadU16(&in, &signature) || signature != 0x4d42u ||
      !ReadU32(&in, &file_size) ||
      !ReadU16(&in, &reserved_a) ||
      !ReadU16(&in, &reserved_b) ||
      !ReadU32(&in, &pixel_offset) ||
      !ReadU32(&in, &dib_size) || dib_size < 40u ||
      !ReadS32(&in, &width) ||
      !ReadS32(&in, &height) ||
      !ReadU16(&in, &planes) || planes != 1u ||
      !ReadU16(&in, &bit_count) ||
      !ReadU32(&in, &compression) ||
      !ReadU32(&in, &image_size) ||
      !ReadS32(&in, &x_ppm) ||
      !ReadS32(&in, &y_ppm) ||
      !ReadU32(&in, &colors_used) ||
      !ReadU32(&in, &colors_important)) {
    return false;
  }

  if (width <= 0 || height == 0 || compression != 0u ||
      (bit_count != 24u && bit_count != 8u)) {
    return false;
  }

  const bool bottom_up = height > 0;
  const int abs_height = height > 0 ? height : -height;
  const int channels = bit_count == 24u ? 3 : 1;
  const std::size_t row_stride =
      ((static_cast<std::size_t>(width) * channels) + 3u) & ~static_cast<std::size_t>(3u);

  in.seekg(static_cast<std::streamoff>(pixel_offset), std::ios::beg);
  if (!in.good()) {
    return false;
  }

  Frame frame;
  frame.width = width;
  frame.height = abs_height;
  frame.channels = channels;
  frame.pixel_format = channels == 3 ? PixelFormat::kBgr888 : PixelFormat::kGray8;
  frame.data.resize(static_cast<std::size_t>(width) * abs_height * channels, 0u);

  std::vector<unsigned char> row_buffer(row_stride, 0u);
  for (int row = 0; row < abs_height; ++row) {
    in.read(reinterpret_cast<char*>(row_buffer.data()),
            static_cast<std::streamsize>(row_stride));
    if (!in.good()) {
      return false;
    }
    const int dst_row = bottom_up ? (abs_height - 1 - row) : row;
    unsigned char* dst = frame.data.data() +
                         static_cast<std::size_t>(dst_row * width * channels);
    for (int col = 0; col < width * channels; ++col) {
      dst[col] = row_buffer[static_cast<std::size_t>(col)];
    }
  }

  *out_frame = std::move(frame);
  return true;
}

}  // namespace sentriface::camera
