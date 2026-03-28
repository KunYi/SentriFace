#include <cmath>
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
  using sentriface::search::SearchConfig;

  EnrollmentConfigV2 enroll_config;
  enroll_config.embedding_dim = 4;
  enroll_config.max_baseline_prototypes = 1;
  enroll_config.max_verified_history_prototypes = 5;
  enroll_config.max_recent_adaptive_prototypes = 2;
  enroll_config.baseline_weight = 1.0f;
  enroll_config.verified_history_weight = 0.8f;
  enroll_config.recent_adaptive_weight = 0.6f;

  EnrollmentStoreV2 store(enroll_config);
  if (!store.UpsertPerson(1, "alice") || !store.UpsertPerson(2, "bob")) {
    return 1;
  }

  PrototypeMetadata metadata;
  metadata.liveness_ok = true;
  metadata.quality_score = 0.9f;
  metadata.decision_score = 0.85f;
  metadata.top1_top2_margin = 0.08f;

  if (!store.AddPrototype(
          1, PrototypeZone::kBaseline,
          std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f}, metadata)) {
    return 2;
  }
  if (!store.AddPrototype(
          1, PrototypeZone::kRecentAdaptive,
          std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f}, metadata)) {
    return 3;
  }
  if (!store.AddPrototype(
          2, PrototypeZone::kVerifiedHistory,
          std::vector<float> {0.9f, 0.1f, 0.0f, 0.0f}, metadata)) {
    return 4;
  }

  const auto exported = store.ExportWeightedPrototypes();
  if (exported.size() != 3U) {
    return 5;
  }

  SearchConfig search_config;
  search_config.embedding_dim = 4;
  search_config.top_k = 2;

  FaceSearchV2 search(search_config);
  for (const auto& prototype : exported) {
    if (!search.AddPrototype(prototype)) {
      return 6;
    }
  }

  const auto result = search.Query(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!result.ok) {
    return 7;
  }
  if (result.hits.size() != 2U) {
    return 8;
  }
  if (result.prototype_hits.size() != 3U) {
    return 9;
  }

  if (result.hits[0].person_id != 1) {
    return 10;
  }
  if (result.hits[0].best_zone != static_cast<int>(PrototypeZone::kBaseline)) {
    return 11;
  }
  if (result.hits[0].score != enroll_config.baseline_weight) {
    return 12;
  }

  if (result.prototype_hits[0].zone != static_cast<int>(PrototypeZone::kBaseline)) {
    return 13;
  }
  if (!AlmostEqual(result.prototype_hits[0].weighted_score, enroll_config.baseline_weight)) {
    return 14;
  }

  bool found_recent = false;
  bool found_verified = false;
  for (const auto& hit : result.prototype_hits) {
    if (hit.zone == static_cast<int>(PrototypeZone::kRecentAdaptive)) {
      found_recent = true;
      if (!AlmostEqual(hit.weighted_score, enroll_config.recent_adaptive_weight)) {
        return 15;
      }
    }
    if (hit.zone == static_cast<int>(PrototypeZone::kVerifiedHistory)) {
      found_verified = true;
      if (!(hit.weighted_score > enroll_config.recent_adaptive_weight)) {
        return 16;
      }
    }
  }

  if (!found_recent || !found_verified) {
    return 17;
  }

  if (!(result.top1_top2_margin > 0.0f)) {
    return 18;
  }

  return 0;
}
