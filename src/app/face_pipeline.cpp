#include "app/face_pipeline.hpp"

#include <algorithm>
#include <cmath>

#undef SENTRIFACE_LOG_TAG
#define SENTRIFACE_LOG_TAG "pipeline"

namespace sentriface::app {

namespace {

float RectArea(const sentriface::tracker::RectF& rect) {
  return rect.w > 0.0f && rect.h > 0.0f ? rect.w * rect.h : 0.0f;
}

float RectDiag(const sentriface::tracker::RectF& rect) {
  return std::sqrt(rect.w * rect.w + rect.h * rect.h);
}

sentriface::tracker::Point2f RectCenter(const sentriface::tracker::RectF& rect) {
  return sentriface::tracker::Point2f {
      rect.x + rect.w * 0.5f,
      rect.y + rect.h * 0.5f,
  };
}

}  // namespace

FacePipeline::FacePipeline(const PipelineConfig& config)
    : config_(config),
      search_(config.search),
      search_v2_(config.search),
      decision_(config.decision),
      logger_(config.logger_config) {}

void FacePipeline::Reset() {
  search_.Clear();
  search_v2_.Clear();
  decision_.Reset();
  snapshots_.clear();
  recent_removed_.clear();
  track_embeddings_.clear();
  last_prototype_update_timestamp_ms_.clear();
  last_sync_timestamp_ms_ = 0;
  short_gap_merge_count_ = 0;
}

void FacePipeline::SyncTracks(
    const std::vector<sentriface::tracker::TrackSnapshot>& snapshots) {
  std::unordered_map<int, sentriface::tracker::TrackSnapshot> next;
  next.reserve(snapshots.size());
  std::uint64_t current_timestamp_ms = last_sync_timestamp_ms_;
  for (const auto& snapshot : snapshots) {
    next[snapshot.track_id] = snapshot;
    if (snapshot.last_timestamp_ms > current_timestamp_ms) {
      current_timestamp_ms = snapshot.last_timestamp_ms;
    }
  }

  for (const auto& entry : snapshots_) {
    if (next.find(entry.first) == next.end()) {
      const auto decision_state = decision_.GetState(entry.first);
      if (config_.enable_short_gap_merge &&
          decision_state.stable_person_id >= 0 &&
          entry.second.last_timestamp_ms > 0) {
        RecentRemovedTrack removed;
        removed.snapshot = entry.second;
        removed.decision = decision_state;
        const auto embedding_it = track_embeddings_.find(entry.first);
        if (embedding_it != track_embeddings_.end()) {
          removed.last_embedding = embedding_it->second;
        }
        recent_removed_[entry.first] = removed;
      } else {
        decision_.RemoveTrack(entry.first);
      }
      track_embeddings_.erase(entry.first);
    }
  }

  snapshots_ = std::move(next);
  last_sync_timestamp_ms_ = current_timestamp_ms;
  CleanupRecentRemoved(current_timestamp_ms);
}

bool FacePipeline::LoadEnrollment(const sentriface::enroll::EnrollmentStore& store) {
  search_.Clear();
  bool ok = true;
  for (const auto& prototype : store.ExportPrototypes()) {
    if (!search_.AddPrototype(prototype)) {
      ok = false;
    }
  }
  return ok;
}

bool FacePipeline::LoadEnrollment(const sentriface::enroll::EnrollmentStoreV2& store) {
  search_v2_.Clear();
  bool ok = true;
  for (const auto& prototype : store.ExportWeightedPrototypes()) {
    if (!search_v2_.AddPrototype(prototype)) {
      ok = false;
    }
  }
  return ok;
}

sentriface::search::SearchResult FacePipeline::SearchEmbedding(
    const std::vector<float>& embedding) const {
  return search_.Query(embedding);
}

sentriface::search::SearchResultV2 FacePipeline::SearchEmbeddingV2(
    const std::vector<float>& embedding) const {
  return search_v2_.Query(embedding);
}

sentriface::decision::DecisionState FacePipeline::UpdateTrackSearch(
    int track_id, const sentriface::search::SearchResult& result) {
  static const std::vector<float> kEmptyEmbedding;
  return UpdateTrackSearch(track_id, kEmptyEmbedding, result);
}

sentriface::decision::DecisionState FacePipeline::UpdateTrackSearch(
    int track_id, const sentriface::search::SearchResultV2& result) {
  return UpdateTrackSearch(track_id, sentriface::search::ToSearchResult(result));
}

sentriface::decision::DecisionState FacePipeline::UpdateTrackSearch(
    int track_id,
    const std::vector<float>& embedding,
    const sentriface::search::SearchResult& result) {
  if (!embedding.empty()) {
    track_embeddings_[track_id] = embedding;
  }
  auto state = decision_.Update(track_id, result);
  if (!config_.enable_short_gap_merge) {
    return state;
  }

  const auto current_snapshot_it = snapshots_.find(track_id);
  if (current_snapshot_it == snapshots_.end()) {
    return state;
  }

  int matched_track_id = -1;
  std::uint64_t best_removed_ts = 0;
  for (const auto& entry : recent_removed_) {
    const auto& removed = entry.second;
    if (!IsShortGapMergeCandidate(
            removed.snapshot,
            current_snapshot_it->second,
            removed.decision,
            state,
            removed.last_embedding,
            embedding)) {
      continue;
    }
    if (removed.snapshot.last_timestamp_ms >= best_removed_ts) {
      best_removed_ts = removed.snapshot.last_timestamp_ms;
      matched_track_id = entry.first;
    }
  }

  for (const auto& entry : snapshots_) {
    if (entry.first == track_id) {
      continue;
    }
    const auto& other_snapshot = entry.second;
    if (other_snapshot.state == sentriface::tracker::TrackState::kConfirmed) {
      continue;
    }
    const auto other_state = decision_.GetState(entry.first);
    const auto other_embedding_it = track_embeddings_.find(entry.first);
    static const std::vector<float> kEmptyEmbedding;
    if (!IsShortGapMergeCandidate(
            other_snapshot,
            current_snapshot_it->second,
            other_state,
            state,
            other_embedding_it != track_embeddings_.end() ? other_embedding_it->second
                                                          : kEmptyEmbedding,
            embedding)) {
      continue;
    }
    if (other_snapshot.last_timestamp_ms >= best_removed_ts) {
      best_removed_ts = other_snapshot.last_timestamp_ms;
      matched_track_id = entry.first;
    }
  }

  if (matched_track_id >= 0) {
    state = decision_.MergeTracks(matched_track_id, track_id);
    recent_removed_.erase(matched_track_id);
    const auto source_embedding_it = track_embeddings_.find(matched_track_id);
    if (embedding.empty() && source_embedding_it != track_embeddings_.end()) {
      track_embeddings_[track_id] = source_embedding_it->second;
    }
    track_embeddings_.erase(matched_track_id);
    short_gap_merge_count_ += 1;
    SENTRIFACE_LOGI(
        logger_,
        "short_gap_merge"
            << " previous_track_id=" << matched_track_id
            << " current_track_id=" << track_id
            << " person_id=" << state.stable_person_id
            << " merge_count=" << short_gap_merge_count_);
  }

  return state;
}

sentriface::decision::DecisionState FacePipeline::UpdateTrackSearch(
    int track_id,
    const std::vector<float>& embedding,
    const sentriface::search::SearchResultV2& result) {
  return UpdateTrackSearch(
      track_id, embedding, sentriface::search::ToSearchResult(result));
}

std::vector<PipelineTrackState> FacePipeline::GetTrackStates() const {
  std::vector<PipelineTrackState> out;
  out.reserve(snapshots_.size());
  for (const auto& entry : snapshots_) {
    PipelineTrackState state;
    state.snapshot = entry.second;
    state.decision = decision_.GetState(entry.first);
    out.push_back(state);
  }
  return out;
}

std::vector<PipelineTrackState> FacePipeline::GetUnlockReadyTracks() const {
  std::vector<PipelineTrackState> out;
  for (const auto& entry : snapshots_) {
    const int track_id = entry.first;
    const auto state = decision_.GetState(track_id);
    if (!state.unlock_ready) {
      continue;
    }
    PipelineTrackState pipeline_state;
    pipeline_state.snapshot = entry.second;
    pipeline_state.decision = state;
    out.push_back(pipeline_state);
  }
  return out;
}

std::vector<PrototypeUpdateCandidate> FacePipeline::GetPrototypeUpdateCandidates(
    const sentriface::enroll::EnrollmentStoreV2& store,
    bool liveness_ok) const {
  std::vector<PrototypeUpdateCandidate> out;
  const auto unlock_ready = GetUnlockReadyTracks();
  out.reserve(unlock_ready.size());
  for (const auto& ready : unlock_ready) {
    const auto embedding_it = track_embeddings_.find(ready.snapshot.track_id);
    if (embedding_it == track_embeddings_.end() ||
        embedding_it->second.empty() ||
        ready.decision.stable_person_id < 0) {
      continue;
    }

    PrototypeUpdateCandidate candidate;
    candidate.track_id = ready.snapshot.track_id;
    candidate.person_id = ready.decision.stable_person_id;
    candidate.embedding = embedding_it->second;
    candidate.metadata.timestamp_ms =
        static_cast<std::uint32_t>(ready.snapshot.last_timestamp_ms);
    candidate.metadata.quality_score = ready.snapshot.face_quality;
    candidate.metadata.decision_score = ready.decision.average_score;
    candidate.metadata.top1_top2_margin = ready.decision.average_margin;
    candidate.metadata.liveness_ok = liveness_ok;
    candidate.metadata.manually_enrolled = false;
    candidate.recent_adaptive_diagnostic =
        store.DiagnoseAdaptivePrototype(candidate.metadata);
    candidate.verified_history_diagnostic =
        store.DiagnoseVerifiedHistoryPrototype(candidate.metadata);
    candidate.eligible_recent_adaptive =
        candidate.recent_adaptive_diagnostic.accepted;
    candidate.eligible_verified_history =
        candidate.verified_history_diagnostic.accepted;
    const auto ts_it =
        last_prototype_update_timestamp_ms_.find(candidate.person_id);
    if (ts_it != last_prototype_update_timestamp_ms_.end() &&
        candidate.metadata.timestamp_ms >= ts_it->second &&
        candidate.metadata.timestamp_ms - ts_it->second <
            config_.adaptive_update_min_interval_ms) {
      candidate.cooldown_blocked = true;
    }
    SENTRIFACE_LOGD(
        logger_,
        "prototype_update_candidate"
            << " track_id=" << candidate.track_id
            << " person_id=" << candidate.person_id
            << " quality=" << candidate.metadata.quality_score
            << " decision_score=" << candidate.metadata.decision_score
            << " margin=" << candidate.metadata.top1_top2_margin
            << " recent_ok=" << (candidate.eligible_recent_adaptive ? 1 : 0)
            << " verified_ok=" << (candidate.eligible_verified_history ? 1 : 0)
            << " cooldown_blocked=" << (candidate.cooldown_blocked ? 1 : 0));
    out.push_back(candidate);
  }
  return out;
}

int FacePipeline::ApplyAdaptivePrototypeUpdates(
    sentriface::enroll::EnrollmentStoreV2& store,
    bool promote_to_verified,
    bool liveness_ok) {
  int updates_applied = 0;
  const auto candidates = GetPrototypeUpdateCandidates(store, liveness_ok);
  for (const auto& candidate : candidates) {
    if (candidate.cooldown_blocked) {
      SENTRIFACE_LOGD(
          logger_,
          "prototype_update_skipped"
              << " person_id=" << candidate.person_id
              << " track_id=" << candidate.track_id
              << " reason=cooldown_active");
      continue;
    }

    const bool allowed = promote_to_verified
        ? candidate.eligible_verified_history
        : candidate.eligible_recent_adaptive;
    if (!allowed) {
      const auto& diagnostic = promote_to_verified
          ? candidate.verified_history_diagnostic
          : candidate.recent_adaptive_diagnostic;
      SENTRIFACE_LOGD(
          logger_,
          "prototype_update_skipped"
              << " person_id=" << candidate.person_id
              << " track_id=" << candidate.track_id
              << " reason=" << diagnostic.rejection_reason);
      continue;
    }

    const bool ok = promote_to_verified
        ? store.AddVerifiedHistoryPrototype(
              candidate.person_id, candidate.embedding, candidate.metadata)
        : store.AddRecentAdaptivePrototype(
              candidate.person_id, candidate.embedding, candidate.metadata);
    if (!ok) {
      SENTRIFACE_LOGW(
          logger_,
          "prototype_update_failed"
              << " person_id=" << candidate.person_id
              << " track_id=" << candidate.track_id
              << " target_zone="
              << (promote_to_verified ? "verified_history" : "recent_adaptive"));
      continue;
    }

    last_prototype_update_timestamp_ms_[candidate.person_id] =
        candidate.metadata.timestamp_ms;
    updates_applied += 1;
    SENTRIFACE_LOGI(
        logger_,
        "prototype_update_applied"
            << " person_id=" << candidate.person_id
            << " track_id=" << candidate.track_id
            << " target_zone="
            << (promote_to_verified ? "verified_history" : "recent_adaptive")
            << " total_updates=" << updates_applied);
  }
  return updates_applied;
}

int FacePipeline::GetShortGapMergeCount() const {
  return short_gap_merge_count_;
}

bool FacePipeline::IsShortGapMergeCandidate(
    const sentriface::tracker::TrackSnapshot& previous,
    const sentriface::tracker::TrackSnapshot& current,
    const sentriface::decision::DecisionState& previous_decision,
    const sentriface::decision::DecisionState& current_decision,
    const std::vector<float>& previous_embedding,
    const std::vector<float>& current_embedding) const {
  if (!config_.enable_short_gap_merge) {
    return false;
  }
  if (previous_decision.consistent_hits < config_.short_gap_merge_min_previous_hits) {
    return false;
  }
  if (current.last_timestamp_ms < previous.last_timestamp_ms) {
    return false;
  }

  const std::uint64_t gap_ms = current.last_timestamp_ms - previous.last_timestamp_ms;
  if (gap_ms > config_.short_gap_merge_window_ms) {
    return false;
  }

  const float previous_diag = std::max(1.0f, RectDiag(previous.bbox));
  const auto previous_center = RectCenter(previous.bbox);
  const auto current_center = RectCenter(current.bbox);
  const float dx = current_center.x - previous_center.x;
  const float dy = current_center.y - previous_center.y;
  const float center_distance = std::sqrt(dx * dx + dy * dy);
  if (center_distance >
      config_.short_gap_merge_center_gate_ratio * previous_diag) {
    return false;
  }

  const float previous_area = std::max(1.0f, RectArea(previous.bbox));
  const float current_area = std::max(1.0f, RectArea(current.bbox));
  const float area_ratio = current_area / previous_area;
  if (area_ratio < config_.short_gap_merge_area_ratio_min ||
      area_ratio > config_.short_gap_merge_area_ratio_max) {
    return false;
  }

  const bool same_person =
      previous_decision.stable_person_id >= 0 &&
      current_decision.stable_person_id >= 0 &&
      previous_decision.stable_person_id == current_decision.stable_person_id;
  if (same_person) {
    return true;
  }

  if (!config_.enable_embedding_assisted_relink ||
      previous_embedding.empty() ||
      current_embedding.empty()) {
    return false;
  }

  return CosineSimilarity(previous_embedding, current_embedding) >=
         config_.embedding_relink_similarity_min;
}

float FacePipeline::CosineSimilarity(const std::vector<float>& a,
                                     const std::vector<float>& b) const {
  if (a.empty() || a.size() != b.size()) {
    return -1.0f;
  }

  float dot = 0.0f;
  float norm_a = 0.0f;
  float norm_b = 0.0f;
  for (std::size_t i = 0; i < a.size(); ++i) {
    dot += a[i] * b[i];
    norm_a += a[i] * a[i];
    norm_b += b[i] * b[i];
  }
  if (norm_a <= 0.0f || norm_b <= 0.0f) {
    return -1.0f;
  }
  return dot / std::sqrt(norm_a * norm_b);
}

void FacePipeline::CleanupRecentRemoved(std::uint64_t current_timestamp_ms) {
  std::vector<int> expired_ids;
  expired_ids.reserve(recent_removed_.size());
  for (const auto& entry : recent_removed_) {
    const auto& removed = entry.second;
    if (current_timestamp_ms <= removed.snapshot.last_timestamp_ms) {
      continue;
    }
    const std::uint64_t gap_ms =
        current_timestamp_ms - removed.snapshot.last_timestamp_ms;
    if (gap_ms > config_.short_gap_merge_window_ms) {
      expired_ids.push_back(entry.first);
    }
  }

  for (const int track_id : expired_ids) {
    decision_.RemoveTrack(track_id);
    recent_removed_.erase(track_id);
    track_embeddings_.erase(track_id);
  }
}

}  // namespace sentriface::app
