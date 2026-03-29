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

  SearchConfig kernel_config;
  kernel_config.embedding_dim = 512;
  kernel_config.top_k = 2;

  FaceSearch kernel_search(kernel_config);
  std::vector<float> alice_512(512U, 0.0f);
  std::vector<float> bob_512(512U, 0.0f);
  std::vector<float> query_512(512U, 0.0f);
  alice_512[0] = 4.0f;
  bob_512[1] = 2.0f;
  query_512[0] = 8.0f;

  if (!kernel_search.AddPrototype(FacePrototype {10, "alice512", alice_512}) ||
      !kernel_search.AddPrototype(FacePrototype {20, "bob512", bob_512})) {
    return 10;
  }

  const auto kernel_result = kernel_search.Query(query_512);
  if (!kernel_result.ok || kernel_result.hits.size() != 2U) {
    return 11;
  }
  if (kernel_result.hits[0].person_id != 10) {
    return 12;
  }
  if (!AlmostEqual(kernel_result.hits[0].score, 1.0f, 1e-4f)) {
    return 13;
  }
  if (!AlmostEqual(kernel_result.hits[1].score, 0.0f, 1e-4f)) {
    return 14;
  }

  SearchConfig tie_config;
  tie_config.embedding_dim = 4;
  tie_config.top_k = 2;

  FaceSearch tie_search(tie_config);
  if (!tie_search.AddPrototype(FacePrototype {5, "late", {1.0f, 0.0f, 0.0f, 0.0f}}) ||
      !tie_search.AddPrototype(FacePrototype {3, "early", {1.0f, 0.0f, 0.0f, 0.0f}}) ||
      !tie_search.AddPrototype(FacePrototype {7, "third", {0.0f, 1.0f, 0.0f, 0.0f}})) {
    return 15;
  }

  const auto tie_result = tie_search.Query(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!tie_result.ok || tie_result.hits.size() != 2U) {
    return 16;
  }
  if (tie_result.hits[0].person_id != 3 || tie_result.hits[1].person_id != 5) {
    return 17;
  }

  FaceSearch rebuild_search(kernel_config);
  std::vector<FacePrototype> rebuild_prototypes;
  rebuild_prototypes.push_back(FacePrototype {30, "rebuild_a", alice_512});
  rebuild_prototypes.push_back(FacePrototype {40, "rebuild_b", bob_512});
  if (!rebuild_search.RebuildFromPrototypes(rebuild_prototypes)) {
    return 18;
  }
  const auto rebuild_result = rebuild_search.Query(query_512);
  if (!rebuild_result.ok || rebuild_result.hits.empty() ||
      rebuild_result.hits[0].person_id != 30) {
    return 19;
  }

  return 0;
}
