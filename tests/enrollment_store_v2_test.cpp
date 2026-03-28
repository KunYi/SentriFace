#include <vector>

#include "enroll/enrollment_store.hpp"

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

  if (!store.RemovePerson(1) || store.PersonCount() != 0U) {
    return 21;
  }

  return 0;
}
