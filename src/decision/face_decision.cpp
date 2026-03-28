#include "decision/face_decision.hpp"

#include <algorithm>

namespace sentriface::decision {

FaceDecisionEngine::FaceDecisionEngine(const DecisionConfig& config)
    : config_(config) {}

void FaceDecisionEngine::Reset() {
  history_.clear();
  states_.clear();
}

DecisionState FaceDecisionEngine::Update(
    int track_id, const sentriface::search::SearchResult& result) {
  auto& queue = history_[track_id];
  queue.push_back(MakeVote(result));
  while (static_cast<int>(queue.size()) > config_.history_size) {
    queue.pop_front();
  }
  DecisionState state = BuildState(track_id, queue);
  states_[track_id] = state;
  return state;
}

DecisionState FaceDecisionEngine::GetState(int track_id) const {
  const auto it = states_.find(track_id);
  if (it == states_.end()) {
    DecisionState state;
    state.track_id = track_id;
    return state;
  }
  return it->second;
}

DecisionState FaceDecisionEngine::MergeTracks(int from_track_id, int to_track_id) {
  if (from_track_id == to_track_id) {
    return GetState(to_track_id);
  }

  const auto from_it = history_.find(from_track_id);
  if (from_it == history_.end()) {
    return GetState(to_track_id);
  }

  auto& target_queue = history_[to_track_id];
  std::deque<DecisionVote> merged;
  merged.insert(merged.end(), from_it->second.begin(), from_it->second.end());
  merged.insert(merged.end(), target_queue.begin(), target_queue.end());
  while (static_cast<int>(merged.size()) > config_.history_size) {
    merged.pop_front();
  }

  target_queue = std::move(merged);
  history_.erase(from_it);
  states_.erase(from_track_id);

  DecisionState state = BuildState(to_track_id, target_queue);
  states_[to_track_id] = state;
  return state;
}

bool FaceDecisionEngine::RemoveTrack(int track_id) {
  const bool removed_history = history_.erase(track_id) > 0U;
  const bool removed_state = states_.erase(track_id) > 0U;
  return removed_history || removed_state;
}

DecisionVote FaceDecisionEngine::MakeVote(
    const sentriface::search::SearchResult& result) const {
  DecisionVote vote;
  if (!result.ok || result.hits.empty()) {
    return vote;
  }

  vote.person_id = result.hits[0].person_id;
  vote.score = result.hits[0].score;
  vote.margin = result.top1_top2_margin;
  vote.accepted = vote.score >= config_.unlock_threshold &&
                  vote.margin >= config_.min_margin;
  return vote;
}

DecisionState FaceDecisionEngine::BuildState(
    int track_id, const std::deque<DecisionVote>& history) const {
  DecisionState state;
  state.track_id = track_id;

  if (history.empty()) {
    return state;
  }

  std::unordered_map<int, int> counts;
  float score_sum = 0.0f;
  float margin_sum = 0.0f;
  int accepted_count = 0;

  for (const DecisionVote& vote : history) {
    if (vote.person_id < 0) {
      continue;
    }
    counts[vote.person_id] += 1;
    score_sum += vote.score;
    margin_sum += vote.margin;
    if (vote.accepted) {
      accepted_count += 1;
    }
  }

  int best_person_id = -1;
  int best_count = 0;
  for (const auto& entry : counts) {
    if (entry.second > best_count) {
      best_person_id = entry.first;
      best_count = entry.second;
    }
  }

  state.stable_person_id = best_person_id;
  state.consistent_hits = best_count;

  int scored_votes = 0;
  for (const DecisionVote& vote : history) {
    if (vote.person_id < 0) {
      continue;
    }
    scored_votes += 1;
  }

  if (scored_votes > 0) {
    state.average_score = score_sum / static_cast<float>(scored_votes);
    state.average_margin = margin_sum / static_cast<float>(scored_votes);
  }

  state.unlock_ready =
      state.stable_person_id >= 0 &&
      state.consistent_hits >= config_.min_consistent_hits &&
      state.average_score >= config_.avg_threshold &&
      state.average_margin >= config_.min_margin &&
      accepted_count >= config_.min_consistent_hits;
  return state;
}

}  // namespace sentriface::decision
