#ifndef SENTRIFACE_TRACKER_TRACKER_HPP_
#define SENTRIFACE_TRACKER_TRACKER_HPP_

#include <cstdint>
#include <vector>

#include "tracker/association.hpp"
#include "tracker/detection.hpp"
#include "tracker/face_quality.hpp"
#include "tracker/kalman_filter.hpp"
#include "tracker/track.hpp"
#include "tracker/track_snapshot.hpp"
#include "tracker/tracker_config.hpp"

namespace sentriface::tracker {

class Tracker {
 public:
  explicit Tracker(const TrackerConfig& config);

  void Reset();
  void Step(const std::vector<Detection>& detections, std::uint64_t timestamp_ms);

  const std::vector<Track>& GetTracks() const;
  std::vector<TrackSnapshot> GetTrackSnapshots() const;
  std::vector<Track> GetConfirmedTracks() const;
  std::vector<Track> GetEmbeddingCandidates() const;
  std::vector<TrackSnapshot> GetEmbeddingTriggerCandidates(
      std::uint64_t timestamp_ms) const;
  bool MarkEmbeddingTriggered(int track_id, std::uint64_t timestamp_ms);

 private:
  TrackSnapshot MakeSnapshot(const Track& track) const;
  std::vector<Detection> FilterDetections(
      const std::vector<Detection>& detections) const;
  void PredictTracks(std::uint64_t timestamp_ms);
  void CreateTrack(const Detection& detection, std::uint64_t timestamp_ms);
  void MarkTrackMissed(Track& track);
  void PromoteTrackIfNeeded(Track& track);
  void RemoveDeadTracks();

  TrackerConfig config_;
  FaceQualityEvaluator face_quality_;
  KalmanFilter kf_;
  Association association_;
  std::vector<Track> tracks_;
  int next_track_id_ = 0;
};

}  // namespace sentriface::tracker

#endif  // SENTRIFACE_TRACKER_TRACKER_HPP_
