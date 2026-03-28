#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "app/face_pipeline.hpp"

int main() {
  using sentriface::app::FacePipeline;
  using sentriface::app::PipelineConfig;
  using sentriface::enroll::EnrollmentConfig;
  using sentriface::enroll::EnrollmentConfigV2;
  using sentriface::enroll::EnrollmentStore;
  using sentriface::enroll::EnrollmentStoreV2;
  using sentriface::enroll::PrototypeMetadata;
  using sentriface::enroll::PrototypeZone;
  using sentriface::tracker::TrackSnapshot;
  using sentriface::tracker::TrackState;

  PipelineConfig config;
  config.search.embedding_dim = 4;
  config.search.top_k = 2;
  config.decision.min_consistent_hits = 3;
  config.decision.unlock_threshold = 0.78f;
  config.decision.avg_threshold = 0.72f;
  config.decision.min_margin = 0.03f;
  config.logger_config.enabled = true;
  config.logger_config.level = sentriface::logging::LogLevel::kDebug;
  config.logger_config.backend = sentriface::logging::LogBackend::kFile;
  config.logger_config.file_path = "face_pipeline_test.log";
  std::remove(config.logger_config.file_path.c_str());

  FacePipeline pipeline(config);

  EnrollmentConfig enroll_config;
  enroll_config.embedding_dim = 4;
  EnrollmentStore store(enroll_config);
  if (!store.UpsertPerson(1, "alice")) {
    return 1;
  }
  if (!store.AddPrototype(1, std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f})) {
    return 2;
  }
  if (!pipeline.LoadEnrollment(store)) {
    return 3;
  }

  TrackSnapshot snapshot;
  snapshot.track_id = 7;
  snapshot.state = TrackState::kConfirmed;
  snapshot.last_timestamp_ms = static_cast<std::uint64_t>(1000);
  pipeline.SyncTracks(std::vector<TrackSnapshot> {snapshot});

  const auto result = pipeline.SearchEmbedding(
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!result.ok) {
    return 4;
  }

  auto state = pipeline.UpdateTrackSearch(7, result);
  if (state.unlock_ready) {
    return 5;
  }

  state = pipeline.UpdateTrackSearch(7, result);
  if (state.unlock_ready) {
    return 6;
  }

  state = pipeline.UpdateTrackSearch(7, result);
  if (!state.unlock_ready) {
    return 7;
  }

  const auto unlock_ready = pipeline.GetUnlockReadyTracks();
  if (unlock_ready.size() != 1U) {
    return 8;
  }
  if (unlock_ready[0].snapshot.track_id != 7 ||
      !unlock_ready[0].decision.unlock_ready) {
    return 9;
  }

  pipeline.SyncTracks({});

  FacePipeline merge_pipeline(config);
  if (!merge_pipeline.LoadEnrollment(store)) {
    return 10;
  }

  TrackSnapshot old_snapshot;
  old_snapshot.track_id = 20;
  old_snapshot.state = TrackState::kConfirmed;
  old_snapshot.last_timestamp_ms = static_cast<std::uint64_t>(1000);
  merge_pipeline.SyncTracks(std::vector<TrackSnapshot> {old_snapshot});

  auto merge_result = merge_pipeline.SearchEmbedding(
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!merge_result.ok) {
    return 11;
  }

  auto merge_state = merge_pipeline.UpdateTrackSearch(20, merge_result);
  if (merge_state.unlock_ready) {
    return 12;
  }
  merge_state = merge_pipeline.UpdateTrackSearch(20, merge_result);
  if (merge_state.unlock_ready) {
    return 13;
  }

  merge_pipeline.SyncTracks({});

  TrackSnapshot new_snapshot;
  new_snapshot.track_id = 21;
  new_snapshot.state = TrackState::kConfirmed;
  new_snapshot.last_timestamp_ms = static_cast<std::uint64_t>(1800);
  merge_pipeline.SyncTracks(std::vector<TrackSnapshot> {new_snapshot});

  merge_state = merge_pipeline.UpdateTrackSearch(21, merge_result);
  if (!merge_state.unlock_ready) {
    return 14;
  }
  if (merge_state.stable_person_id != 1) {
    return 15;
  }
  if (merge_pipeline.GetShortGapMergeCount() != 1) {
    return 16;
  }

  FacePipeline overlap_pipeline(config);
  if (!overlap_pipeline.LoadEnrollment(store)) {
    return 17;
  }

  TrackSnapshot overlap_old;
  overlap_old.track_id = 30;
  overlap_old.state = TrackState::kConfirmed;
  overlap_old.last_timestamp_ms = static_cast<std::uint64_t>(1000);
  overlap_pipeline.SyncTracks(std::vector<TrackSnapshot> {overlap_old});

  auto overlap_result = overlap_pipeline.SearchEmbedding(
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  auto overlap_state = overlap_pipeline.UpdateTrackSearch(30, overlap_result);
  overlap_state = overlap_pipeline.UpdateTrackSearch(30, overlap_result);

  overlap_old.state = TrackState::kLost;
  overlap_old.last_timestamp_ms = static_cast<std::uint64_t>(1800);
  TrackSnapshot overlap_new;
  overlap_new.track_id = 31;
  overlap_new.state = TrackState::kConfirmed;
  overlap_new.last_timestamp_ms = static_cast<std::uint64_t>(1800);
  overlap_pipeline.SyncTracks(std::vector<TrackSnapshot> {overlap_old, overlap_new});

  overlap_state = overlap_pipeline.UpdateTrackSearch(31, overlap_result);
  if (!overlap_state.unlock_ready) {
    return 18;
  }
  if (overlap_pipeline.GetShortGapMergeCount() != 1) {
    return 19;
  }

  FacePipeline geometry_pipeline(config);
  if (!geometry_pipeline.LoadEnrollment(store)) {
    return 20;
  }

  TrackSnapshot geo_old;
  geo_old.track_id = 40;
  geo_old.state = TrackState::kConfirmed;
  geo_old.last_timestamp_ms = static_cast<std::uint64_t>(1000);
  geo_old.bbox = sentriface::tracker::RectF {100.0f, 100.0f, 120.0f, 140.0f};
  geometry_pipeline.SyncTracks(std::vector<TrackSnapshot> {geo_old});

  auto geo_result = geometry_pipeline.SearchEmbedding(
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  auto geo_state = geometry_pipeline.UpdateTrackSearch(40, geo_result);
  geo_state = geometry_pipeline.UpdateTrackSearch(40, geo_result);

  geometry_pipeline.SyncTracks({});

  TrackSnapshot geo_new;
  geo_new.track_id = 41;
  geo_new.state = TrackState::kConfirmed;
  geo_new.last_timestamp_ms = static_cast<std::uint64_t>(1800);
  geo_new.bbox = sentriface::tracker::RectF {420.0f, 320.0f, 120.0f, 140.0f};
  geometry_pipeline.SyncTracks(std::vector<TrackSnapshot> {geo_new});

  geo_state = geometry_pipeline.UpdateTrackSearch(41, geo_result);
  if (geo_state.unlock_ready) {
    return 21;
  }
  if (geometry_pipeline.GetShortGapMergeCount() != 0) {
    return 22;
  }

  PipelineConfig embedding_config = config;
  embedding_config.enable_embedding_assisted_relink = true;
  embedding_config.embedding_relink_similarity_min = 0.90f;
  FacePipeline embedding_pipeline(embedding_config);
  if (!embedding_pipeline.LoadEnrollment(store)) {
    return 23;
  }

  TrackSnapshot emb_old;
  emb_old.track_id = 50;
  emb_old.state = TrackState::kConfirmed;
  emb_old.last_timestamp_ms = static_cast<std::uint64_t>(1000);
  emb_old.bbox = sentriface::tracker::RectF {120.0f, 100.0f, 120.0f, 140.0f};
  embedding_pipeline.SyncTracks(std::vector<TrackSnapshot> {emb_old});

  auto emb_result = embedding_pipeline.SearchEmbedding(
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  auto emb_state = embedding_pipeline.UpdateTrackSearch(
      50,
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f},
      emb_result);
  emb_state = embedding_pipeline.UpdateTrackSearch(
      50,
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f},
      emb_result);
  emb_state = embedding_pipeline.UpdateTrackSearch(
      50,
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f},
      emb_result);

  embedding_pipeline.SyncTracks({});

  TrackSnapshot emb_new;
  emb_new.track_id = 51;
  emb_new.state = TrackState::kConfirmed;
  emb_new.last_timestamp_ms = static_cast<std::uint64_t>(1700);
  emb_new.bbox = sentriface::tracker::RectF {126.0f, 105.0f, 118.0f, 138.0f};
  embedding_pipeline.SyncTracks(std::vector<TrackSnapshot> {emb_new});

  sentriface::search::SearchResult empty_result;
  emb_state = embedding_pipeline.UpdateTrackSearch(
      51,
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f},
      empty_result);
  if (!emb_state.unlock_ready) {
    return 24;
  }
  if (embedding_pipeline.GetShortGapMergeCount() != 1) {
    return 25;
  }

  PipelineConfig weak_relink_config = config;
  weak_relink_config.enable_embedding_assisted_relink = true;
  weak_relink_config.embedding_relink_similarity_min = 0.90f;
  weak_relink_config.short_gap_merge_min_previous_hits = 2;
  FacePipeline weak_relink_pipeline(weak_relink_config);
  if (!weak_relink_pipeline.LoadEnrollment(store)) {
    return 26;
  }

  TrackSnapshot weak_old;
  weak_old.track_id = 52;
  weak_old.state = TrackState::kConfirmed;
  weak_old.last_timestamp_ms = static_cast<std::uint64_t>(1000);
  weak_old.bbox = sentriface::tracker::RectF {120.0f, 100.0f, 120.0f, 140.0f};
  weak_relink_pipeline.SyncTracks(std::vector<TrackSnapshot> {weak_old});

  auto weak_result = weak_relink_pipeline.SearchEmbedding(
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  auto weak_state = weak_relink_pipeline.UpdateTrackSearch(
      52,
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f},
      weak_result);
  if (weak_state.unlock_ready) {
    return 27;
  }

  weak_relink_pipeline.SyncTracks({});

  TrackSnapshot weak_new;
  weak_new.track_id = 53;
  weak_new.state = TrackState::kConfirmed;
  weak_new.last_timestamp_ms = static_cast<std::uint64_t>(1700);
  weak_new.bbox = sentriface::tracker::RectF {126.0f, 105.0f, 118.0f, 138.0f};
  weak_relink_pipeline.SyncTracks(std::vector<TrackSnapshot> {weak_new});

  sentriface::search::SearchResult weak_empty_result;
  weak_state = weak_relink_pipeline.UpdateTrackSearch(
      53,
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f},
      weak_empty_result);
  if (weak_state.unlock_ready) {
    return 28;
  }
  if (weak_relink_pipeline.GetShortGapMergeCount() != 0) {
    return 29;
  }

  EnrollmentConfigV2 enroll_v2_config;
  enroll_v2_config.embedding_dim = 4;
  EnrollmentStoreV2 store_v2(enroll_v2_config);
  if (!store_v2.UpsertPerson(1, "alice") ||
      !store_v2.UpsertPerson(2, "bob")) {
    return 26;
  }

  PrototypeMetadata metadata;
  metadata.liveness_ok = true;
  metadata.quality_score = 0.9f;
  metadata.decision_score = 0.8f;
  metadata.top1_top2_margin = 0.05f;
  if (!store_v2.AddPrototype(
          1, PrototypeZone::kBaseline,
          std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f}, metadata) ||
      !store_v2.AddPrototype(
          2, PrototypeZone::kVerifiedHistory,
          std::vector<float> {0.9f, 0.1f, 0.0f, 0.0f}, metadata)) {
    return 27;
  }

  FacePipeline pipeline_v2(config);
  if (!pipeline_v2.LoadEnrollment(store_v2)) {
    return 28;
  }

  auto search_v2_result = pipeline_v2.SearchEmbeddingV2(
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!search_v2_result.ok || search_v2_result.hits.empty()) {
    return 29;
  }
  if (search_v2_result.hits[0].person_id != 1) {
    return 30;
  }

  TrackSnapshot snapshot_v2;
  snapshot_v2.track_id = 60;
  snapshot_v2.state = TrackState::kConfirmed;
  snapshot_v2.last_timestamp_ms = static_cast<std::uint64_t>(1000);
  pipeline_v2.SyncTracks(std::vector<TrackSnapshot> {snapshot_v2});

  auto state_v2 = pipeline_v2.UpdateTrackSearch(60, search_v2_result);
  if (state_v2.unlock_ready) {
    return 31;
  }
  state_v2 = pipeline_v2.UpdateTrackSearch(60, search_v2_result);
  if (state_v2.unlock_ready) {
    return 32;
  }
  state_v2 = pipeline_v2.UpdateTrackSearch(60, search_v2_result);
  if (!state_v2.unlock_ready) {
    return 33;
  }
  if (state_v2.stable_person_id != 1) {
    return 34;
  }

  EnrollmentConfigV2 adaptive_config;
  adaptive_config.embedding_dim = 4;
  adaptive_config.max_baseline_prototypes = 1;
  adaptive_config.max_verified_history_prototypes = 5;
  adaptive_config.max_recent_adaptive_prototypes = 3;
  adaptive_config.min_adaptive_quality_score = 0.80f;
  adaptive_config.min_adaptive_decision_score = 0.85f;
  adaptive_config.min_adaptive_margin = 0.05f;
  adaptive_config.require_adaptive_liveness = true;
  adaptive_config.min_verified_quality_score = 0.85f;
  adaptive_config.min_verified_decision_score = 0.90f;
  adaptive_config.min_verified_margin = 0.08f;
  adaptive_config.require_verified_liveness = true;

  EnrollmentStoreV2 adaptive_store(adaptive_config);
  if (!adaptive_store.UpsertPerson(1, "alice") ||
      !adaptive_store.UpsertPerson(2, "bob")) {
    return 35;
  }

  PrototypeMetadata adaptive_baseline;
  adaptive_baseline.timestamp_ms = 1000U;
  adaptive_baseline.quality_score = 0.95f;
  adaptive_baseline.decision_score = 0.95f;
  adaptive_baseline.top1_top2_margin = 0.20f;
  adaptive_baseline.liveness_ok = true;
  adaptive_baseline.manually_enrolled = true;
  if (!adaptive_store.AddPrototype(
          1,
          PrototypeZone::kBaseline,
          std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f},
          adaptive_baseline) ||
      !adaptive_store.AddPrototype(
          2,
          PrototypeZone::kBaseline,
          std::vector<float> {0.0f, 1.0f, 0.0f, 0.0f},
          adaptive_baseline)) {
    return 36;
  }

  PipelineConfig adaptive_pipeline_config = config;
  adaptive_pipeline_config.adaptive_update_min_interval_ms = 1000U;
  FacePipeline adaptive_pipeline(adaptive_pipeline_config);
  if (!adaptive_pipeline.LoadEnrollment(adaptive_store)) {
    return 37;
  }

  TrackSnapshot adaptive_snapshot;
  adaptive_snapshot.track_id = 70;
  adaptive_snapshot.state = TrackState::kConfirmed;
  adaptive_snapshot.last_timestamp_ms = static_cast<std::uint64_t>(1000);
  adaptive_snapshot.face_quality = 0.92f;
  adaptive_pipeline.SyncTracks(std::vector<TrackSnapshot> {adaptive_snapshot});

  const std::vector<float> adaptive_embedding {1.0f, 0.0f, 0.0f, 0.0f};
  const auto adaptive_result = adaptive_pipeline.SearchEmbeddingV2(adaptive_embedding);
  if (!adaptive_result.ok) {
    return 38;
  }

  auto adaptive_state = adaptive_pipeline.UpdateTrackSearch(
      70, adaptive_embedding, adaptive_result);
  adaptive_state = adaptive_pipeline.UpdateTrackSearch(
      70, adaptive_embedding, adaptive_result);
  adaptive_state = adaptive_pipeline.UpdateTrackSearch(
      70, adaptive_embedding, adaptive_result);
  if (!adaptive_state.unlock_ready) {
    return 39;
  }

  const auto update_candidates =
      adaptive_pipeline.GetPrototypeUpdateCandidates(adaptive_store, true);
  if (update_candidates.size() != 1U) {
    return 40;
  }
  if (!update_candidates[0].eligible_recent_adaptive ||
      !update_candidates[0].eligible_verified_history) {
    return 41;
  }

  const int adaptive_updates =
      adaptive_pipeline.ApplyAdaptivePrototypeUpdates(adaptive_store, false, true);
  if (adaptive_updates != 1) {
    return 42;
  }
  if (adaptive_store.PrototypeCount(1, PrototypeZone::kRecentAdaptive) != 1U) {
    return 43;
  }

  const int duplicate_updates =
      adaptive_pipeline.ApplyAdaptivePrototypeUpdates(adaptive_store, false, true);
  if (duplicate_updates != 0) {
    return 44;
  }

  adaptive_snapshot.last_timestamp_ms = static_cast<std::uint64_t>(2500);
  adaptive_pipeline.SyncTracks(std::vector<TrackSnapshot> {adaptive_snapshot});
  adaptive_state = adaptive_pipeline.UpdateTrackSearch(
      70, adaptive_embedding, adaptive_result);
  if (!adaptive_state.unlock_ready) {
    return 45;
  }

  const int verified_updates =
      adaptive_pipeline.ApplyAdaptivePrototypeUpdates(adaptive_store, true, true);
  if (verified_updates != 1) {
    return 46;
  }
  if (adaptive_store.PrototypeCount(1, PrototypeZone::kVerifiedHistory) != 1U) {
    return 47;
  }

  std::ifstream log_in(config.logger_config.file_path);
  if (!log_in.is_open()) {
    return 48;
  }
  const std::string log_content((std::istreambuf_iterator<char>(log_in)),
                                std::istreambuf_iterator<char>());
  if (log_content.find("I/pipeline: short_gap_merge") == std::string::npos) {
    return 49;
  }
  if (log_content.find("D/pipeline: prototype_update_candidate") ==
      std::string::npos) {
    return 50;
  }
  if (log_content.find("I/pipeline: prototype_update_applied") ==
      std::string::npos) {
    return 51;
  }
  std::remove(config.logger_config.file_path.c_str());

  return 0;
}
