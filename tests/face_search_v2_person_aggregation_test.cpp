#include <cmath>
#include <vector>

#include "search/face_search.hpp"

namespace {

bool AlmostEqual(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}

}  // namespace

int main() {
  using sentriface::search::FacePrototypeV2;
  using sentriface::search::FaceSearchV2;
  using sentriface::search::SearchConfig;

  SearchConfig config;
  config.embedding_dim = 4;
  config.top_k = 3;

  FaceSearchV2 search(config);

  FacePrototypeV2 person1_a;
  person1_a.person_id = 1;
  person1_a.prototype_id = 100;
  person1_a.zone = 0;
  person1_a.label = "p1_base";
  person1_a.embedding = {0.9f, 0.1f, 0.0f, 0.0f};
  person1_a.prototype_weight = 1.0f;

  FacePrototypeV2 person1_b;
  person1_b.person_id = 1;
  person1_b.prototype_id = 101;
  person1_b.zone = 2;
  person1_b.label = "p1_recent";
  person1_b.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  person1_b.prototype_weight = 0.5f;

  FacePrototypeV2 person2_a;
  person2_a.person_id = 2;
  person2_a.prototype_id = 200;
  person2_a.zone = 1;
  person2_a.label = "p2_verified";
  person2_a.embedding = {0.8f, 0.2f, 0.0f, 0.0f};
  person2_a.prototype_weight = 0.8f;

  FacePrototypeV2 person2_b;
  person2_b.person_id = 2;
  person2_b.prototype_id = 201;
  person2_b.zone = 0;
  person2_b.label = "p2_base";
  person2_b.embedding = {0.7f, 0.3f, 0.0f, 0.0f};
  person2_b.prototype_weight = 1.0f;

  if (!search.AddPrototype(person1_a) ||
      !search.AddPrototype(person1_b) ||
      !search.AddPrototype(person2_a) ||
      !search.AddPrototype(person2_b)) {
    return 1;
  }

  const auto result = search.Query(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!result.ok) {
    return 2;
  }
  if (result.hits.size() != 2U) {
    return 3;
  }

  if (result.hits[0].person_id != 1) {
    return 4;
  }
  if (result.hits[0].best_prototype_id != 100) {
    return 5;
  }
  if (result.hits[0].best_zone != 0) {
    return 6;
  }

  if (result.hits[1].person_id != 2) {
    return 7;
  }
  if (result.hits[1].best_prototype_id != 201) {
    return 8;
  }
  if (result.hits[1].best_zone != 0) {
    return 9;
  }

  if (!(result.hits[0].score > result.hits[1].score)) {
    return 10;
  }

  const float expected_margin = result.hits[0].score - result.hits[1].score;
  if (!AlmostEqual(result.top1_top2_margin, expected_margin)) {
    return 11;
  }

  return 0;
}
