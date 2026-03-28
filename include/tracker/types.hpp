#ifndef SENTRIFACE_TRACKER_TYPES_HPP_
#define SENTRIFACE_TRACKER_TYPES_HPP_

#include <array>
#include <cstdint>

namespace sentriface::tracker {

struct Point2f {
  float x = 0.0f;
  float y = 0.0f;
};

struct RectF {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
};

struct Landmark5 {
  std::array<Point2f, 5> points {};
};

enum class TrackState : std::uint8_t {
  kTentative = 0,
  kConfirmed = 1,
  kLost = 2,
  kRemoved = 3,
};

}  // namespace sentriface::tracker

#endif  // SENTRIFACE_TRACKER_TYPES_HPP_
