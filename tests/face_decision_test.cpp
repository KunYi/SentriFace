#include "decision/face_decision.hpp"
#include "search/face_search.hpp"

int main() {
  using sentriface::decision::DecisionConfig;
  using sentriface::decision::FaceDecisionEngine;
  using sentriface::search::SearchHit;
  using sentriface::search::SearchResult;

  DecisionConfig config;
  config.history_size = 5;
  config.min_consistent_hits = 3;
  config.unlock_threshold = 0.78f;
  config.avg_threshold = 0.72f;
  config.min_margin = 0.03f;

  FaceDecisionEngine engine(config);

  SearchResult weak;
  weak.ok = true;
  weak.hits = {SearchHit {1, "alice", 0.76f}, SearchHit {2, "bob", 0.74f}};
  weak.top1_top2_margin = 0.02f;

  SearchResult strong;
  strong.ok = true;
  strong.hits = {SearchHit {1, "alice", 0.82f}, SearchHit {2, "bob", 0.75f}};
  strong.top1_top2_margin = 0.07f;

  auto state = engine.Update(7, weak);
  if (state.unlock_ready) {
    return 1;
  }

  state = engine.Update(7, strong);
  if (state.unlock_ready) {
    return 2;
  }

  state = engine.Update(7, strong);
  if (state.unlock_ready) {
    return 3;
  }

  state = engine.Update(7, strong);
  if (!state.unlock_ready) {
    return 4;
  }
  if (state.stable_person_id != 1) {
    return 5;
  }
  if (state.consistent_hits < 3) {
    return 6;
  }

  FaceDecisionEngine merge_engine(config);
  auto merged_state = merge_engine.Update(10, strong);
  if (merged_state.unlock_ready) {
    return 7;
  }
  merged_state = merge_engine.Update(10, strong);
  if (merged_state.unlock_ready) {
    return 8;
  }
  merged_state = merge_engine.Update(11, strong);
  if (merged_state.unlock_ready) {
    return 9;
  }
  merged_state = merge_engine.MergeTracks(10, 11);
  if (!merged_state.unlock_ready) {
    return 10;
  }
  if (merged_state.track_id != 11) {
    return 11;
  }

  if (!engine.RemoveTrack(7)) {
    return 12;
  }

  return 0;
}
