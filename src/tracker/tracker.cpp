#include "tracker/tracker.hpp"

#include <algorithm>

namespace sentriface::tracker {
namespace {

RectF StateToRect(const std::array<float, 8>& x) {
  return RectF {x[0] - 0.5f * x[2], x[1] - 0.5f * x[3], x[2], x[3]};
}

}  // namespace

Tracker::Tracker(const TrackerConfig& config)
    : config_(config), face_quality_(config), kf_(config), association_(config) {}

void Tracker::Reset() {
  tracks_.clear();
  next_track_id_ = 0;
}

void Tracker::Step(const std::vector<Detection>& detections,
                   std::uint64_t timestamp_ms) {
  const std::vector<Detection> filtered = FilterDetections(detections);
  PredictTracks(timestamp_ms);

  const std::vector<MatchCandidate> candidates =
      association_.BuildCandidates(tracks_, filtered);

  std::vector<bool> track_used(tracks_.size(), false);
  std::vector<bool> detection_used(filtered.size(), false);

  for (const MatchCandidate& candidate : candidates) {
    if (candidate.track_index < 0 || candidate.detection_index < 0) {
      continue;
    }
    if (track_used[candidate.track_index] || detection_used[candidate.detection_index]) {
      continue;
    }

    Track& track = tracks_[candidate.track_index];
    const Detection& det = filtered[candidate.detection_index];
    const float cx = det.bbox.x + 0.5f * det.bbox.w;
    const float cy = det.bbox.y + 0.5f * det.bbox.h;
    kf_.Update(track.x, track.p, cx, cy, det.bbox.w, det.bbox.h);
    track.bbox_latest = det.bbox;
    track.bbox_pred = StateToRect(track.x);
    track.landmarks_latest = det.landmarks;
    track.last_detection_score = det.score;
    track.face_quality = face_quality_.Evaluate(det);
    track.best_face_quality = std::max(track.best_face_quality, track.face_quality);
    track.last_timestamp_ms = timestamp_ms;
    track.age += 1;
    track.hits += 1;
    track.miss = 0;
    if (track.state == TrackState::kLost) {
      track.state = TrackState::kConfirmed;
    }
    PromoteTrackIfNeeded(track);

    track_used[candidate.track_index] = true;
    detection_used[candidate.detection_index] = true;
  }

  for (int i = 0; i < static_cast<int>(tracks_.size()); ++i) {
    if (!track_used[i]) {
      MarkTrackMissed(tracks_[i]);
    }
  }

  for (int i = 0; i < static_cast<int>(filtered.size()); ++i) {
    if (!detection_used[i] &&
        static_cast<int>(tracks_.size()) < config_.max_tracks) {
      CreateTrack(filtered[i], timestamp_ms);
    }
  }

  RemoveDeadTracks();
}

const std::vector<Track>& Tracker::GetTracks() const { return tracks_; }

TrackSnapshot Tracker::MakeSnapshot(const Track& track) const {
  TrackSnapshot snapshot;
  snapshot.track_id = track.track_id;
  snapshot.state = track.state;
  snapshot.bbox = track.bbox_latest;
  snapshot.landmarks = track.landmarks_latest;
  snapshot.age = track.age;
  snapshot.hits = track.hits;
  snapshot.miss = track.miss;
  snapshot.last_detection_score = track.last_detection_score;
  snapshot.face_quality = track.face_quality;
  snapshot.best_face_quality = track.best_face_quality;
  snapshot.last_timestamp_ms = track.last_timestamp_ms;
  snapshot.last_embedding_timestamp_ms = track.last_embedding_timestamp_ms;
  return snapshot;
}

std::vector<TrackSnapshot> Tracker::GetTrackSnapshots() const {
  std::vector<TrackSnapshot> out;
  out.reserve(tracks_.size());
  for (const Track& track : tracks_) {
    out.push_back(MakeSnapshot(track));
  }
  return out;
}

std::vector<Track> Tracker::GetConfirmedTracks() const {
  std::vector<Track> out;
  out.reserve(tracks_.size());
  for (const Track& track : tracks_) {
    if (track.state == TrackState::kConfirmed) {
      out.push_back(track);
    }
  }
  return out;
}

std::vector<Track> Tracker::GetEmbeddingCandidates() const {
  std::vector<Track> out;
  out.reserve(tracks_.size());
  for (const Track& track : tracks_) {
    if (track.state != TrackState::kConfirmed) {
      continue;
    }
    if (track.miss != 0) {
      continue;
    }
    if (track.last_detection_score < config_.embed_det_score_thresh) {
      continue;
    }
    if (track.bbox_latest.w < config_.embed_min_face_width ||
        track.bbox_latest.h < config_.embed_min_face_height) {
      continue;
    }
    if (track.face_quality < config_.embed_min_face_quality) {
      continue;
    }
      out.push_back(track);
  }
  return out;
}

std::vector<TrackSnapshot> Tracker::GetEmbeddingTriggerCandidates(
    std::uint64_t timestamp_ms) const {
  std::vector<TrackSnapshot> out;
  out.reserve(tracks_.size());
  for (const Track& track : tracks_) {
    if (track.state != TrackState::kConfirmed) {
      continue;
    }
    if (track.miss != 0) {
      continue;
    }
    if (track.last_detection_score < config_.embed_det_score_thresh) {
      continue;
    }
    if (track.bbox_latest.w < config_.embed_min_face_width ||
        track.bbox_latest.h < config_.embed_min_face_height) {
      continue;
    }
    if (track.face_quality < config_.embed_min_face_quality) {
      continue;
    }
    if (track.last_embedding_timestamp_ms != 0 &&
        timestamp_ms >= track.last_embedding_timestamp_ms &&
        timestamp_ms - track.last_embedding_timestamp_ms < config_.embed_interval_ms) {
      continue;
    }
    out.push_back(MakeSnapshot(track));
  }
  return out;
}

bool Tracker::MarkEmbeddingTriggered(int track_id, std::uint64_t timestamp_ms) {
  for (Track& track : tracks_) {
    if (track.track_id != track_id) {
      continue;
    }
    track.last_embedding_timestamp_ms = timestamp_ms;
    return true;
  }
  return false;
}

std::vector<Detection> Tracker::FilterDetections(
    const std::vector<Detection>& detections) const {
  std::vector<Detection> out;
  out.reserve(detections.size());
  for (const Detection& det : detections) {
    if (det.score < config_.det_score_thresh) {
      continue;
    }
    if (det.bbox.w < config_.min_face_width || det.bbox.h < config_.min_face_height) {
      continue;
    }
    out.push_back(det);
  }
  return out;
}

void Tracker::PredictTracks(std::uint64_t timestamp_ms) {
  for (Track& track : tracks_) {
    if (track.state == TrackState::kRemoved) {
      continue;
    }
    float dt = 0.0f;
    if (track.last_timestamp_ms != 0 && timestamp_ms >= track.last_timestamp_ms) {
      dt = static_cast<float>(timestamp_ms - track.last_timestamp_ms) / 1000.0f;
    }
    kf_.Predict(track.x, track.p, dt);
    track.bbox_pred = StateToRect(track.x);
  }
}

void Tracker::CreateTrack(const Detection& detection, std::uint64_t timestamp_ms) {
  Track track;
  track.track_id = next_track_id_++;
  track.state = TrackState::kTentative;
  track.age = 1;
  track.hits = 1;
  track.miss = 0;
  track.last_timestamp_ms = timestamp_ms;
  track.bbox_latest = detection.bbox;
  track.landmarks_latest = detection.landmarks;
  track.last_detection_score = detection.score;
  track.face_quality = face_quality_.Evaluate(detection);
  track.best_face_quality = track.face_quality;
  const float cx = detection.bbox.x + 0.5f * detection.bbox.w;
  const float cy = detection.bbox.y + 0.5f * detection.bbox.h;
  kf_.Initialize(track.x, track.p, cx, cy, detection.bbox.w, detection.bbox.h);
  track.bbox_pred = StateToRect(track.x);
  tracks_.push_back(track);
}

void Tracker::MarkTrackMissed(Track& track) {
  track.age += 1;
  track.miss += 1;
  if (track.miss > config_.max_miss) {
    track.state = TrackState::kRemoved;
    return;
  }
  track.state = TrackState::kLost;
}

void Tracker::PromoteTrackIfNeeded(Track& track) {
  if (track.hits >= config_.min_confirm_hits) {
    track.state = TrackState::kConfirmed;
  }
}

void Tracker::RemoveDeadTracks() {
  tracks_.erase(
      std::remove_if(tracks_.begin(), tracks_.end(),
                     [](const Track& track) {
                       return track.state == TrackState::kRemoved;
                     }),
      tracks_.end());
}

}  // namespace sentriface::tracker
