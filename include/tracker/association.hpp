#ifndef SENTRIFACE_TRACKER_ASSOCIATION_HPP_
#define SENTRIFACE_TRACKER_ASSOCIATION_HPP_

#include <vector>

#include "tracker/detection.hpp"
#include "tracker/track.hpp"
#include "tracker/tracker_config.hpp"

namespace sentriface::tracker {

struct MatchCandidate {
  int track_index = -1;
  int detection_index = -1;
  float cost = 0.0f;
};

class Association {
 public:
  explicit Association(const TrackerConfig& config);

  std::vector<MatchCandidate> BuildCandidates(
      const std::vector<Track>& tracks,
      const std::vector<Detection>& detections) const;

 private:
  TrackerConfig config_;
};

}  // namespace sentriface::tracker

#endif  // SENTRIFACE_TRACKER_ASSOCIATION_HPP_
