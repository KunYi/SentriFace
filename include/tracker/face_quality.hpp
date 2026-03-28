#ifndef SENTRIFACE_TRACKER_FACE_QUALITY_HPP_
#define SENTRIFACE_TRACKER_FACE_QUALITY_HPP_

#include "tracker/detection.hpp"
#include "tracker/tracker_config.hpp"

namespace sentriface::tracker {

class FaceQualityEvaluator {
 public:
  explicit FaceQualityEvaluator(const TrackerConfig& config);

  float Evaluate(const Detection& detection) const;

 private:
  TrackerConfig config_;
};

}  // namespace sentriface::tracker

#endif  // SENTRIFACE_TRACKER_FACE_QUALITY_HPP_
