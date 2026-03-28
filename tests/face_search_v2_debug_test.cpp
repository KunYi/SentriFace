#include <vector>

#include "search/face_search.hpp"

int main() {
  using sentriface::search::FacePrototypeV2;
  using sentriface::search::FaceSearchV2;
  using sentriface::search::SearchConfig;

  SearchConfig config;
  config.embedding_dim = 4;
  config.top_k = 2;

  FaceSearchV2 search(config);

  FacePrototypeV2 strong;
  strong.person_id = 1;
  strong.prototype_id = 10;
  strong.zone = 0;
  strong.label = "strong";
  strong.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  strong.prototype_weight = 1.0f;

  FacePrototypeV2 medium;
  medium.person_id = 2;
  medium.prototype_id = 20;
  medium.zone = 1;
  medium.label = "medium";
  medium.embedding = {0.8f, 0.2f, 0.0f, 0.0f};
  medium.prototype_weight = 0.8f;

  FacePrototypeV2 weak;
  weak.person_id = 3;
  weak.prototype_id = 30;
  weak.zone = 2;
  weak.label = "weak";
  weak.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  weak.prototype_weight = 0.4f;

  if (!search.AddPrototype(strong) ||
      !search.AddPrototype(medium) ||
      !search.AddPrototype(weak)) {
    return 1;
  }

  const auto result = search.Query(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!result.ok) {
    return 2;
  }
  if (result.prototype_hits.size() != 3U) {
    return 3;
  }

  if (result.prototype_hits[0].prototype_id != 10) {
    return 4;
  }
  if (result.prototype_hits[1].prototype_id != 20) {
    return 5;
  }
  if (result.prototype_hits[2].prototype_id != 30) {
    return 6;
  }

  if (!(result.prototype_hits[0].weighted_score >= result.prototype_hits[1].weighted_score &&
        result.prototype_hits[1].weighted_score >= result.prototype_hits[2].weighted_score)) {
    return 7;
  }

  if (result.hits[0].best_prototype_id != result.prototype_hits[0].prototype_id) {
    return 8;
  }
  if (result.hits[0].best_zone != result.prototype_hits[0].zone) {
    return 9;
  }

  return 0;
}
