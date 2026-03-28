#ifndef SENTRIFACE_APP_FACE_PIPELINE_HPP_
#define SENTRIFACE_APP_FACE_PIPELINE_HPP_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "decision/face_decision.hpp"
#include "enroll/enrollment_store.hpp"
#include "logging/system_logger.hpp"
#include "search/face_search.hpp"
#include "tracker/track_snapshot.hpp"

namespace sentriface::app {

struct PipelineConfig {
  sentriface::search::SearchConfig search {};
  sentriface::decision::DecisionConfig decision {};
  bool enable_short_gap_merge = true;
  std::uint64_t short_gap_merge_window_ms = 1500;
  int short_gap_merge_min_previous_hits = 2;
  float short_gap_merge_center_gate_ratio = 1.25f;
  float short_gap_merge_area_ratio_min = 0.40f;
  float short_gap_merge_area_ratio_max = 2.50f;
  bool enable_embedding_assisted_relink = true;
  float embedding_relink_similarity_min = 0.45f;
  std::uint32_t adaptive_update_min_interval_ms = 1000;
  sentriface::logging::LoggerConfig logger_config {};
};

struct PipelineTrackState {
  sentriface::tracker::TrackSnapshot snapshot {};
  sentriface::decision::DecisionState decision {};
};

struct PrototypeUpdateCandidate {
  int track_id = -1;
  int person_id = -1;
  std::vector<float> embedding;
  sentriface::enroll::PrototypeMetadata metadata {};
  bool eligible_recent_adaptive = false;
  bool eligible_verified_history = false;
  sentriface::enroll::PrototypeAcceptanceDiagnostic recent_adaptive_diagnostic {};
  sentriface::enroll::PrototypeAcceptanceDiagnostic verified_history_diagnostic {};
  bool cooldown_blocked = false;
};

class FacePipeline {
 public:
  explicit FacePipeline(const PipelineConfig& config = PipelineConfig {});

  void Reset();
  void SyncTracks(const std::vector<sentriface::tracker::TrackSnapshot>& snapshots);
  bool LoadEnrollment(const sentriface::enroll::EnrollmentStore& store);
  bool LoadEnrollment(const sentriface::enroll::EnrollmentStoreV2& store);

  sentriface::search::SearchResult SearchEmbedding(
      const std::vector<float>& embedding) const;
  sentriface::search::SearchResultV2 SearchEmbeddingV2(
      const std::vector<float>& embedding) const;
  sentriface::decision::DecisionState UpdateTrackSearch(
      int track_id, const sentriface::search::SearchResult& result);
  sentriface::decision::DecisionState UpdateTrackSearch(
      int track_id, const sentriface::search::SearchResultV2& result);
  sentriface::decision::DecisionState UpdateTrackSearch(
      int track_id,
      const std::vector<float>& embedding,
      const sentriface::search::SearchResult& result);
  sentriface::decision::DecisionState UpdateTrackSearch(
      int track_id,
      const std::vector<float>& embedding,
      const sentriface::search::SearchResultV2& result);

  std::vector<PipelineTrackState> GetTrackStates() const;
  std::vector<PipelineTrackState> GetUnlockReadyTracks() const;
  std::vector<PrototypeUpdateCandidate> GetPrototypeUpdateCandidates(
      const sentriface::enroll::EnrollmentStoreV2& store,
      bool liveness_ok = true) const;
  int ApplyAdaptivePrototypeUpdates(
      sentriface::enroll::EnrollmentStoreV2& store,
      bool promote_to_verified = false,
      bool liveness_ok = true);
  int GetShortGapMergeCount() const;

 private:
  struct RecentRemovedTrack {
    sentriface::tracker::TrackSnapshot snapshot {};
    sentriface::decision::DecisionState decision {};
    std::vector<float> last_embedding;
  };

  bool IsShortGapMergeCandidate(
      const sentriface::tracker::TrackSnapshot& previous,
      const sentriface::tracker::TrackSnapshot& current,
      const sentriface::decision::DecisionState& previous_decision,
      const sentriface::decision::DecisionState& current_decision,
      const std::vector<float>& previous_embedding,
      const std::vector<float>& current_embedding) const;
  float CosineSimilarity(const std::vector<float>& a,
                         const std::vector<float>& b) const;
  void CleanupRecentRemoved(std::uint64_t current_timestamp_ms);

  PipelineConfig config_;
  sentriface::search::FaceSearch search_;
  sentriface::search::FaceSearchV2 search_v2_;
  sentriface::decision::FaceDecisionEngine decision_;
  sentriface::logging::SystemLogger logger_;
  std::unordered_map<int, sentriface::tracker::TrackSnapshot> snapshots_;
  std::unordered_map<int, RecentRemovedTrack> recent_removed_;
  std::unordered_map<int, std::vector<float>> track_embeddings_;
  std::unordered_map<int, std::uint32_t> last_prototype_update_timestamp_ms_;
  std::uint64_t last_sync_timestamp_ms_ = 0;
  int short_gap_merge_count_ = 0;
};

}  // namespace sentriface::app

#endif  // SENTRIFACE_APP_FACE_PIPELINE_HPP_
