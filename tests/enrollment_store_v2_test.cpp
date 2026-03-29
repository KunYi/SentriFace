#include <cmath>
#include <filesystem>
#include <vector>

#include "enroll/enrollment_store.hpp"
#include "search/face_search.hpp"

int main() {
  using sentriface::enroll::EnrollmentConfigV2;
  using sentriface::enroll::EnrollmentStoreV2;
  using sentriface::enroll::PrototypeMetadata;
  using sentriface::enroll::PrototypeZone;

  EnrollmentConfigV2 config;
  config.embedding_dim = 4;
  config.max_baseline_prototypes = 1;
  config.max_verified_history_prototypes = 5;
  config.max_recent_adaptive_prototypes = 2;
  config.baseline_weight = 1.0f;
  config.verified_history_weight = 0.8f;
  config.recent_adaptive_weight = 0.6f;
  config.min_adaptive_quality_score = 0.80f;
  config.min_adaptive_decision_score = 0.85f;
  config.min_adaptive_margin = 0.05f;
  config.require_adaptive_liveness = true;
  config.min_verified_quality_score = 0.85f;
  config.min_verified_decision_score = 0.90f;
  config.min_verified_margin = 0.08f;
  config.require_verified_liveness = true;

  EnrollmentStoreV2 store(config);
  if (!store.UpsertPerson(1, "alice")) {
    return 1;
  }

  PrototypeMetadata metadata;
  metadata.quality_score = 0.9f;
  metadata.decision_score = 0.8f;
  metadata.top1_top2_margin = 0.1f;
  metadata.liveness_ok = true;

  if (!store.AddPrototype(1, PrototypeZone::kBaseline,
                          std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f}, metadata)) {
    return 2;
  }
  if (store.AddPrototype(1, PrototypeZone::kBaseline,
                         std::vector<float> {0.9f, 0.1f, 0.0f, 0.0f}, metadata)) {
    return 3;
  }

  for (int i = 0; i < 6; ++i) {
    metadata.timestamp_ms = static_cast<std::uint32_t>(100 + i);
    if (!store.AddPrototype(
            1, PrototypeZone::kVerifiedHistory,
            std::vector<float> {0.8f, 0.2f, 0.0f, static_cast<float>(i)}, metadata)) {
      return 4;
    }
  }
  if (store.PrototypeCount(1, PrototypeZone::kVerifiedHistory) != 5U) {
    return 5;
  }

  for (int i = 0; i < 3; ++i) {
    metadata.timestamp_ms = static_cast<std::uint32_t>(200 + i);
    if (!store.AddPrototype(
            1, PrototypeZone::kRecentAdaptive,
            std::vector<float> {0.7f, 0.3f, static_cast<float>(i), 0.0f}, metadata)) {
      return 6;
    }
  }
  if (store.PrototypeCount(1, PrototypeZone::kRecentAdaptive) != 2U) {
    return 7;
  }

  PrototypeMetadata weak_metadata;
  weak_metadata.timestamp_ms = 300U;
  weak_metadata.quality_score = 0.79f;
  weak_metadata.decision_score = 0.90f;
  weak_metadata.top1_top2_margin = 0.10f;
  weak_metadata.liveness_ok = true;
  if (store.ShouldAcceptAdaptivePrototype(weak_metadata)) {
    return 8;
  }
  if (store.AddRecentAdaptivePrototype(
          1, std::vector<float> {0.6f, 0.4f, 0.0f, 0.0f}, weak_metadata)) {
    return 9;
  }

  PrototypeMetadata adaptive_metadata;
  adaptive_metadata.timestamp_ms = 301U;
  adaptive_metadata.quality_score = 0.90f;
  adaptive_metadata.decision_score = 0.88f;
  adaptive_metadata.top1_top2_margin = 0.07f;
  adaptive_metadata.liveness_ok = true;
  if (!store.ShouldAcceptAdaptivePrototype(adaptive_metadata)) {
    return 10;
  }
  if (!store.AddRecentAdaptivePrototype(
          1, std::vector<float> {0.6f, 0.4f, 1.0f, 0.0f}, adaptive_metadata)) {
    return 11;
  }
  if (store.PrototypeCount(1, PrototypeZone::kRecentAdaptive) != 2U) {
    return 12;
  }

  PrototypeMetadata verified_metadata;
  verified_metadata.timestamp_ms = 302U;
  verified_metadata.quality_score = 0.88f;
  verified_metadata.decision_score = 0.93f;
  verified_metadata.top1_top2_margin = 0.12f;
  verified_metadata.liveness_ok = true;
  if (!store.ShouldAcceptVerifiedHistoryPrototype(verified_metadata)) {
    return 13;
  }
  if (!store.AddVerifiedHistoryPrototype(
          1, std::vector<float> {0.5f, 0.5f, 0.0f, 1.0f}, verified_metadata)) {
    return 14;
  }
  if (store.PrototypeCount(1, PrototypeZone::kVerifiedHistory) != 5U) {
    return 15;
  }

  const auto exported = store.ExportWeightedPrototypes();
  if (exported.size() != 8U) {
    return 16;
  }

  int baseline_count = 0;
  int verified_count = 0;
  int recent_count = 0;
  for (const auto& prototype : exported) {
    if (prototype.zone == static_cast<int>(PrototypeZone::kBaseline)) {
      baseline_count += 1;
      if (prototype.prototype_weight != config.baseline_weight) {
        return 17;
      }
    } else if (prototype.zone == static_cast<int>(PrototypeZone::kVerifiedHistory)) {
      verified_count += 1;
      if (prototype.prototype_weight != config.verified_history_weight) {
        return 18;
      }
    } else if (prototype.zone == static_cast<int>(PrototypeZone::kRecentAdaptive)) {
      recent_count += 1;
      if (prototype.prototype_weight != config.recent_adaptive_weight) {
        return 19;
      }
    }
  }

  if (baseline_count != 1 || verified_count != 5 || recent_count != 2) {
    return 20;
  }

  sentriface::search::FaceSearchV2IndexPackage index_package;
  const auto build_index_result = store.BuildSearchIndexPackage(&index_package);
  if (!build_index_result.ok) {
    return 21;
  }
  if (index_package.entries.size() != exported.size() ||
      index_package.normalized_matrix.size() !=
          exported.size() * static_cast<std::size_t>(config.embedding_dim)) {
    return 22;
  }
  if (index_package.entries[0].person_id != 1 ||
      index_package.entries[0].zone != static_cast<int>(PrototypeZone::kBaseline) ||
      index_package.entries[0].prototype_weight != config.baseline_weight) {
    return 23;
  }
  const float first_row_norm =
      index_package.normalized_matrix[0] * index_package.normalized_matrix[0] +
      index_package.normalized_matrix[1] * index_package.normalized_matrix[1] +
      index_package.normalized_matrix[2] * index_package.normalized_matrix[2] +
      index_package.normalized_matrix[3] * index_package.normalized_matrix[3];
  if (std::fabs(first_row_norm - 1.0f) > 1e-4f) {
    return 24;
  }

  const std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() / "sentriface_enrollment_store_v2_test";
  std::filesystem::create_directories(temp_dir);
  const std::filesystem::path index_path = temp_dir / "search_index.sfsi";
  const auto save_index_result = store.SaveSearchIndexPackageBinary(index_path.string());
  if (!save_index_result.ok) {
    return 25;
  }
  sentriface::search::FaceSearchV2 search(
      sentriface::search::SearchConfig {.embedding_dim = 4, .top_k = 2});
  if (!search.LoadFromIndexPackagePath(index_path.string())) {
    return 26;
  }
  const auto search_result = search.Query(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!search_result.ok || search_result.hits.empty() ||
      search_result.hits[0].person_id != 1) {
    return 27;
  }
  EnrollmentStoreV2 loaded_store(config);
  if (!loaded_store.LoadFromSearchIndexPackagePath(index_path.string())) {
    return 28;
  }
  if (loaded_store.PersonCount() != 1U ||
      loaded_store.PrototypeCount(1, PrototypeZone::kBaseline) != 1U ||
      loaded_store.PrototypeCount(1, PrototypeZone::kVerifiedHistory) != 5U ||
      loaded_store.PrototypeCount(1, PrototypeZone::kRecentAdaptive) != 2U) {
    return 29;
  }
  const auto reexported = loaded_store.ExportWeightedPrototypes();
  if (reexported.size() != exported.size()) {
    return 30;
  }
  for (std::size_t i = 0; i < reexported.size(); ++i) {
    if (reexported[i].person_id != exported[i].person_id ||
        reexported[i].prototype_id != exported[i].prototype_id ||
        reexported[i].zone != exported[i].zone ||
        reexported[i].label != exported[i].label ||
        reexported[i].prototype_weight != exported[i].prototype_weight) {
      return 31;
    }
  }
  sentriface::search::FaceSearchV2IndexPackage invalid_package = index_package;
  invalid_package.normalized_matrix.clear();
  EnrollmentStoreV2 invalid_store(config);
  if (invalid_store.LoadFromSearchIndexPackage(invalid_package)) {
    return 32;
  }
  invalid_package = index_package;
  invalid_package.entries[0].zone = 7;
  if (invalid_store.LoadFromSearchIndexPackage(invalid_package)) {
    return 33;
  }
  invalid_package = index_package;
  if (invalid_package.entries.size() > 1U) {
    invalid_package.entries[1].label = "wrong_label";
    if (invalid_store.LoadFromSearchIndexPackage(invalid_package)) {
      return 34;
    }
  }
  invalid_package = index_package;
  if (invalid_package.entries.size() > 1U) {
    invalid_package.entries[1].prototype_id = invalid_package.entries[0].prototype_id;
    invalid_package.entries[1].person_id = invalid_package.entries[0].person_id;
    if (invalid_store.LoadFromSearchIndexPackage(invalid_package)) {
      return 35;
    }
  }
  std::filesystem::remove_all(temp_dir);

  if (!store.RemovePerson(1) || store.PersonCount() != 0U) {
    return 36;
  }

  return 0;
}
