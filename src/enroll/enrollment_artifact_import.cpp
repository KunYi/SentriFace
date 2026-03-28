#include "enroll/enrollment_artifact_import.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sstream>

namespace sentriface::enroll {

namespace {

std::string Trim(const std::string& value) {
  std::size_t start = 0;
  while (start < value.size() &&
         (value[start] == ' ' || value[start] == '\t' ||
          value[start] == '\r' || value[start] == '\n')) {
    start += 1U;
  }
  std::size_t end = value.size();
  while (end > start &&
         (value[end - 1] == ' ' || value[end - 1] == '\t' ||
          value[end - 1] == '\r' || value[end - 1] == '\n')) {
    end -= 1U;
  }
  return value.substr(start, end - start);
}

bool ParseKeyValue(const std::string& line,
                   std::string* key,
                   std::string* value) {
  const std::size_t pos = line.find('=');
  if (pos == std::string::npos) {
    return false;
  }
  *key = Trim(line.substr(0, pos));
  *value = Trim(line.substr(pos + 1));
  return !key->empty();
}

bool ParseInt(const std::string& text, int* out_value) {
  try {
    *out_value = std::stoi(text);
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseUInt32(const std::string& text, std::uint32_t* out_value) {
  try {
    const unsigned long value = std::stoul(text);
    *out_value = static_cast<std::uint32_t>(value);
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseFloat(const std::string& text, float* out_value) {
  try {
    *out_value = std::stof(text);
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseBool01(const std::string& text, bool* out_value) {
  if (text == "1") {
    *out_value = true;
    return true;
  }
  if (text == "0") {
    *out_value = false;
    return true;
  }
  return false;
}

std::string ResolveArtifactPath(const std::filesystem::path& base_dir,
                                const std::string& relative_path) {
  if (relative_path.empty() || relative_path == "none") {
    return std::string();
  }
  return (base_dir / relative_path).string();
}

float ComputeEnrollmentSampleWeight(const EnrollmentArtifactSampleRecord& sample) {
  float slot_bias = 1.0f;
  if (sample.slot == "frontal") {
    slot_bias = 1.12f;
  } else if (sample.slot == "left_quarter" || sample.slot == "right_quarter") {
    slot_bias = 1.04f;
  }

  const float pose_penalty =
      std::abs(sample.pitch_deg) * 0.01f + std::abs(sample.roll_deg) * 0.01f;
  const float size_bonus = sample.bbox_h * 0.0008f;
  float weight = slot_bias + sample.quality_score * 0.55f + size_bonus - pose_penalty;
  if (weight < 0.5f) {
    weight = 0.5f;
  }
  if (weight > 2.0f) {
    weight = 2.0f;
  }
  return weight;
}

}  // namespace

EnrollmentImportDiagnostic LoadEnrollmentArtifactPackage(
    const std::string& summary_path,
    EnrollmentArtifactPackage* out_package) {
  EnrollmentImportDiagnostic diagnostic;
  if (out_package == nullptr) {
    diagnostic.error_message = "null_output_package";
    return diagnostic;
  }

  std::ifstream input(summary_path);
  if (!input.good()) {
    diagnostic.error_message = "failed_to_open_summary";
    return diagnostic;
  }

  EnrollmentArtifactPackage package;
  package.summary_path = summary_path;
  package.artifact_dir = std::filesystem::path(summary_path).parent_path().string();
  const std::filesystem::path artifact_dir(package.artifact_dir);

  EnrollmentArtifactSampleRecord current_sample;
  bool in_sample = false;

  std::string line;
  while (std::getline(input, line)) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) {
      continue;
    }
    if (trimmed == "[sample]") {
      if (in_sample) {
        package.samples.push_back(current_sample);
      }
      current_sample = EnrollmentArtifactSampleRecord {};
      in_sample = true;
      continue;
    }

    std::string key;
    std::string value;
    if (!ParseKeyValue(trimmed, &key, &value)) {
      continue;
    }

    if (!in_sample) {
      if (key == "user_id") {
        package.user_id = value;
      } else if (key == "user_name") {
        package.user_name = value;
      } else if (key == "source") {
        package.source = value;
      } else if (key == "observation") {
        package.observation = value;
      } else if (key == "capture_plan") {
        package.capture_plan = value;
      }
      continue;
    }

    if (key == "index") {
      ParseInt(value, &current_sample.index);
    } else if (key == "slot") {
      current_sample.slot = value;
    } else if (key == "elapsed_ms") {
      ParseUInt32(value, &current_sample.elapsed_ms);
    } else if (key == "candidate_count") {
      continue;
    } else if (key == "has_face") {
      ParseBool01(value, &current_sample.has_face);
    } else if (key == "bbox_x") {
      ParseFloat(value, &current_sample.bbox_x);
    } else if (key == "bbox_y") {
      ParseFloat(value, &current_sample.bbox_y);
    } else if (key == "bbox_w") {
      ParseFloat(value, &current_sample.bbox_w);
    } else if (key == "bbox_h") {
      ParseFloat(value, &current_sample.bbox_h);
    } else if (key == "yaw_deg") {
      ParseFloat(value, &current_sample.yaw_deg);
    } else if (key == "pitch_deg") {
      ParseFloat(value, &current_sample.pitch_deg);
    } else if (key == "roll_deg") {
      ParseFloat(value, &current_sample.roll_deg);
    } else if (key == "quality_score") {
      ParseFloat(value, &current_sample.quality_score);
    } else if (key == "sample_weight") {
      ParseFloat(value, &current_sample.sample_weight);
    } else if (key == "preview_width") {
      ParseInt(value, &current_sample.preview_width);
    } else if (key == "preview_height") {
      ParseInt(value, &current_sample.preview_height);
    } else if (key == "image") {
      current_sample.frame_image_path = ResolveArtifactPath(artifact_dir, value);
    } else if (key == "frame_image") {
      current_sample.frame_image_path = ResolveArtifactPath(artifact_dir, value);
    } else if (key == "face_crop") {
      current_sample.face_crop_path = ResolveArtifactPath(artifact_dir, value);
    }
  }

  if (in_sample) {
    package.samples.push_back(current_sample);
  }

  if (package.user_id.empty()) {
    diagnostic.error_message = "missing_user_id";
    return diagnostic;
  }
  if (package.samples.empty()) {
    diagnostic.error_message = "missing_samples";
    return diagnostic;
  }

  for (auto& sample : package.samples) {
    if (sample.sample_weight <= 0.0f) {
      sample.sample_weight = ComputeEnrollmentSampleWeight(sample);
    }
  }

  *out_package = package;
  diagnostic.ok = true;
  return diagnostic;
}

EnrollmentImportDiagnostic ApplyArtifactIdentityToStoreV2(
    const EnrollmentArtifactPackage& package,
    int person_id,
    EnrollmentStoreV2* store) {
  EnrollmentImportDiagnostic diagnostic;
  if (store == nullptr) {
    diagnostic.error_message = "null_store";
    return diagnostic;
  }
  if (person_id < 0) {
    diagnostic.error_message = "invalid_person_id";
    return diagnostic;
  }
  if (package.user_name.empty()) {
    diagnostic.error_message = "missing_user_name";
    return diagnostic;
  }
  if (!store->UpsertPerson(person_id, package.user_name)) {
    diagnostic.error_message = "upsert_person_failed";
    return diagnostic;
  }
  diagnostic.ok = true;
  return diagnostic;
}

EnrollmentImportDiagnostic BuildBaselineEnrollmentPlan(
    const EnrollmentArtifactPackage& package,
    int max_samples,
    BaselineEnrollmentPlan* out_plan) {
  EnrollmentImportDiagnostic diagnostic;
  if (out_plan == nullptr) {
    diagnostic.error_message = "null_output_plan";
    return diagnostic;
  }
  if (max_samples <= 0) {
    diagnostic.error_message = "invalid_max_samples";
    return diagnostic;
  }
  if (package.user_id.empty()) {
    diagnostic.error_message = "missing_user_id";
    return diagnostic;
  }
  if (package.samples.empty()) {
    diagnostic.error_message = "missing_samples";
    return diagnostic;
  }

  std::vector<const EnrollmentArtifactSampleRecord*> ranked;
  ranked.reserve(package.samples.size());
  for (const auto& sample : package.samples) {
    if (!sample.has_face) {
      continue;
    }
    if (sample.face_crop_path.empty() && sample.frame_image_path.empty()) {
      continue;
    }
    ranked.push_back(&sample);
  }
  if (ranked.empty()) {
    diagnostic.error_message = "no_usable_samples";
    return diagnostic;
  }

  std::sort(ranked.begin(), ranked.end(),
            [](const EnrollmentArtifactSampleRecord* a,
               const EnrollmentArtifactSampleRecord* b) {
              if (a->sample_weight != b->sample_weight) {
                return a->sample_weight > b->sample_weight;
              }
              if (a->quality_score != b->quality_score) {
                return a->quality_score > b->quality_score;
              }
              return a->index < b->index;
            });

  BaselineEnrollmentPlan plan;
  plan.user_id = package.user_id;
  plan.user_name = package.user_name;

  const int count = std::min<int>(max_samples, static_cast<int>(ranked.size()));
  plan.samples.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const auto& sample = *ranked[static_cast<std::size_t>(i)];
    BaselineEnrollmentSample baseline_sample;
    baseline_sample.sample_index = sample.index;
    baseline_sample.slot = sample.slot;
    baseline_sample.frame_image_path = sample.frame_image_path;
    baseline_sample.face_crop_path = sample.face_crop_path;
    baseline_sample.preferred_image_path =
        !sample.face_crop_path.empty() ? sample.face_crop_path
                                       : sample.frame_image_path;
    baseline_sample.quality_score = sample.quality_score;
    baseline_sample.yaw_deg = sample.yaw_deg;
    baseline_sample.pitch_deg = sample.pitch_deg;
    baseline_sample.roll_deg = sample.roll_deg;
    baseline_sample.sample_weight = sample.sample_weight;
    plan.samples.push_back(baseline_sample);
  }

  *out_plan = plan;
  diagnostic.ok = true;
  return diagnostic;
}

}  // namespace sentriface::enroll
