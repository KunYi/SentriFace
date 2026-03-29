#ifndef SENTRIFACE_CAMERA_IMAGE_IO_HPP_
#define SENTRIFACE_CAMERA_IMAGE_IO_HPP_

#include <string>

#include "camera/frame_source.hpp"

namespace sentriface::camera {

bool LoadBmpFrame(const std::string& path, Frame* out_frame);

}  // namespace sentriface::camera

#endif  // SENTRIFACE_CAMERA_IMAGE_IO_HPP_
