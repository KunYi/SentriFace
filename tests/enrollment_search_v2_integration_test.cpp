#include <cmath>
#include <filesystem>
#include <vector>

#include "enroll/enrollment_store.hpp"
#include "search/face_search.hpp"

namespace {

bool AlmostEqual(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}

}  // namespace

int main() {
  using sentriface::enroll::EnrollmentConfigV2;
  using sentriface::enroll::EnrollmentStoreV2;
  using sentriface::enroll::PrototypeMetadata;
  using sentriface::enroll::PrototypeZone;
  using sentriface::search::FaceSearchV2;
  using sentriface::search::FaceSearchV2IndexPackage;
  using sentriface::search::SearchConfig;

  SearchConfig search_config;
  search_config.embedding_dim = 4;
  search_config.top_k = 2;

  EnrollmentConfigV2 enroll_config;
  enroll_config.embedding_dim = 4;
  EnrollmentStoreV2 store(enroll_config);
  PrototypeMetadata metadata;
  metadata.timestamp_ms = 1000U;
  metadata.quality_score = 0.95f;
  metadata.decision_score = 0.90f;
  metadata.top1_top2_margin = 0.20f;
  metadata.liveness_ok = true;
  metadata.manually_enrolled = true;
  if (!store.UpsertPerson(1, "alice") ||
      !store.UpsertPerson(2, "bob") ||
      !store.AddPrototype(
          1, PrototypeZone::kBaseline,
          std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f}, metadata) ||
      !store.AddPrototype(
          1, PrototypeZone::kRecentAdaptive,
          std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f}, metadata) ||
      !store.AddPrototype(
          2, PrototypeZone::kVerifiedHistory,
          std::vector<float> {0.9f, 0.1f, 0.0f, 0.0f}, metadata)) {
    return 1;
  }

  FaceSearchV2IndexPackage index_package;
  const auto build_result = store.BuildSearchIndexPackage(&index_package);
  if (!build_result.ok) {
    return 2;
  }

  const std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() /
      "sentriface_enrollment_search_v2_integration_test";
  std::filesystem::create_directories(temp_dir);
  const std::filesystem::path index_path = temp_dir / "search_index.sfsi";

  FaceSearchV2 package_search(search_config);
  if (!package_search.LoadFromIndexPackage(index_package) ||
      !package_search.SaveIndexPackageBinary(index_path.string())) {
    return 3;
  }

  FaceSearchV2 search(search_config);
  if (!search.LoadFromIndexPackagePath(index_path.string())) {
    return 4;
  }
  const auto result = search.Query(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!result.ok) {
    return 5;
  }
  if (result.hits.size() != 2U) {
    return 6;
  }
  if (result.prototype_hits.size() != 3U) {
    return 7;
  }

  if (result.hits[0].person_id != 1) {
    return 8;
  }
  if (result.hits[0].best_zone != static_cast<int>(PrototypeZone::kBaseline)) {
    return 9;
  }
  if (result.hits[0].score != 1.0f) {
    return 10;
  }

  if (result.prototype_hits[0].zone != static_cast<int>(PrototypeZone::kBaseline)) {
    return 11;
  }
  if (!AlmostEqual(result.prototype_hits[0].weighted_score, 1.0f)) {
    return 12;
  }

  bool found_recent = false;
  bool found_verified = false;
  for (const auto& hit : result.prototype_hits) {
    if (hit.zone == static_cast<int>(PrototypeZone::kRecentAdaptive)) {
      found_recent = true;
      if (!AlmostEqual(hit.weighted_score, 0.6f)) {
        return 13;
      }
    }
    if (hit.zone == static_cast<int>(PrototypeZone::kVerifiedHistory)) {
      found_verified = true;
      if (!(hit.weighted_score > 0.6f)) {
        return 14;
      }
    }
  }

  if (!found_recent || !found_verified) {
    return 15;
  }

  if (!(result.top1_top2_margin > 0.0f)) {
    return 16;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}
