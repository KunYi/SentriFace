#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>

#include "app/face_pipeline.hpp"
#include "app/pipeline_report.hpp"
#include "logging/system_logger.hpp"
#include "camera/offline_sequence_source.hpp"
#include "detector/face_detector.hpp"
#include "enroll/enrollment_store.hpp"
#include "access/unlock_controller.hpp"
#include "tracker/tracker.hpp"

#undef SENTRIFACE_LOG_TAG
#define SENTRIFACE_LOG_TAG "offline_runner"

namespace {

int ReadEnvInt(const char* name, int default_value) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return default_value;
  }
  return std::atoi(value);
}

bool ReadEnvBool(const char* name, bool default_value) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return default_value;
  }
  return std::atoi(value) != 0;
}

float ReadEnvFloat(const char* name, float default_value) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return default_value;
  }
  return std::strtof(value, nullptr);
}

}  // namespace

namespace {

using EmbeddingEventMap = std::unordered_map<std::string, std::vector<float>>;

std::string MakeEmbeddingEventKey(std::uint64_t timestamp_ms, int track_id) {
  return std::to_string(timestamp_ms) + ":" + std::to_string(track_id);
}

bool LoadEmbeddingEventManifest(const std::string& path,
                                int& embedding_dim,
                                EmbeddingEventMap& events) {
  if (path.empty()) {
    return true;
  }

  std::ifstream in(path);
  if (!in.is_open()) {
    return false;
  }

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::istringstream iss(line);
    std::uint64_t timestamp_ms = 0;
    int track_id = -1;
    if (!(iss >> timestamp_ms >> track_id)) {
      return false;
    }

    std::vector<float> embedding;
    float value = 0.0f;
    while (iss >> value) {
      embedding.push_back(value);
    }
    if (embedding.empty()) {
      return false;
    }
    if (embedding_dim == 0) {
      embedding_dim = static_cast<int>(embedding.size());
    }
    if (static_cast<int>(embedding.size()) != embedding_dim) {
      return false;
    }

    events[MakeEmbeddingEventKey(timestamp_ms, track_id)] = std::move(embedding);
  }

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 4) {
    std::cerr << "Usage: offline_sequence_runner <frame_manifest.txt> [detection_manifest.txt] [embedding_manifest.txt]\n";
    return 1;
  }

  const std::string frame_manifest_path = argv[1];
  const std::string detection_manifest_path = argc >= 3 ? argv[2] : std::string {};
  const std::string embedding_manifest_path = argc >= 4 ? argv[3] : std::string {};
  const bool use_search_v2 = ReadEnvInt("SENTRIFACE_PIPELINE_USE_V2", 0) != 0;
  const bool apply_adaptive_updates =
      ReadEnvInt("SENTRIFACE_PIPELINE_APPLY_ADAPTIVE_UPDATES", 0) != 0;
  const bool promote_verified_updates =
      ReadEnvInt("SENTRIFACE_PIPELINE_PROMOTE_VERIFIED_UPDATES", 0) != 0;

  sentriface::logging::LoggerConfig logger_defaults;
  logger_defaults.enabled = false;
  logger_defaults.level = sentriface::logging::LogLevel::kInfo;
  logger_defaults.backend = sentriface::logging::LogBackend::kDummy;
  logger_defaults.module_filter = "offline_runner,pipeline,access";
  sentriface::logging::SystemLogger logger(
      sentriface::logging::LoadLoggerConfigFromEnv(logger_defaults));

  sentriface::camera::OfflineSequenceFrameSource source(frame_manifest_path);
  if (!source.Open()) {
    std::cerr << "Failed to open offline sequence manifest: " << frame_manifest_path << '\n';
    return 2;
  }
  SENTRIFACE_LOGI(logger, "offline_sequence_runner_started");

  sentriface::detector::DetectionConfig detector_config;
  detector_config.input_width = 640;
  detector_config.input_height = 640;
  detector_config.input_channels = 3;
  detector_config.score_threshold = 0.60f;
  detector_config.min_face_width =
      ReadEnvFloat("SENTRIFACE_DETECTOR_MIN_FACE_WIDTH", 120.0f);
  detector_config.min_face_height =
      ReadEnvFloat("SENTRIFACE_DETECTOR_MIN_FACE_HEIGHT", 120.0f);
  detector_config.prefer_larger_faces =
      ReadEnvBool("SENTRIFACE_DETECTOR_PREFER_LARGER_FACES", true);
  detector_config.max_output_detections =
      ReadEnvInt("SENTRIFACE_DETECTOR_MAX_OUTPUT_DETECTIONS", 0);
  detector_config.enable_roi =
      ReadEnvBool("SENTRIFACE_DETECTOR_ENABLE_ROI", false);
  detector_config.roi.x =
      ReadEnvFloat("SENTRIFACE_DETECTOR_ROI_X", 0.0f);
  detector_config.roi.y =
      ReadEnvFloat("SENTRIFACE_DETECTOR_ROI_Y", 0.0f);
  detector_config.roi.w =
      ReadEnvFloat("SENTRIFACE_DETECTOR_ROI_W", 0.0f);
  detector_config.roi.h =
      ReadEnvFloat("SENTRIFACE_DETECTOR_ROI_H", 0.0f);

  sentriface::detector::ManifestFaceDetector detector(
      detection_manifest_path, detector_config, "offline_manifest_detector");
  const bool use_detection_replay = !detection_manifest_path.empty() && detector.IsLoaded();

  sentriface::tracker::TrackerConfig tracker_config;
  tracker_config.min_confirm_hits =
      ReadEnvInt("SENTRIFACE_TRACKER_MIN_CONFIRM_HITS", 2);
  tracker_config.max_miss = ReadEnvInt("SENTRIFACE_TRACKER_MAX_MISS", 2);
  tracker_config.max_tracks = ReadEnvInt("SENTRIFACE_TRACKER_MAX_TRACKS", 5);
  tracker_config.iou_min = ReadEnvFloat("SENTRIFACE_TRACKER_IOU_MIN", 0.20f);
  tracker_config.center_gate_ratio =
      ReadEnvFloat("SENTRIFACE_TRACKER_CENTER_GATE_RATIO", 0.60f);
  tracker_config.area_ratio_min =
      ReadEnvFloat("SENTRIFACE_TRACKER_AREA_RATIO_MIN", 0.50f);
  tracker_config.area_ratio_max =
      ReadEnvFloat("SENTRIFACE_TRACKER_AREA_RATIO_MAX", 2.00f);
  sentriface::tracker::Tracker tracker(tracker_config);

  EmbeddingEventMap embedding_events;
  int embedding_dim = 0;
  if (!LoadEmbeddingEventManifest(
          embedding_manifest_path,
          embedding_dim,
          embedding_events)) {
    std::cerr << "Failed to load embedding event manifest\n";
    return 3;
  }
  if (embedding_dim == 0) {
    embedding_dim = ReadEnvInt("SENTRIFACE_SEARCH_EMBEDDING_DIM", 4);
  }

  sentriface::app::PipelineConfig pipeline_config;
  pipeline_config.search.embedding_dim = embedding_dim;
  pipeline_config.search.top_k = 2;
  pipeline_config.decision.min_consistent_hits =
      ReadEnvInt("SENTRIFACE_DECISION_MIN_CONSISTENT_HITS", 2);
  pipeline_config.decision.unlock_threshold =
      ReadEnvFloat("SENTRIFACE_DECISION_UNLOCK_THRESHOLD", 0.78f);
  pipeline_config.decision.avg_threshold =
      ReadEnvFloat("SENTRIFACE_DECISION_AVG_THRESHOLD", 0.72f);
  pipeline_config.decision.min_margin =
      ReadEnvFloat("SENTRIFACE_DECISION_MIN_MARGIN", 0.03f);
  pipeline_config.short_gap_merge_window_ms = static_cast<std::uint32_t>(
      ReadEnvInt("SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS", 1500));
  pipeline_config.short_gap_merge_min_previous_hits =
      ReadEnvInt("SENTRIFACE_SHORT_GAP_MERGE_MIN_PREVIOUS_HITS", 2);
  pipeline_config.short_gap_merge_center_gate_ratio =
      ReadEnvFloat("SENTRIFACE_SHORT_GAP_MERGE_CENTER_GATE_RATIO", 1.25f);
  pipeline_config.short_gap_merge_area_ratio_min =
      ReadEnvFloat("SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MIN", 0.40f);
  pipeline_config.short_gap_merge_area_ratio_max =
      ReadEnvFloat("SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MAX", 2.50f);
  pipeline_config.enable_embedding_assisted_relink =
      ReadEnvBool("SENTRIFACE_ENABLE_EMBEDDING_ASSISTED_RELINK", true);
  pipeline_config.embedding_relink_similarity_min =
      ReadEnvFloat("SENTRIFACE_EMBEDDING_RELINK_SIMILARITY_MIN", 0.45f);
  pipeline_config.logger_config = logger.config();

  sentriface::app::FacePipeline pipeline(pipeline_config);
  sentriface::enroll::EnrollmentConfig enroll_config;
  enroll_config.embedding_dim = embedding_dim;
  sentriface::enroll::EnrollmentStore store(enroll_config);
  sentriface::enroll::EnrollmentConfigV2 enroll_config_v2;
  enroll_config_v2.embedding_dim = embedding_dim;
  sentriface::enroll::EnrollmentStoreV2 store_v2(enroll_config_v2);
  std::vector<float> prototype(static_cast<std::size_t>(embedding_dim), 0.0f);
  if (!embedding_events.empty()) {
    prototype = embedding_events.begin()->second;
  } else if (embedding_dim > 0) {
    prototype[0] = 1.0f;
  }
  if (use_search_v2) {
    sentriface::enroll::PrototypeMetadata metadata;
    metadata.timestamp_ms = 1000U;
    metadata.quality_score = 0.95f;
    metadata.decision_score = 0.90f;
    metadata.top1_top2_margin = 0.20f;
    metadata.liveness_ok = true;
    metadata.manually_enrolled = true;
    if (!store_v2.UpsertPerson(1, "mock_person") ||
        !store_v2.AddPrototype(
            1,
            sentriface::enroll::PrototypeZone::kBaseline,
            prototype,
            metadata) ||
        !pipeline.LoadEnrollment(store_v2)) {
      std::cerr << "Failed to initialize offline mock decision pipeline v2\n";
      return 4;
    }
  } else {
    if (!store.UpsertPerson(1, "mock_person") ||
        !store.AddPrototype(1, prototype) ||
        !pipeline.LoadEnrollment(store)) {
      std::cerr << "Failed to initialize offline mock decision pipeline\n";
      return 4;
    }
  }

  sentriface::access::UnlockControllerConfig unlock_config;
  unlock_config.backend = sentriface::access::UnlockBackend::kDummy;
  sentriface::access::UnlockController unlock_controller(
      sentriface::access::LoadUnlockControllerConfigFromEnv(unlock_config));

  std::ofstream timeline;
  std::ofstream decision_timeline;
  std::ofstream prototype_update_timeline;
  if (use_detection_replay) {
    timeline.open("offline_tracker_timeline.csv");
    if (!timeline.is_open()) {
      std::cerr << "Failed to open offline_tracker_timeline.csv\n";
      return 5;
    }
    timeline << "timestamp_ms,track_id,state,bbox_x,bbox_y,bbox_w,bbox_h,hits,miss,score,face_quality\n";

    decision_timeline.open("offline_decision_timeline.csv");
    if (!decision_timeline.is_open()) {
      std::cerr << "Failed to open offline_decision_timeline.csv\n";
      return 6;
    }
    decision_timeline << "timestamp_ms,track_id,stable_person_id,consistent_hits,average_score,average_margin,unlock_ready,short_gap_merges\n";

    if (use_search_v2 && apply_adaptive_updates) {
      prototype_update_timeline.open("offline_prototype_updates.csv");
      if (!prototype_update_timeline.is_open()) {
        std::cerr << "Failed to open offline_prototype_updates.csv\n";
        return 7;
      }
      prototype_update_timeline
          << "timestamp_ms,track_id,person_id,target_zone,accepted,cooldown_blocked,"
             "quality_score,decision_score,margin,"
             "quality_ok,decision_score_ok,margin_ok,liveness_ok,rejection_reason\n";
    }
  }

  sentriface::app::PipelineRunStats stats;
  sentriface::camera::Frame frame;
  while (source.Read(frame)) {
    stats.frames_processed += 1;
    if (use_detection_replay) {
      const auto batch = detector.Detect(frame);
      stats.detections_processed += static_cast<int>(batch.detections.size());
      tracker.Step(batch.detections, frame.timestamp_ms);
      const auto snapshots = tracker.GetTrackSnapshots();
      pipeline.SyncTracks(snapshots);
      if (static_cast<int>(snapshots.size()) > stats.tracks_seen) {
        stats.tracks_seen = static_cast<int>(snapshots.size());
      }
      for (const auto& snapshot : snapshots) {
        timeline << frame.timestamp_ms << ','
                 << snapshot.track_id << ','
                 << static_cast<int>(snapshot.state) << ','
                 << snapshot.bbox.x << ','
                 << snapshot.bbox.y << ','
                 << snapshot.bbox.w << ','
                 << snapshot.bbox.h << ','
                 << snapshot.hits << ','
                 << snapshot.miss << ','
                 << snapshot.last_detection_score << ','
                 << snapshot.face_quality << '\n';

        if (snapshot.state != sentriface::tracker::TrackState::kConfirmed) {
          continue;
        }

        const auto embedding_it = embedding_events.find(
            MakeEmbeddingEventKey(frame.timestamp_ms, snapshot.track_id));

        const std::vector<float> default_embedding {1.0f, 0.0f, 0.0f, 0.0f};
        const std::vector<float>& embedding =
            embedding_it != embedding_events.end() ? embedding_it->second : default_embedding;
        const auto decision_state =
            embedding_it != embedding_events.end()
                ? (use_search_v2
                      ? pipeline.UpdateTrackSearch(
                            snapshot.track_id,
                            embedding,
                            pipeline.SearchEmbeddingV2(embedding))
                      : pipeline.UpdateTrackSearch(
                            snapshot.track_id,
                            embedding,
                            pipeline.SearchEmbedding(embedding)))
                : (use_search_v2
                      ? pipeline.UpdateTrackSearch(
                            snapshot.track_id,
                            pipeline.SearchEmbeddingV2(embedding))
                      : pipeline.UpdateTrackSearch(
                            snapshot.track_id,
                            pipeline.SearchEmbedding(embedding)));
        decision_timeline << frame.timestamp_ms << ','
                          << snapshot.track_id << ','
                          << decision_state.stable_person_id << ','
                          << decision_state.consistent_hits << ','
                          << decision_state.average_score << ','
                          << decision_state.average_margin << ','
                          << (decision_state.unlock_ready ? 1 : 0) << ','
                          << pipeline.GetShortGapMergeCount() << '\n';
      }

      const auto unlock_ready = pipeline.GetUnlockReadyTracks();
      for (const auto& ready : unlock_ready) {
        sentriface::access::UnlockEvent event;
        event.track_id = ready.snapshot.track_id;
        event.person_id = ready.decision.stable_person_id;
        event.timestamp_ms =
            static_cast<std::uint32_t>(ready.snapshot.last_timestamp_ms);
        event.label = "offline_unlock_ready";
        const auto unlock_result = unlock_controller.TriggerUnlock(event);
        if (unlock_result.ok && unlock_result.fired) {
          stats.unlock_actions += 1;
        }
        std::ostringstream unlock_log;
        unlock_log << "unlock_event"
                   << " track_id=" << ready.snapshot.track_id
                   << " person_id=" << ready.decision.stable_person_id
                   << " fired=" << (unlock_result.fired ? 1 : 0)
                   << " detail=" << unlock_result.message;
        SENTRIFACE_LOGI_TAG(logger, "access", unlock_log.str());
      }

      if (use_search_v2 && apply_adaptive_updates) {
        const auto candidates = pipeline.GetPrototypeUpdateCandidates(store_v2, true);
        for (const auto& candidate : candidates) {
          const auto& diagnostic = promote_verified_updates
              ? candidate.verified_history_diagnostic
              : candidate.recent_adaptive_diagnostic;
          const bool accepted = promote_verified_updates
              ? candidate.eligible_verified_history
              : candidate.eligible_recent_adaptive;
          prototype_update_timeline
              << candidate.metadata.timestamp_ms << ','
              << candidate.track_id << ','
              << candidate.person_id << ','
              << (promote_verified_updates ? "verified_history" : "recent_adaptive")
              << ','
              << (accepted ? 1 : 0)
              << ','
              << (candidate.cooldown_blocked ? 1 : 0)
              << ','
              << candidate.metadata.quality_score << ','
              << candidate.metadata.decision_score << ','
              << candidate.metadata.top1_top2_margin << ','
              << (diagnostic.quality_ok ? 1 : 0) << ','
              << (diagnostic.decision_score_ok ? 1 : 0) << ','
              << (diagnostic.margin_ok ? 1 : 0) << ','
              << (diagnostic.liveness_ok ? 1 : 0) << ','
              << (candidate.cooldown_blocked ? "cooldown_active"
                                             : diagnostic.rejection_reason)
              << '\n';
          std::ostringstream update_log;
          update_log << "prototype_update_candidate"
                     << " track_id=" << candidate.track_id
                     << " person_id=" << candidate.person_id
                     << " zone="
                     << (promote_verified_updates ? "verified_history"
                                                  : "recent_adaptive")
                     << " accepted=" << (accepted ? 1 : 0)
                     << " cooldown_blocked=" << (candidate.cooldown_blocked ? 1 : 0)
                     << " quality=" << candidate.metadata.quality_score
                     << " decision_score=" << candidate.metadata.decision_score
                     << " margin=" << candidate.metadata.top1_top2_margin
                     << " reason="
                     << (candidate.cooldown_blocked ? "cooldown_active"
                                                   : diagnostic.rejection_reason);
          if (accepted) {
            SENTRIFACE_LOGI_TAG(logger, "pipeline", update_log.str());
          } else {
            SENTRIFACE_LOGD_TAG(logger, "pipeline", update_log.str());
          }
        }
        stats.adaptive_updates_applied +=
            pipeline.ApplyAdaptivePrototypeUpdates(
                store_v2, promote_verified_updates, true);
      }
    }
  }

  stats.unlock_ready_tracks =
      static_cast<int>(pipeline.GetUnlockReadyTracks().size());
  stats.short_gap_merges = pipeline.GetShortGapMergeCount();

  if (!sentriface::app::WritePipelineReport("offline_sequence_report.txt", stats)) {
    std::cerr << "Failed to write offline sequence report\n";
    return 8;
  }

  std::cout << "offline_frames_processed=" << stats.frames_processed << '\n';
  if (use_detection_replay) {
    std::cout << "offline_detections_processed=" << stats.detections_processed << '\n';
    std::cout << "offline_peak_tracks_seen=" << stats.tracks_seen << '\n';
    std::cout << "offline_tracker_timeline=offline_tracker_timeline.csv\n";
    std::cout << "offline_decision_timeline=offline_decision_timeline.csv\n";
    std::cout << "offline_short_gap_merges=" << stats.short_gap_merges << '\n';
    if (use_search_v2 && apply_adaptive_updates) {
      std::cout << "offline_prototype_updates=offline_prototype_updates.csv\n";
      std::cout << "offline_adaptive_updates_applied="
                << stats.adaptive_updates_applied << '\n';
    }
  }
  std::ostringstream summary_log;
  summary_log << "offline_sequence_runner_finished"
              << " frames=" << stats.frames_processed
              << " detections=" << stats.detections_processed
              << " tracks=" << stats.tracks_seen
              << " unlock_actions=" << stats.unlock_actions
              << " short_gap_merges=" << stats.short_gap_merges
              << " adaptive_updates=" << stats.adaptive_updates_applied;
  SENTRIFACE_LOGI(logger, summary_log.str());
  std::cout << "pipeline_mode=" << (use_search_v2 ? "v2" : "v1") << '\n';
  return 0;
}
