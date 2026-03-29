#include <cmath>
#include <filesystem>
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

  SearchConfig kernel_config;
  kernel_config.embedding_dim = 512;
  kernel_config.top_k = 2;

  FaceSearchV2 kernel_search(kernel_config);

  std::vector<float> strong_512(512U, 0.0f);
  std::vector<float> weak_512(512U, 0.0f);
  std::vector<float> query_512(512U, 0.0f);
  strong_512[0] = 3.0f;
  weak_512[0] = 1.0f;
  weak_512[1] = 1.0f;
  query_512[0] = 6.0f;

  FacePrototypeV2 strong_kernel;
  strong_kernel.person_id = 10;
  strong_kernel.prototype_id = 100;
  strong_kernel.zone = 0;
  strong_kernel.label = "strong_kernel";
  strong_kernel.embedding = strong_512;
  strong_kernel.prototype_weight = 1.0f;

  FacePrototypeV2 weak_kernel;
  weak_kernel.person_id = 20;
  weak_kernel.prototype_id = 200;
  weak_kernel.zone = 2;
  weak_kernel.label = "weak_kernel";
  weak_kernel.embedding = weak_512;
  weak_kernel.prototype_weight = 0.5f;

  if (!kernel_search.AddPrototype(strong_kernel) ||
      !kernel_search.AddPrototype(weak_kernel)) {
    return 17;
  }

  const auto kernel_result = kernel_search.Query(query_512);
  if (!kernel_result.ok || kernel_result.hits.size() != 2U ||
      kernel_result.prototype_hits.size() != 2U) {
    return 18;
  }
  if (kernel_result.hits[0].person_id != 10 ||
      kernel_result.hits[0].best_prototype_id != 100) {
    return 19;
  }
  if (!AlmostEqual(kernel_result.prototype_hits[0].raw_score, 1.0f, 1e-4f)) {
    return 20;
  }
  if (!(kernel_result.prototype_hits[0].weighted_score >
        kernel_result.prototype_hits[1].weighted_score)) {
    return 21;
  }

  SearchConfig tie_config;
  tie_config.embedding_dim = 4;
  tie_config.top_k = 1;

  FaceSearchV2 tie_search(tie_config);

  FacePrototypeV2 tie_a;
  tie_a.person_id = 9;
  tie_a.prototype_id = 300;
  tie_a.zone = 0;
  tie_a.label = "tie_a";
  tie_a.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  tie_a.prototype_weight = 1.0f;

  FacePrototypeV2 tie_b;
  tie_b.person_id = 4;
  tie_b.prototype_id = 200;
  tie_b.zone = 1;
  tie_b.label = "tie_b";
  tie_b.embedding = {1.0f, 0.0f, 0.0f, 0.0f};
  tie_b.prototype_weight = 1.0f;

  if (!tie_search.AddPrototype(tie_a) || !tie_search.AddPrototype(tie_b)) {
    return 22;
  }

  const auto tie_result = tie_search.Query(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!tie_result.ok || tie_result.hits.size() != 1U) {
    return 23;
  }
  if (tie_result.hits[0].person_id != 4 ||
      tie_result.hits[0].best_prototype_id != 200) {
    return 24;
  }
  if (tie_result.prototype_hits.size() != 2U ||
      tie_result.prototype_hits[0].prototype_id != 200) {
    return 25;
  }

  FaceSearchV2 rebuild_search(kernel_config);
  std::vector<FacePrototypeV2> rebuild_prototypes;
  rebuild_prototypes.push_back(strong_kernel);
  rebuild_prototypes.push_back(weak_kernel);
  if (!rebuild_search.RebuildFromPrototypes(rebuild_prototypes)) {
    return 26;
  }
  const auto rebuild_result = rebuild_search.Query(query_512);
  if (!rebuild_result.ok || rebuild_result.hits.empty() ||
      rebuild_result.hits[0].person_id != 10) {
    return 27;
  }

  sentriface::search::FaceSearchV2IndexPackage index_package;
  if (!rebuild_search.ExportIndexPackage(&index_package)) {
    return 28;
  }
  if (index_package.entries.size() != 2U ||
      index_package.normalized_matrix.size() != 1024U) {
    return 29;
  }

  const std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() / "sentriface_face_search_v2_test";
  std::filesystem::create_directories(temp_dir);
  const std::filesystem::path index_path = temp_dir / "search_index.sfsi";
  if (!rebuild_search.SaveIndexPackageBinary(index_path.string())) {
    return 30;
  }

  FaceSearchV2 loaded_search(kernel_config);
  if (!loaded_search.LoadFromIndexPackagePath(index_path.string())) {
    return 32;
  }
  const auto loaded_result = loaded_search.Query(query_512);
  if (!loaded_result.ok || loaded_result.hits.size() != kernel_result.hits.size() ||
      loaded_result.hits[0].person_id != kernel_result.hits[0].person_id ||
      !AlmostEqual(loaded_result.hits[0].score, kernel_result.hits[0].score,
                   1e-5f)) {
    return 33;
  }

  const std::filesystem::path built_index_path =
      temp_dir / "built_search_index.sfsi";
  const auto build_and_save_result =
      sentriface::search::BuildAndSaveFaceSearchV2IndexPackageBinary(
          rebuild_prototypes, kernel_config.embedding_dim,
          built_index_path.string());
  if (!build_and_save_result.ok) {
    return 34;
  }
  FaceSearchV2 built_search(kernel_config);
  if (!built_search.LoadFromIndexPackagePath(built_index_path.string())) {
    return 35;
  }
  const auto built_result = built_search.Query(query_512);
  if (!built_result.ok || built_result.hits.empty() ||
      built_result.hits[0].person_id != kernel_result.hits[0].person_id) {
    return 36;
  }

  std::filesystem::remove_all(temp_dir);

  return 0;
}
