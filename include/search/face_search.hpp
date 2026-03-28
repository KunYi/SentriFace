#ifndef SENTRIFACE_SEARCH_FACE_SEARCH_HPP_
#define SENTRIFACE_SEARCH_FACE_SEARCH_HPP_

#include <string>
#include <vector>

namespace sentriface::search {

struct SearchConfig {
  int embedding_dim = 512;
  int top_k = 5;
};

struct FacePrototype {
  int person_id = -1;
  std::string label;
  std::vector<float> embedding;
};

struct FacePrototypeV2 {
  int person_id = -1;
  int prototype_id = -1;
  int zone = 0;
  std::string label;
  std::vector<float> embedding;
  float prototype_weight = 1.0f;
};

struct SearchHit {
  int person_id = -1;
  std::string label;
  float score = 0.0f;
};

struct PrototypeHit {
  int person_id = -1;
  int prototype_id = -1;
  int zone = 0;
  std::string label;
  float raw_score = 0.0f;
  float weighted_score = 0.0f;
};

struct SearchHitV2 {
  int person_id = -1;
  std::string label;
  float score = 0.0f;
  int best_prototype_id = -1;
  int best_zone = 0;
};

struct SearchResult {
  bool ok = false;
  std::vector<SearchHit> hits;
  float top1_top2_margin = 0.0f;
};

struct SearchResultV2 {
  bool ok = false;
  std::vector<SearchHitV2> hits;
  std::vector<PrototypeHit> prototype_hits;
  float top1_top2_margin = 0.0f;
};

class FaceSearch {
 public:
  explicit FaceSearch(const SearchConfig& config = SearchConfig {});

  bool AddPrototype(const FacePrototype& prototype);
  void Clear();
  std::size_t PrototypeCount() const;
  SearchResult Query(const std::vector<float>& embedding) const;

 private:
  float CosineSimilarity(const std::vector<float>& a,
                         const std::vector<float>& b) const;

  SearchConfig config_;
  std::vector<FacePrototype> prototypes_;
};

class FaceSearchV2 {
 public:
  explicit FaceSearchV2(const SearchConfig& config = SearchConfig {});

  bool AddPrototype(const FacePrototypeV2& prototype);
  void Clear();
  std::size_t PrototypeCount() const;
  SearchResultV2 Query(const std::vector<float>& embedding) const;

 private:
  SearchConfig config_;
  std::vector<FacePrototypeV2> prototypes_;
};

SearchResult ToSearchResult(const SearchResultV2& result_v2);

}  // namespace sentriface::search

#endif  // SENTRIFACE_SEARCH_FACE_SEARCH_HPP_
