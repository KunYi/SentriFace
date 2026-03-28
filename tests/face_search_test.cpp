#include <cmath>
#include <vector>

#include "search/face_search.hpp"

namespace {

bool AlmostEqual(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}

}  // namespace

int main() {
  using sentriface::search::FacePrototype;
  using sentriface::search::FaceSearch;
  using sentriface::search::SearchConfig;

  SearchConfig config;
  config.embedding_dim = 4;
  config.top_k = 2;

  FaceSearch search(config);

  FacePrototype alice_a {1, "alice_a", {1.0f, 0.0f, 0.0f, 0.0f}};
  FacePrototype alice_b {1, "alice_b", {0.8f, 0.2f, 0.0f, 0.0f}};
  FacePrototype bob {2, "bob", {0.0f, 1.0f, 0.0f, 0.0f}};

  if (!search.AddPrototype(alice_a) ||
      !search.AddPrototype(alice_b) ||
      !search.AddPrototype(bob)) {
    return 1;
  }

  if (search.PrototypeCount() != 3U) {
    return 2;
  }

  const auto result = search.Query(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!result.ok) {
    return 3;
  }
  if (result.hits.size() != 2U) {
    return 4;
  }
  if (result.hits[0].person_id != 1) {
    return 5;
  }
  if (!(result.hits[0].score >= result.hits[1].score)) {
    return 6;
  }
  if (!(result.top1_top2_margin >= 0.0f)) {
    return 7;
  }

  const auto invalid = search.Query(std::vector<float> {1.0f, 0.0f});
  if (invalid.ok) {
    return 8;
  }

  search.Clear();
  if (search.PrototypeCount() != 0U) {
    return 9;
  }

  return 0;
}
