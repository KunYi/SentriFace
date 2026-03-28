#ifndef SENTRIFACE_DECISION_FACE_DECISION_HPP_
#define SENTRIFACE_DECISION_FACE_DECISION_HPP_

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

#include "search/face_search.hpp"

namespace sentriface::decision {

struct DecisionConfig {
  int history_size = 5;
  int min_consistent_hits = 3;
  float unlock_threshold = 0.78f;
  float avg_threshold = 0.72f;
  float min_margin = 0.03f;
};

struct DecisionVote {
  int person_id = -1;
  float score = 0.0f;
  float margin = 0.0f;
  bool accepted = false;
};

struct DecisionState {
  int track_id = -1;
  int stable_person_id = -1;
  int consistent_hits = 0;
  float average_score = 0.0f;
  float average_margin = 0.0f;
  bool unlock_ready = false;
};

class FaceDecisionEngine {
 public:
  explicit FaceDecisionEngine(const DecisionConfig& config = DecisionConfig {});

  void Reset();
  DecisionState Update(int track_id, const sentriface::search::SearchResult& result);
  DecisionState GetState(int track_id) const;
  DecisionState MergeTracks(int from_track_id, int to_track_id);
  bool RemoveTrack(int track_id);

 private:
  DecisionVote MakeVote(const sentriface::search::SearchResult& result) const;
  DecisionState BuildState(int track_id, const std::deque<DecisionVote>& history) const;

  DecisionConfig config_;
  std::unordered_map<int, std::deque<DecisionVote>> history_;
  std::unordered_map<int, DecisionState> states_;
};

}  // namespace sentriface::decision

#endif  // SENTRIFACE_DECISION_FACE_DECISION_HPP_
