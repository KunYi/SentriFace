#include <vector>

#include "app/face_pipeline.hpp"
#include "enroll/enrollment_store.hpp"
#include "search/face_search.hpp"

int main() {
  using sentriface::app::FacePipeline;
  using sentriface::app::PipelineConfig;
  using sentriface::enroll::EnrollmentConfigV2;
  using sentriface::enroll::EnrollmentStoreV2;
  using sentriface::enroll::PrototypeMetadata;
  using sentriface::enroll::PrototypeZone;
  using sentriface::search::FaceSearchV2;
  using sentriface::search::SearchConfig;

  EnrollmentConfigV2 enroll_config;
  enroll_config.embedding_dim = 4;
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
          std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f}, metadata) ||
      !store.AddPrototype(
          2, PrototypeZone::kVerifiedHistory,
          std::vector<float> {0.9f, 0.1f, 0.0f, 0.0f}, metadata)) {
    return 2;
  }

  SearchConfig search_config;
  search_config.embedding_dim = 4;
  search_config.top_k = 2;
  FaceSearchV2 search(search_config);
  for (const auto& prototype : store.ExportWeightedPrototypes()) {
    if (!search.AddPrototype(prototype)) {
      return 3;
    }
  }

  const auto search_v2_result = search.Query(
      std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!search_v2_result.ok || search_v2_result.hits.empty()) {
    return 4;
  }

  PipelineConfig pipeline_config;
  pipeline_config.search.embedding_dim = 4;
  pipeline_config.decision.min_consistent_hits = 2;
  pipeline_config.decision.unlock_threshold = 0.5f;
  pipeline_config.decision.avg_threshold = 0.5f;
  pipeline_config.decision.min_margin = 0.01f;

  FacePipeline pipeline(pipeline_config);
  const auto state1 = pipeline.UpdateTrackSearch(10, search_v2_result);
  if (state1.stable_person_id != 1) {
    return 5;
  }
  if (state1.unlock_ready) {
    return 6;
  }

  const auto state2 = pipeline.UpdateTrackSearch(10, search_v2_result);
  if (state2.stable_person_id != 1) {
    return 7;
  }
  if (!state2.unlock_ready) {
    return 8;
  }

  return 0;
}
