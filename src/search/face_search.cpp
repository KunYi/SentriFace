#include "search/face_search.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace sentriface::search {

namespace {

float CosineSimilarityImpl(const std::vector<float>& a,
                           const std::vector<float>& b) {
  if (a.size() != b.size() || a.empty()) {
    return 0.0f;
  }

  float dot = 0.0f;
  float norm_a = 0.0f;
  float norm_b = 0.0f;
  for (std::size_t i = 0; i < a.size(); ++i) {
    dot += a[i] * b[i];
    norm_a += a[i] * a[i];
    norm_b += b[i] * b[i];
  }

  const float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
  if (denom < 1e-9f) {
    return 0.0f;
  }
  return dot / denom;
}

}  // namespace

FaceSearch::FaceSearch(const SearchConfig& config) : config_(config) {}

bool FaceSearch::AddPrototype(const FacePrototype& prototype) {
  if (static_cast<int>(prototype.embedding.size()) != config_.embedding_dim) {
    return false;
  }
  prototypes_.push_back(prototype);
  return true;
}

void FaceSearch::Clear() { prototypes_.clear(); }

std::size_t FaceSearch::PrototypeCount() const { return prototypes_.size(); }

SearchResult FaceSearch::Query(const std::vector<float>& embedding) const {
  SearchResult result;
  if (static_cast<int>(embedding.size()) != config_.embedding_dim ||
      prototypes_.empty()) {
    return result;
  }

  for (const auto& prototype : prototypes_) {
    SearchHit hit;
    hit.person_id = prototype.person_id;
    hit.label = prototype.label;
    hit.score = CosineSimilarity(embedding, prototype.embedding);
    result.hits.push_back(hit);
  }

  std::sort(result.hits.begin(), result.hits.end(),
            [](const SearchHit& a, const SearchHit& b) {
              return a.score > b.score;
            });

  if (static_cast<int>(result.hits.size()) > config_.top_k) {
    result.hits.resize(config_.top_k);
  }

  if (!result.hits.empty()) {
    result.ok = true;
    if (result.hits.size() >= 2) {
      result.top1_top2_margin = result.hits[0].score - result.hits[1].score;
    } else {
      result.top1_top2_margin = result.hits[0].score;
    }
  }

  return result;
}

float FaceSearch::CosineSimilarity(const std::vector<float>& a,
                                   const std::vector<float>& b) const {
  return CosineSimilarityImpl(a, b);
}

FaceSearchV2::FaceSearchV2(const SearchConfig& config) : config_(config) {}

bool FaceSearchV2::AddPrototype(const FacePrototypeV2& prototype) {
  if (static_cast<int>(prototype.embedding.size()) != config_.embedding_dim ||
      prototype.person_id < 0 || prototype.prototype_id < 0 ||
      prototype.prototype_weight < 0.0f) {
    return false;
  }
  prototypes_.push_back(prototype);
  return true;
}

void FaceSearchV2::Clear() { prototypes_.clear(); }

std::size_t FaceSearchV2::PrototypeCount() const { return prototypes_.size(); }

SearchResultV2 FaceSearchV2::Query(const std::vector<float>& embedding) const {
  SearchResultV2 result;
  if (static_cast<int>(embedding.size()) != config_.embedding_dim ||
      prototypes_.empty()) {
    return result;
  }

  struct PersonBestHit {
    std::string label;
    float score = 0.0f;
    int best_prototype_id = -1;
    int best_zone = 0;
    bool initialized = false;
  };

  std::unordered_map<int, PersonBestHit> best_by_person;

  for (const auto& prototype : prototypes_) {
    PrototypeHit hit;
    hit.person_id = prototype.person_id;
    hit.prototype_id = prototype.prototype_id;
    hit.zone = prototype.zone;
    hit.label = prototype.label;
    hit.raw_score = CosineSimilarityImpl(embedding, prototype.embedding);
    hit.weighted_score = hit.raw_score * prototype.prototype_weight;
    result.prototype_hits.push_back(hit);

    auto& best = best_by_person[prototype.person_id];
    if (!best.initialized || hit.weighted_score > best.score) {
      best.label = prototype.label;
      best.score = hit.weighted_score;
      best.best_prototype_id = prototype.prototype_id;
      best.best_zone = prototype.zone;
      best.initialized = true;
    }
  }

  std::sort(result.prototype_hits.begin(), result.prototype_hits.end(),
            [](const PrototypeHit& a, const PrototypeHit& b) {
              return a.weighted_score > b.weighted_score;
            });

  for (const auto& entry : best_by_person) {
    SearchHitV2 hit;
    hit.person_id = entry.first;
    hit.label = entry.second.label;
    hit.score = entry.second.score;
    hit.best_prototype_id = entry.second.best_prototype_id;
    hit.best_zone = entry.second.best_zone;
    result.hits.push_back(hit);
  }

  std::sort(result.hits.begin(), result.hits.end(),
            [](const SearchHitV2& a, const SearchHitV2& b) {
              return a.score > b.score;
            });

  if (static_cast<int>(result.hits.size()) > config_.top_k) {
    result.hits.resize(config_.top_k);
  }

  if (!result.hits.empty()) {
    result.ok = true;
    if (result.hits.size() >= 2U) {
      result.top1_top2_margin = result.hits[0].score - result.hits[1].score;
    } else {
      result.top1_top2_margin = result.hits[0].score;
    }
  }

  return result;
}

SearchResult ToSearchResult(const SearchResultV2& result_v2) {
  SearchResult result;
  result.ok = result_v2.ok;
  result.top1_top2_margin = result_v2.top1_top2_margin;
  result.hits.reserve(result_v2.hits.size());
  for (const auto& hit_v2 : result_v2.hits) {
    SearchHit hit;
    hit.person_id = hit_v2.person_id;
    hit.label = hit_v2.label;
    hit.score = hit_v2.score;
    result.hits.push_back(hit);
  }
  return result;
}

}  // namespace sentriface::search
