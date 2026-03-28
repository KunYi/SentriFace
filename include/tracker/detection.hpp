#ifndef SENTRIFACE_TRACKER_DETECTION_HPP_
#define SENTRIFACE_TRACKER_DETECTION_HPP_

#include <cstdint>

#include "tracker/types.hpp"

namespace sentriface::tracker {

struct Detection {
  RectF bbox {};
  float score = 0.0f;
  Landmark5 landmarks {};
  std::uint64_t timestamp_ms = 0;
};

}  // namespace sentriface::tracker

#endif  // SENTRIFACE_TRACKER_DETECTION_HPP_
