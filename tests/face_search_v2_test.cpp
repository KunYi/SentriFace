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
  config.top_k = 2;

  FaceSearchV2 search(config);

  FacePrototypeV2 alice_baseline;
  alice_baseline.person_id = 1;
  alice_baseline.prototype_id = 10;
  alice_baseline.zone = 0;
  alice_baseline.label = "alice_baseline";
  alice_baseline.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  alice_baseline.prototype_weight = 1.0f;

  FacePrototypeV2 alice_recent;
  alice_recent.person_id = 1;
  alice_recent.prototype_id = 11;
  alice_recent.zone = 2;
  alice_recent.label = "alice_recent";
  alice_recent.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  alice_recent.prototype_weight = 0.6f;

  FacePrototypeV2 bob_verified;
  bob_verified.person_id = 2;
  bob_verified.prototype_id = 20;
  bob_verified.zone = 1;
  bob_verified.label = "bob_verified";
  bob_verified.embedding = {0.9f, 0.1f, 0.0f, 0.0f};
  bob_verified.prototype_weight = 0.8f;

  if (!search.AddPrototype(alice_baseline) ||
      !search.AddPrototype(alice_recent) ||
      !search.AddPrototype(bob_verified)) {
    return 1;
  }

  if (search.PrototypeCount() != 3U) {
    return 2;
  }

  const auto result = search.Query(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!result.ok) {
    return 3;
  }
  if (result.prototype_hits.size() != 3U) {
    return 4;
  }
  if (result.hits.size() != 2U) {
    return 5;
  }
  if (result.hits[0].person_id != 1) {
    return 6;
  }
  if (result.hits[0].best_prototype_id != 10) {
    return 7;
  }
  if (result.hits[0].best_zone != 0) {
    return 8;
  }
  if (!(result.hits[0].score > result.hits[1].score)) {
    return 9;
  }
  if (!AlmostEqual(result.prototype_hits[0].raw_score, 1.0f)) {
    return 10;
  }
  if (!AlmostEqual(result.prototype_hits[0].weighted_score, 1.0f)) {
    return 11;
  }
  if (result.prototype_hits[1].prototype_id != 20) {
    return 12;
  }
  if (!(result.top1_top2_margin > 0.0f)) {
    return 13;
  }

  FacePrototypeV2 invalid;
  invalid.person_id = 3;
  invalid.prototype_id = 30;
  invalid.embedding = {1.0f, 0.0f};
  invalid.prototype_weight = 1.0f;
  if (search.AddPrototype(invalid)) {
    return 14;
  }

  const auto invalid_query = search.Query(std::vector<float> {1.0f, 0.0f});
  if (invalid_query.ok) {
    return 15;
  }

  search.Clear();
  if (search.PrototypeCount() != 0U) {
    return 16;
  }

  return 0;
}
