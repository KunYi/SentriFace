#include "enroll/enrollment_baseline_generation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>

namespace sentriface::enroll {

namespace {

std::uint32_t Fnv1a32(const std::string& text) {
  std::uint32_t hash = 2166136261u;
  for (unsigned char ch : text) {
    hash ^= static_cast<std::uint32_t>(ch);
    hash *= 16777619u;
  }
  return hash;
}

std::uint32_t Fnv1a32Bytes(const std::vector<unsigned char>& bytes) {
  std::uint32_t hash = 2166136261u;
  for (unsigned char value : bytes) {
    hash ^= static_cast<std::uint32_t>(value);
    hash *= 16777619u;
  }
  return hash;
}

std::vector<unsigned char> ReadFileBytes(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.good()) {
    return {};
  }
  return std::vector<unsigned char>(std::istreambuf_iterator<char>(input),
                                    std::istreambuf_iterator<char>());
}

std::string Hex32(std::uint32_t value) {
  std::ostringstream stream;
  stream << std::hex;
  stream.width(8);
  stream.fill('0');
  stream << value;
  return stream.str();
}

std::string MakeSeedText(const BaselineEnrollmentSample& sample,
                         const std::string& user_id,
                         std::string* out_digest) {
  const std::string path = !sample.preferred_image_path.empty()
                               ? sample.preferred_image_path
                               : sample.frame_image_path;
  const std::vector<unsigned char> bytes = ReadFileBytes(path);
  std::uint32_t digest = 0;
  if (!bytes.empty()) {
    digest = Fnv1a32Bytes(bytes);
    if (out_digest != nullptr) {
      *out_digest = Hex32(digest);
    }
    return Hex32(digest) + "|" + sample.slot + "|" + user_id;
  }

  digest = Fnv1a32(path + "|" + sample.slot + "|" + user_id);
  if (out_digest != nullptr) {
    *out_digest = Hex32(digest);
  }
  return path + "|" + sample.slot + "|" + user_id;
}

std::vector<float> MakeDeterministicEmbedding(const std::string& seed_text,
                                              int embedding_dim) {
  std::vector<float> values(static_cast<std::size_t>(embedding_dim), 0.0f);
  std::uint32_t state = Fnv1a32(seed_text);
  float norm = 0.0f;
  for (int i = 0; i < embedding_dim; ++i) {
    state = state * 1664525u + 1013904223u;
    const float unit =
        static_cast<float>(state & 0x00ffffffu) / 16777215.0f;
    const float centered = unit * 2.0f - 1.0f;
    values[static_cast<std::size_t>(i)] = centered;
    norm += centered * centered;
  }

  norm = std::sqrt(norm);
  if (norm > 1e-9f) {
    for (float& value : values) {
      value /= norm;
    }
  }
  return values;
}

std::vector<std::string> SplitCsvLine(const std::string& line) {
  std::vector<std::string> fields;
  std::string current;
  bool in_quotes = false;
  for (char ch : line) {
    if (ch == '"') {
      in_quotes = !in_quotes;
      continue;
    }
    if (ch == ',' && !in_quotes) {
      fields.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  fields.push_back(current);
  for (std::string& field : fields) {
    while (!field.empty() &&
           (field.back() == '\r' || field.back() == ' ' || field.back() == '\t')) {
      field.pop_back();
    }
    std::size_t start = 0;
    while (start < field.size() &&
           (field[start] == ' ' || field[start] == '\t')) {
      ++start;
    }
    if (start > 0) {
      field.erase(0, start);
    }
  }
  return fields;
}

int FindColumnIndex(const std::vector<std::string>& header, std::string_view name) {
  for (std::size_t i = 0; i < header.size(); ++i) {
    if (header[i] == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool ParseFloatValue(const std::string& text, float* out_value) {
  if (out_value == nullptr) {
    return false;
  }
  try {
    *out_value = std::stof(text);
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseIntValue(const std::string& text, int* out_value) {
  if (out_value == nullptr) {
    return false;
  }
  try {
    *out_value = std::stoi(text);
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace

const char* ToString(BaselineGenerationBackend backend) {
  switch (backend) {
    case BaselineGenerationBackend::kMockDeterministic:
      return "mock_deterministic";
  }
  return "unknown";
}

EnrollmentImportDiagnostic GenerateBaselinePrototypePackage(
    const BaselineEnrollmentPlan& plan,
    const BaselineGenerationConfig& config,
    BaselinePrototypePackage* out_package) {
  switch (config.backend) {
    case BaselineGenerationBackend::kMockDeterministic:
      return GenerateMockBaselinePrototypePackage(plan, config, out_package);
  }

  EnrollmentImportDiagnostic diagnostic;
  diagnostic.error_message = "unsupported_backend";
  return diagnostic;
}

EnrollmentImportDiagnostic BuildBaselineEmbeddingInputManifest(
    const BaselineEnrollmentPlan& plan,
    BaselineEmbeddingInputManifest* out_manifest) {
  EnrollmentImportDiagnostic diagnostic;
  if (out_manifest == nullptr) {
    diagnostic.error_message = "null_output_manifest";
    return diagnostic;
  }
  if (plan.user_id.empty()) {
    diagnostic.error_message = "missing_user_id";
    return diagnostic;
  }
  if (plan.samples.empty()) {
    diagnostic.error_message = "missing_baseline_samples";
    return diagnostic;
  }

  BaselineEmbeddingInputManifest manifest;
  manifest.user_id = plan.user_id;
  manifest.user_name = plan.user_name;
  manifest.records.reserve(plan.samples.size());
  for (const auto& sample : plan.samples) {
    if (sample.preferred_image_path.empty()) {
      diagnostic.error_message = "missing_preferred_image_path";
      return diagnostic;
    }
    BaselineEmbeddingInputRecord record;
    record.sample_index = sample.sample_index;
    record.slot = sample.slot;
    record.image_path = sample.preferred_image_path;
    record.quality_score = sample.quality_score;
    record.sample_weight = sample.sample_weight;
    record.yaw_deg = sample.yaw_deg;
    record.pitch_deg = sample.pitch_deg;
    record.roll_deg = sample.roll_deg;
    manifest.records.push_back(record);
  }

  *out_manifest = manifest;
  diagnostic.ok = true;
  return diagnostic;
}

EnrollmentImportDiagnostic LoadBaselinePrototypePackageFromEmbeddingCsv(
    const std::string& csv_path,
    const BaselineEmbeddingCsvImportConfig& config,
    BaselinePrototypePackage* out_package) {
  EnrollmentImportDiagnostic diagnostic;
  if (out_package == nullptr) {
    diagnostic.error_message = "null_output_package";
    return diagnostic;
  }
  if (config.embedding_dim <= 0) {
    diagnostic.error_message = "invalid_embedding_dim";
    return diagnostic;
  }

  std::ifstream input(csv_path);
  if (!input.good()) {
    diagnostic.error_message = "failed_to_open_embedding_csv";
    return diagnostic;
  }

  std::string header_line;
  if (!std::getline(input, header_line)) {
    diagnostic.error_message = "missing_csv_header";
    return diagnostic;
  }
  const std::vector<std::string> header = SplitCsvLine(header_line);

  const int user_id_col = FindColumnIndex(header, "user_id");
  const int user_name_col = FindColumnIndex(header, "user_name");
  const int sample_index_col = FindColumnIndex(header, "sample_index");
  const int slot_col = FindColumnIndex(header, "slot");
  const int image_path_col = FindColumnIndex(header, "image_path");
  const int quality_col = FindColumnIndex(header, "quality_score");
  if (user_id_col < 0 || user_name_col < 0 || sample_index_col < 0 ||
      slot_col < 0 || image_path_col < 0 || quality_col < 0) {
    diagnostic.error_message = "missing_required_columns";
    return diagnostic;
  }

  std::vector<int> embedding_cols;
  embedding_cols.reserve(static_cast<std::size_t>(config.embedding_dim));
  for (int i = 0; i < config.embedding_dim; ++i) {
    const int col = FindColumnIndex(header, "e" + std::to_string(i));
    if (col < 0) {
      diagnostic.error_message = "missing_embedding_columns";
      return diagnostic;
    }
    embedding_cols.push_back(col);
  }

  BaselinePrototypePackage package;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    const std::vector<std::string> fields = SplitCsvLine(line);
    const auto field_at = [&fields](int index) -> std::string {
      if (index < 0 || index >= static_cast<int>(fields.size())) {
        return std::string();
      }
      return fields[static_cast<std::size_t>(index)];
    };

    if (package.user_id.empty()) {
      package.user_id = field_at(user_id_col);
    }
    if (package.user_name.empty()) {
      package.user_name = field_at(user_name_col);
    }

    BaselinePrototypeRecord record;
    ParseIntValue(field_at(sample_index_col), &record.sample_index);
    record.slot = field_at(slot_col);
    record.source_image_path = field_at(image_path_col);
    record.source_image_digest = Hex32(Fnv1a32(record.source_image_path));
    record.embedding.resize(static_cast<std::size_t>(config.embedding_dim), 0.0f);
    for (int i = 0; i < config.embedding_dim; ++i) {
      float value = 0.0f;
      if (!ParseFloatValue(field_at(embedding_cols[static_cast<std::size_t>(i)]), &value)) {
        diagnostic.error_message = "invalid_embedding_value";
        return diagnostic;
      }
      record.embedding[static_cast<std::size_t>(i)] = value;
    }

    record.metadata.timestamp_ms = config.timestamp_ms;
    ParseFloatValue(field_at(quality_col), &record.metadata.quality_score);
    record.metadata.decision_score = 1.0f;
    record.metadata.top1_top2_margin = 1.0f;
    record.metadata.sample_weight = 1.0f;
    record.metadata.liveness_ok = true;
    record.metadata.manually_enrolled = config.manually_enrolled;
    package.prototypes.push_back(record);
  }

  if (package.user_id.empty()) {
    diagnostic.error_message = "missing_user_id";
    return diagnostic;
  }
  if (package.user_name.empty()) {
    diagnostic.error_message = "missing_user_name";
    return diagnostic;
  }
  if (package.prototypes.empty()) {
    diagnostic.error_message = "missing_prototypes";
    return diagnostic;
  }

  *out_package = package;
  diagnostic.ok = true;
  return diagnostic;
}

EnrollmentImportDiagnostic GenerateMockBaselinePrototypePackage(
    const BaselineEnrollmentPlan& plan,
    const BaselineGenerationConfig& config,
    BaselinePrototypePackage* out_package) {
  EnrollmentImportDiagnostic diagnostic;
  if (out_package == nullptr) {
    diagnostic.error_message = "null_output_package";
    return diagnostic;
  }
  if (config.embedding_dim <= 0) {
    diagnostic.error_message = "invalid_embedding_dim";
    return diagnostic;
  }
  if (plan.user_id.empty()) {
    diagnostic.error_message = "missing_user_id";
    return diagnostic;
  }
  if (plan.samples.empty()) {
    diagnostic.error_message = "missing_baseline_samples";
    return diagnostic;
  }

  BaselinePrototypePackage package;
  package.user_id = plan.user_id;
  package.user_name = plan.user_name;

  const int count = std::min<int>(
      config.max_samples, static_cast<int>(plan.samples.size()));
  package.prototypes.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const auto& sample = plan.samples[static_cast<std::size_t>(i)];
    BaselinePrototypeRecord record;
    record.sample_index = sample.sample_index;
    record.slot = sample.slot;
    record.source_image_path = sample.preferred_image_path;
    std::string digest;
    const std::string seed = MakeSeedText(sample, plan.user_id, &digest);
    record.source_image_digest = digest;
    record.embedding = MakeDeterministicEmbedding(seed, config.embedding_dim);
    record.metadata.timestamp_ms = 0;
    record.metadata.quality_score = sample.quality_score;
    record.metadata.decision_score = 1.0f;
    record.metadata.top1_top2_margin = 1.0f;
    record.metadata.sample_weight = sample.sample_weight;
    record.metadata.liveness_ok = true;
    record.metadata.manually_enrolled = true;
    package.prototypes.push_back(record);
  }

  *out_package = package;
  diagnostic.ok = true;
  return diagnostic;
}

EnrollmentImportDiagnostic ApplyBaselinePrototypePackageToStoreV2(
    const BaselinePrototypePackage& package,
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
  if (package.prototypes.empty()) {
    diagnostic.error_message = "missing_prototypes";
    return diagnostic;
  }

  if (!store->UpsertPerson(person_id, package.user_name)) {
    diagnostic.error_message = "upsert_person_failed";
    return diagnostic;
  }

  for (const auto& prototype : package.prototypes) {
    if (!store->AddPrototype(person_id,
                             PrototypeZone::kBaseline,
                             prototype.embedding,
                             prototype.metadata)) {
      diagnostic.error_message = "add_baseline_prototype_failed";
      return diagnostic;
    }
  }

  diagnostic.ok = true;
  return diagnostic;
}

}  // namespace sentriface::enroll
