#ifndef SENTRIFACE_TRACKER_KALMAN_FILTER_HPP_
#define SENTRIFACE_TRACKER_KALMAN_FILTER_HPP_

#include <array>

#include "tracker/tracker_config.hpp"

namespace sentriface::tracker {

class KalmanFilter {
 public:
  explicit KalmanFilter(const TrackerConfig& config);

  void Initialize(std::array<float, 8>& x, std::array<float, 64>& p,
                  float cx, float cy, float w, float h) const;

  void Predict(std::array<float, 8>& x, std::array<float, 64>& p,
               float dt) const;

  void Update(std::array<float, 8>& x, std::array<float, 64>& p,
              float cx, float cy, float w, float h) const;

 private:
  TrackerConfig config_;
};

}  // namespace sentriface::tracker

#endif  // SENTRIFACE_TRACKER_KALMAN_FILTER_HPP_
