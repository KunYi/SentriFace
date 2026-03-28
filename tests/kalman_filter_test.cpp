#include <array>
#include <cmath>

#include "tracker/kalman_filter.hpp"
#include "tracker/tracker_config.hpp"

namespace {

bool AlmostEqual(float a, float b, float eps = 1e-3f) {
  return std::fabs(a - b) <= eps;
}

}  // namespace

int main() {
  using sentriface::tracker::KalmanFilter;
  using sentriface::tracker::TrackerConfig;

  TrackerConfig config;
  KalmanFilter kf(config);

  std::array<float, 8> x {};
  std::array<float, 64> p {};
  kf.Initialize(x, p, 100.0f, 120.0f, 80.0f, 96.0f);

  if (!AlmostEqual(x[0], 100.0f) || !AlmostEqual(x[1], 120.0f) ||
      !AlmostEqual(x[2], 80.0f) || !AlmostEqual(x[3], 96.0f)) {
    return 1;
  }

  x[4] = 10.0f;
  x[5] = -5.0f;
  x[6] = 2.0f;
  x[7] = -1.0f;
  kf.Predict(x, p, 0.1f);

  if (!AlmostEqual(x[0], 101.0f) || !AlmostEqual(x[1], 119.5f) ||
      !AlmostEqual(x[2], 80.2f) || !AlmostEqual(x[3], 95.9f)) {
    return 2;
  }

  kf.Update(x, p, 102.0f, 121.0f, 81.0f, 97.0f);

  if (x[2] < 1.0f || x[3] < 1.0f) {
    return 3;
  }

  if (!(x[0] > 101.0f && x[0] < 102.0f)) {
    return 4;
  }
  if (!(x[1] > 119.5f && x[1] < 121.0f)) {
    return 5;
  }

  return 0;
}
