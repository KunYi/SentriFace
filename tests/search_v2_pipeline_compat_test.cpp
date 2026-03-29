#include <cstdint>
#include <filesystem>
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
          2, PrototypeZone::kVerifiedHistory,
          std::vector<float> {0.9f, 0.1f, 0.0f, 0.0f}, metadata)) {
    return 1;
  }
  FaceSearchV2IndexPackage index_package;
  const auto build_result = store.BuildSearchIndexPackage(&index_package);
  if (!build_result.ok) {
    return 2;
  }
  FaceSearchV2 search(search_config);
  if (!search.LoadFromIndexPackage(index_package)) {
    return 3;
  }
  const std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() /
      "sentriface_search_v2_pipeline_compat_test";
  std::filesystem::create_directories(temp_dir);
  const std::filesystem::path index_path = temp_dir / "search_index.sfsi";
  if (!search.SaveIndexPackageBinary(index_path.string())) {
    return 4;
  }

  const auto search_v2_result =
      search.Query(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  if (!search_v2_result.ok || search_v2_result.hits.empty()) {
    return 5;
  }

  PipelineConfig pipeline_config;
  pipeline_config.search.embedding_dim = 4;
  pipeline_config.decision.min_consistent_hits = 2;
  pipeline_config.decision.unlock_threshold = 0.5f;
  pipeline_config.decision.avg_threshold = 0.5f;
  pipeline_config.decision.min_margin = 0.01f;

  FacePipeline pipeline(pipeline_config);
  if (!pipeline.LoadEnrollment(index_path.string())) {
    return 6;
  }
  sentriface::tracker::TrackSnapshot snapshot;
  snapshot.track_id = 10;
  snapshot.state = sentriface::tracker::TrackState::kConfirmed;
  snapshot.last_timestamp_ms = static_cast<std::uint64_t>(1000);
  pipeline.SyncTracks(std::vector<sentriface::tracker::TrackSnapshot> {snapshot});
  const auto state1 = pipeline.UpdateTrackSearch(10, search_v2_result);
  if (state1.stable_person_id != 1) {
    return 7;
  }
  if (state1.unlock_ready) {
    return 8;
  }

  const auto state2 = pipeline.UpdateTrackSearch(10, search_v2_result);
  if (state2.stable_person_id != 1) {
    return 9;
  }
  if (!state2.unlock_ready) {
    return 10;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}
