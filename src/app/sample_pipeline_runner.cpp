#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "app/face_pipeline.hpp"
#include "app/pipeline_report.hpp"
#include "camera/frame_source.hpp"
#include "detector/face_detector.hpp"
#include "enroll/enrollment_store.hpp"
#include "access/unlock_controller.hpp"
#include "tracker/tracker.hpp"

namespace {

sentriface::camera::Frame MakeFrame(std::uint64_t timestamp_ms) {
  sentriface::camera::Frame frame;
  frame.width = 640;
  frame.height = 640;
  frame.channels = 3;
  frame.pixel_format = sentriface::camera::PixelFormat::kRgb888;
  frame.timestamp_ms = timestamp_ms;
  frame.data.resize(static_cast<std::size_t>(frame.width * frame.height * frame.channels), 127U);
  return frame;
}

sentriface::tracker::Detection MakeDetection(float x, float y, std::uint64_t timestamp_ms) {
  sentriface::tracker::Detection detection;
  detection.bbox = sentriface::tracker::RectF {x, y, 96.0f, 104.0f};
  detection.score = 0.93f;
  detection.timestamp_ms = timestamp_ms;
  detection.landmarks.points = {{
      {x + 28.0f, y + 36.0f},
      {x + 68.0f, y + 35.0f},
      {x + 48.0f, y + 56.0f},
      {x + 32.0f, y + 80.0f},
      {x + 66.0f, y + 79.0f},
  }};
  return detection;
}

}  // namespace

namespace {

int ReadEnvInt(const char* name, int default_value) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return default_value;
  }
  return std::atoi(value);
}

std::string ReadEnvString(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr) {
    return {};
  }
  return std::string(value);
}

bool LoadSinglePrototypeSearchIndex(
    sentriface::app::FacePipeline& pipeline,
    const std::string& label,
    const std::vector<float>& embedding,
    int embedding_dim) {
  sentriface::search::FacePrototypeV2 prototype;
  prototype.person_id = 1;
  prototype.prototype_id = 10;
  prototype.zone = 0;
  prototype.label = label;
  prototype.embedding = embedding;
  prototype.prototype_weight = 1.0f;
  sentriface::search::FaceSearchV2IndexPackage index_package;
  const auto diagnostic = sentriface::search::BuildFaceSearchV2IndexPackage(
      std::vector<sentriface::search::FacePrototypeV2> {prototype},
      embedding_dim, &index_package);
  return diagnostic.ok && pipeline.LoadEnrollment(index_package);
}

bool LoadSearchV2Pipeline(
    sentriface::app::FacePipeline& pipeline,
    const std::string& search_index_path,
    const std::string& baseline_package_path,
    int baseline_person_id,
    const std::string& label,
    const std::vector<float>& embedding,
    int embedding_dim) {
  if (!search_index_path.empty()) {
    return pipeline.LoadEnrollment(search_index_path);
  }
  if (!baseline_package_path.empty()) {
    return pipeline.LoadEnrollmentBaselinePackage(
        baseline_package_path, baseline_person_id);
  }
  return LoadSinglePrototypeSearchIndex(
      pipeline, label, embedding, embedding_dim);
}

}  // namespace

int main() {
  using sentriface::app::FacePipeline;
  using sentriface::app::PipelineConfig;
  using sentriface::tracker::TrackSnapshot;
  using sentriface::tracker::TrackState;
  using sentriface::camera::Frame;
  using sentriface::camera::SequenceFrameSource;
  using sentriface::detector::DetectionBatch;
  using sentriface::detector::DetectionConfig;
  using sentriface::detector::SequenceFaceDetector;
  using sentriface::enroll::EnrollmentConfig;
  using sentriface::enroll::EnrollmentStore;
  using sentriface::access::LoadUnlockControllerConfigFromEnv;
  using sentriface::access::UnlockController;
  using sentriface::access::UnlockControllerConfig;
  using sentriface::access::UnlockBackend;
  using sentriface::access::UnlockEvent;
  using sentriface::tracker::Tracker;
  using sentriface::tracker::TrackerConfig;

  std::vector<Frame> frames;
  frames.push_back(MakeFrame(static_cast<std::uint64_t>(1000)));
  frames.push_back(MakeFrame(static_cast<std::uint64_t>(1033)));
  frames.push_back(MakeFrame(static_cast<std::uint64_t>(1066)));

  SequenceFrameSource source(std::move(frames), "sample_sequence");
  if (!source.Open()) {
    std::cerr << "Failed to open sample frame source\n";
    return 1;
  }

  const bool use_search_v2 = ReadEnvInt("SENTRIFACE_PIPELINE_USE_V2", 0) != 0;
  const std::string search_index_path =
      ReadEnvString("SENTRIFACE_SEARCH_INDEX_PATH");
  const std::string baseline_package_path =
      ReadEnvString("SENTRIFACE_BASELINE_PACKAGE_PATH");
  const int baseline_person_id =
      ReadEnvInt("SENTRIFACE_BASELINE_PERSON_ID", 1);

  EnrollmentConfig enroll_config;
  enroll_config.embedding_dim = 4;
  EnrollmentStore store(enroll_config);

  DetectionConfig detector_config;
  detector_config.input_width = 640;
  detector_config.input_height = 640;
  detector_config.input_channels = 3;
  detector_config.score_threshold = 0.60f;

  std::vector<DetectionBatch> batches;
  batches.push_back(DetectionBatch {
      static_cast<std::uint64_t>(1000),
      {MakeDetection(100.0f, 120.0f, static_cast<std::uint64_t>(1000))},
  });
  batches.push_back(DetectionBatch {
      static_cast<std::uint64_t>(1033),
      {MakeDetection(104.0f, 121.0f, static_cast<std::uint64_t>(1033))},
  });
  batches.push_back(DetectionBatch {
      static_cast<std::uint64_t>(1066),
      {MakeDetection(108.0f, 122.0f, static_cast<std::uint64_t>(1066))},
  });

  SequenceFaceDetector detector(std::move(batches), detector_config, "sample_detector");

  TrackerConfig tracker_config;
  tracker_config.min_confirm_hits = 2;
  tracker_config.max_miss = 2;
  tracker_config.max_tracks = 3;
  tracker_config.embed_det_score_thresh = 0.80f;
  tracker_config.embed_min_face_width = 90.0f;
  tracker_config.embed_min_face_height = 90.0f;
  tracker_config.embed_min_face_quality = 0.70f;
  tracker_config.embed_interval_ms = 500;
  Tracker tracker(tracker_config);

  PipelineConfig pipeline_config;
  pipeline_config.search.embedding_dim = 4;
  pipeline_config.search.top_k = 2;
  pipeline_config.decision.min_consistent_hits = 2;
  pipeline_config.decision.unlock_threshold = 0.78f;
  pipeline_config.decision.avg_threshold = 0.72f;
  pipeline_config.decision.min_margin = 0.03f;

  FacePipeline pipeline(pipeline_config);
  if (use_search_v2) {
    if (!LoadSearchV2Pipeline(
            pipeline, search_index_path, baseline_package_path,
            baseline_person_id, "alice",
            std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f},
            pipeline_config.search.embedding_dim)) {
      std::cerr << "Failed to load search index package into pipeline\n";
      return 2;
    }
  } else {
    if (!store.UpsertPerson(1, "alice")) {
      std::cerr << "Failed to create sample person\n";
      return 2;
    }
    if (!store.AddPrototype(1, std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f})) {
      std::cerr << "Failed to add sample prototype\n";
      return 3;
    }
    if (!pipeline.LoadEnrollment(store)) {
      std::cerr << "Failed to load enrollment into pipeline\n";
      return 4;
    }
  }

  UnlockControllerConfig unlock_config;
  unlock_config.backend = UnlockBackend::kDummy;
  UnlockController unlock_controller(LoadUnlockControllerConfigFromEnv(unlock_config));

  sentriface::app::PipelineRunStats stats;
  Frame frame;
  int frame_count = 0;
  while (source.Read(frame)) {
    const auto detections = detector.Detect(frame);
    stats.frames_processed += 1;
    stats.detections_processed += static_cast<int>(detections.detections.size());
    tracker.Step(detections.detections, frame.timestamp_ms);
    pipeline.SyncTracks(tracker.GetTrackSnapshots());
    stats.tracks_seen = static_cast<int>(pipeline.GetTrackStates().size());

    auto trigger_candidates = tracker.GetEmbeddingTriggerCandidates(frame.timestamp_ms);
    for (const auto& candidate : trigger_candidates) {
      const std::vector<float> embedding {1.0f, 0.0f, 0.0f, 0.0f};
      const auto decision_state = use_search_v2
          ? pipeline.UpdateTrackSearch(
                candidate.track_id,
                pipeline.SearchEmbeddingV2(embedding))
          : pipeline.UpdateTrackSearch(
                candidate.track_id,
                pipeline.SearchEmbedding(embedding));
      tracker.MarkEmbeddingTriggered(candidate.track_id, frame.timestamp_ms);
      stats.embedding_triggers += 1;

      std::cout << "frame=" << frame_count
                << " ts=" << frame.timestamp_ms
                << " track=" << candidate.track_id
                << " hits=" << decision_state.consistent_hits
                << " avg_score=" << decision_state.average_score
                << " unlock_ready=" << (decision_state.unlock_ready ? "yes" : "no")
                << '\n';
    }

    ++frame_count;
  }

  const auto unlock_ready = pipeline.GetUnlockReadyTracks();
  stats.unlock_ready_tracks = static_cast<int>(unlock_ready.size());
  for (const auto& ready : unlock_ready) {
    UnlockEvent event;
    event.track_id = ready.snapshot.track_id;
    event.person_id = ready.decision.stable_person_id;
    event.timestamp_ms = static_cast<std::uint32_t>(ready.snapshot.last_timestamp_ms);
    event.label = "unlock_ready";
    const auto unlock_result = unlock_controller.TriggerUnlock(event);
    if (unlock_result.ok && unlock_result.fired) {
      stats.unlock_actions += 1;
    }
  }
  stats.short_gap_merges = pipeline.GetShortGapMergeCount();
  std::cout << "unlock_ready_tracks=" << unlock_ready.size() << '\n';
  std::cout << "unlock_actions=" << stats.unlock_actions << '\n';

  PipelineConfig merge_config = pipeline_config;
  merge_config.short_gap_merge_window_ms = 1500;
  FacePipeline merge_pipeline(merge_config);
  if (use_search_v2) {
    if (!LoadSearchV2Pipeline(
            merge_pipeline, search_index_path, baseline_package_path,
            baseline_person_id, "alice",
            std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f},
            merge_config.search.embedding_dim)) {
      std::cerr << "Failed to load search index package into merge pipeline\n";
      return 5;
    }
  } else {
    if (!merge_pipeline.LoadEnrollment(store)) {
      std::cerr << "Failed to load enrollment into merge pipeline\n";
      return 5;
    }
  }

  TrackSnapshot old_snapshot;
  old_snapshot.track_id = 100;
  old_snapshot.state = TrackState::kConfirmed;
  old_snapshot.last_timestamp_ms = static_cast<std::uint64_t>(1000);
  merge_pipeline.SyncTracks(std::vector<TrackSnapshot> {old_snapshot});

  const std::vector<float> merge_embedding {1.0f, 0.0f, 0.0f, 0.0f};
  auto merge_state = use_search_v2
      ? merge_pipeline.UpdateTrackSearch(
            100, merge_pipeline.SearchEmbeddingV2(merge_embedding))
      : merge_pipeline.UpdateTrackSearch(
            100, merge_pipeline.SearchEmbedding(merge_embedding));
  merge_state = use_search_v2
      ? merge_pipeline.UpdateTrackSearch(
            100, merge_pipeline.SearchEmbeddingV2(merge_embedding))
      : merge_pipeline.UpdateTrackSearch(
            100, merge_pipeline.SearchEmbedding(merge_embedding));

  merge_pipeline.SyncTracks({});

  TrackSnapshot new_snapshot;
  new_snapshot.track_id = 101;
  new_snapshot.state = TrackState::kConfirmed;
  new_snapshot.last_timestamp_ms = static_cast<std::uint64_t>(1800);
  merge_pipeline.SyncTracks(std::vector<TrackSnapshot> {new_snapshot});
  merge_state = use_search_v2
      ? merge_pipeline.UpdateTrackSearch(
            101, merge_pipeline.SearchEmbeddingV2(merge_embedding))
      : merge_pipeline.UpdateTrackSearch(
            101, merge_pipeline.SearchEmbedding(merge_embedding));

  std::cout << "short_gap_merge_count=" << merge_pipeline.GetShortGapMergeCount() << '\n';
  std::cout << "merged_track=" << merge_state.track_id
            << " merged_hits=" << merge_state.consistent_hits
            << " merged_unlock_ready=" << (merge_state.unlock_ready ? "yes" : "no")
            << '\n';
  std::cout << "pipeline_mode=" << (use_search_v2 ? "v2" : "v1") << '\n';

  stats.short_gap_merges += merge_pipeline.GetShortGapMergeCount();
  if (!sentriface::app::WritePipelineReport("sample_pipeline_report.txt", stats)) {
    std::cerr << "Failed to write sample pipeline report\n";
    return 6;
  }
  return 0;
}
