#include "enroll/enrollment_baseline_generation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>

#include "camera/image_io.hpp"
#include "embedding/face_embedder.hpp"

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

bool HasBinaryPackageExtension(const std::string& path) {
  return std::filesystem::path(path).extension() ==
         kBaselinePrototypePackageBinaryExtension;
}

bool WriteBytes(std::ostream* out, const void* data, std::size_t size) {
  if (out == nullptr || data == nullptr) {
    return false;
  }
  out->write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
  return out->good();
}

bool ReadBytes(std::istream* in, void* data, std::size_t size) {
  if (in == nullptr || data == nullptr) {
    return false;
  }
  in->read(static_cast<char*>(data), static_cast<std::streamsize>(size));
  return in->good();
}

bool WriteU32(std::ostream* out, std::uint32_t value) {
  unsigned char bytes[4];
  bytes[0] = static_cast<unsigned char>(value & 0xffu);
  bytes[1] = static_cast<unsigned char>((value >> 8) & 0xffu);
  bytes[2] = static_cast<unsigned char>((value >> 16) & 0xffu);
  bytes[3] = static_cast<unsigned char>((value >> 24) & 0xffu);
  return WriteBytes(out, bytes, sizeof(bytes));
}

bool ReadU32(std::istream* in, std::uint32_t* out_value) {
  if (out_value == nullptr) {
    return false;
  }
  unsigned char bytes[4];
  if (!ReadBytes(in, bytes, sizeof(bytes))) {
    return false;
  }
  *out_value = static_cast<std::uint32_t>(bytes[0]) |
               (static_cast<std::uint32_t>(bytes[1]) << 8) |
               (static_cast<std::uint32_t>(bytes[2]) << 16) |
               (static_cast<std::uint32_t>(bytes[3]) << 24);
  return true;
}

bool WriteFloat32(std::ostream* out, float value) {
  std::uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "float size mismatch");
  std::memcpy(&bits, &value, sizeof(bits));
  return WriteU32(out, bits);
}

bool ReadFloat32(std::istream* in, float* out_value) {
  if (out_value == nullptr) {
    return false;
  }
  std::uint32_t bits = 0;
  if (!ReadU32(in, &bits)) {
    return false;
  }
  std::memcpy(out_value, &bits, sizeof(bits));
  return true;
}

bool WriteBool(std::ostream* out, bool value) {
  const unsigned char byte = value ? 1u : 0u;
  return WriteBytes(out, &byte, sizeof(byte));
}

bool ReadBool(std::istream* in, bool* out_value) {
  if (out_value == nullptr) {
    return false;
  }
  unsigned char byte = 0u;
  if (!ReadBytes(in, &byte, sizeof(byte))) {
    return false;
  }
  *out_value = byte != 0u;
  return true;
}

bool WriteString(std::ostream* out, const std::string& value) {
  if (!WriteU32(out, static_cast<std::uint32_t>(value.size()))) {
    return false;
  }
  if (value.empty()) {
    return true;
  }
  return WriteBytes(out, value.data(), value.size());
}

bool ReadString(std::istream* in, std::string* out_value) {
  if (out_value == nullptr) {
    return false;
  }
  std::uint32_t size = 0;
  if (!ReadU32(in, &size)) {
    return false;
  }
  out_value->assign(static_cast<std::size_t>(size), '\0');
  if (size == 0u) {
    return true;
  }
  return ReadBytes(in, out_value->data(), size);
}

bool WriteMetadata(std::ostream* out, const PrototypeMetadata& metadata) {
  return WriteU32(out, metadata.timestamp_ms) &&
         WriteFloat32(out, metadata.quality_score) &&
         WriteFloat32(out, metadata.decision_score) &&
         WriteFloat32(out, metadata.top1_top2_margin) &&
         WriteFloat32(out, metadata.sample_weight) &&
         WriteBool(out, metadata.liveness_ok) &&
         WriteBool(out, metadata.manually_enrolled);
}

bool ReadMetadata(std::istream* in, PrototypeMetadata* out_metadata) {
  if (out_metadata == nullptr) {
    return false;
  }
  return ReadU32(in, &out_metadata->timestamp_ms) &&
         ReadFloat32(in, &out_metadata->quality_score) &&
         ReadFloat32(in, &out_metadata->decision_score) &&
         ReadFloat32(in, &out_metadata->top1_top2_margin) &&
         ReadFloat32(in, &out_metadata->sample_weight) &&
         ReadBool(in, &out_metadata->liveness_ok) &&
         ReadBool(in, &out_metadata->manually_enrolled);
}

EnrollmentImportDiagnostic GenerateOnnxBaselinePrototypePackage(
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
  if (config.model_path.empty()) {
    diagnostic.error_message = "missing_model_path";
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

  sentriface::embedding::EmbeddingConfig embed_config;
  embed_config.input_width = 112;
  embed_config.input_height = 112;
  embed_config.input_channels = 3;
  embed_config.embedding_dim = config.embedding_dim;
  sentriface::embedding::FaceEmbedder embedder(embed_config);
  sentriface::embedding::EmbeddingRuntimeConfig runtime_config;
  runtime_config.backend =
      sentriface::embedding::EmbeddingRuntimeBackend::kOnnxRuntime;
  runtime_config.model_path = config.model_path;
  if (!embedder.InitializeRuntime(runtime_config)) {
    diagnostic.error_message = "embedding_runtime_init_failed";
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
    if (sample.preferred_image_path.empty()) {
      diagnostic.error_message = "missing_preferred_image_path";
      return diagnostic;
    }

    sentriface::camera::Frame image;
    if (!sentriface::camera::LoadBmpFrame(sample.preferred_image_path, &image)) {
      diagnostic.error_message = "failed_to_load_preferred_image";
      return diagnostic;
    }

    const auto embedding_result = embedder.Run(image);
    if (!embedding_result.ok ||
        static_cast<int>(embedding_result.values.size()) != config.embedding_dim) {
      diagnostic.error_message = "embedding_inference_failed";
      return diagnostic;
    }

    BaselinePrototypeRecord record;
    record.sample_index = sample.sample_index;
    record.slot = sample.slot;
    record.source_image_path = sample.preferred_image_path;
    record.source_image_digest = Hex32(Fnv1a32(record.source_image_path));
    record.embedding = embedding_result.values;
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

}  // namespace

const char* ToString(BaselineGenerationBackend backend) {
  switch (backend) {
    case BaselineGenerationBackend::kMockDeterministic:
      return "mock_deterministic";
    case BaselineGenerationBackend::kOnnxRuntime:
      return "onnxruntime";
  }
  return "unknown";
}

std::string MakeBaselinePrototypePackagePath(const std::string& input_path) {
  std::filesystem::path path(input_path);
  path.replace_extension(kBaselinePrototypePackageBinaryExtension);
  return path.string();
}

std::string MakeFaceSearchV2IndexPath(const std::string& input_path) {
  std::filesystem::path path(input_path);
  path.replace_extension(sentriface::search::kFaceSearchV2IndexBinaryExtension);
  return path.string();
}

int InferBaselinePrototypePackageEmbeddingDim(
    const BaselinePrototypePackage& package) {
  if (package.prototypes.empty()) {
    return 0;
  }
  const int embedding_dim =
      static_cast<int>(package.prototypes.front().embedding.size());
  if (embedding_dim <= 0) {
    return 0;
  }
  for (const auto& prototype : package.prototypes) {
    if (static_cast<int>(prototype.embedding.size()) != embedding_dim) {
      return 0;
    }
  }
  return embedding_dim;
}

EnrollmentImportDiagnostic GenerateBaselinePrototypePackage(
    const BaselineEnrollmentPlan& plan,
    const BaselineGenerationConfig& config,
    BaselinePrototypePackage* out_package) {
  switch (config.backend) {
    case BaselineGenerationBackend::kMockDeterministic:
      return GenerateMockBaselinePrototypePackage(plan, config, out_package);
    case BaselineGenerationBackend::kOnnxRuntime:
      return GenerateOnnxBaselinePrototypePackage(plan, config, out_package);
  }

  EnrollmentImportDiagnostic diagnostic;
  diagnostic.error_message = "unsupported_backend";
  return diagnostic;
}

EnrollmentImportDiagnostic GenerateBaselinePrototypePackageFromArtifactSummary(
    const std::string& summary_path,
    const BaselineGenerationConfig& config,
    EnrollmentArtifactPackage* out_artifact,
    BaselineEnrollmentPlan* out_plan,
    BaselinePrototypePackage* out_package) {
  EnrollmentImportDiagnostic diagnostic;
  if (out_package == nullptr) {
    diagnostic.error_message = "null_output_package";
    return diagnostic;
  }
  if (config.max_samples <= 0) {
    diagnostic.error_message = "invalid_max_samples";
    return diagnostic;
  }

  EnrollmentArtifactPackage artifact;
  diagnostic = LoadEnrollmentArtifactPackage(summary_path, &artifact);
  if (!diagnostic.ok) {
    return diagnostic;
  }

  BaselineEnrollmentPlan plan;
  diagnostic = BuildBaselineEnrollmentPlan(artifact, config.max_samples, &plan);
  if (!diagnostic.ok) {
    return diagnostic;
  }

  diagnostic = GenerateBaselinePrototypePackage(plan, config, out_package);
  if (!diagnostic.ok) {
    return diagnostic;
  }

  if (out_artifact != nullptr) {
    *out_artifact = artifact;
  }
  if (out_plan != nullptr) {
    *out_plan = plan;
  }

  diagnostic.ok = true;
  return diagnostic;
}

EnrollmentImportDiagnostic GenerateAndSaveBaselinePackageArtifactsFromArtifactSummary(
    const std::string& summary_path,
    int person_id,
    const BaselineGenerationConfig& config,
    float baseline_weight,
    const std::string& output_summary_path,
    EnrollmentArtifactPackage* out_artifact,
    BaselineEnrollmentPlan* out_plan,
    BaselinePrototypePackage* out_package,
    std::string* out_baseline_package_path,
    std::string* out_search_index_path) {
  EnrollmentImportDiagnostic diagnostic;
  if (person_id < 0) {
    diagnostic.error_message = "invalid_person_id";
    return diagnostic;
  }

  BaselinePrototypePackage package;
  diagnostic = GenerateBaselinePrototypePackageFromArtifactSummary(
      summary_path, config, out_artifact, out_plan, &package);
  if (!diagnostic.ok) {
    return diagnostic;
  }

  const std::string baseline_package_path =
      MakeBaselinePrototypePackagePath(output_summary_path);
  const std::string search_index_path =
      MakeFaceSearchV2IndexPath(output_summary_path);
  const int embedding_dim = InferBaselinePrototypePackageEmbeddingDim(package);
  if (embedding_dim <= 0) {
    diagnostic.error_message = "invalid_embedding_dim";
    return diagnostic;
  }

  diagnostic = SaveBaselinePackageArtifacts(
      package, person_id, embedding_dim, baseline_weight, baseline_package_path,
      search_index_path);
  if (!diagnostic.ok) {
    return diagnostic;
  }

  if (out_package != nullptr) {
    *out_package = package;
  }
  if (out_baseline_package_path != nullptr) {
    *out_baseline_package_path = baseline_package_path;
  }
  if (out_search_index_path != nullptr) {
    *out_search_index_path = search_index_path;
  }

  diagnostic.ok = true;
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

EnrollmentImportDiagnostic SaveBaselinePrototypePackageBinary(
    const BaselinePrototypePackage& package,
    const std::string& output_path) {
  EnrollmentImportDiagnostic diagnostic;
  if (output_path.empty()) {
    diagnostic.error_message = "missing_output_path";
    return diagnostic;
  }
  if (package.user_id.empty() || package.user_name.empty() ||
      package.prototypes.empty()) {
    diagnostic.error_message = "invalid_package";
    return diagnostic;
  }

  const std::size_t embedding_dim = package.prototypes.front().embedding.size();
  if (embedding_dim == 0U) {
    diagnostic.error_message = "invalid_embedding_dim";
    return diagnostic;
  }
  for (const auto& prototype : package.prototypes) {
    if (prototype.embedding.size() != embedding_dim) {
      diagnostic.error_message = "inconsistent_embedding_dim";
      return diagnostic;
    }
  }

  std::ofstream out(output_path, std::ios::binary);
  if (!out.good()) {
    diagnostic.error_message = "failed_to_open_binary_package";
    return diagnostic;
  }

  static constexpr char kMagic[4] = {'S', 'F', 'B', 'P'};
  if (!WriteBytes(&out, kMagic, sizeof(kMagic)) ||
      !WriteU32(&out, 1u) ||
      !WriteU32(&out, static_cast<std::uint32_t>(embedding_dim)) ||
      !WriteU32(&out, static_cast<std::uint32_t>(package.prototypes.size())) ||
      !WriteString(&out, package.user_id) ||
      !WriteString(&out, package.user_name)) {
    diagnostic.error_message = "failed_to_write_binary_header";
    return diagnostic;
  }

  for (const auto& prototype : package.prototypes) {
    if (!WriteU32(&out, static_cast<std::uint32_t>(prototype.sample_index)) ||
        !WriteString(&out, prototype.slot) ||
        !WriteString(&out, prototype.source_image_path) ||
        !WriteString(&out, prototype.source_image_digest) ||
        !WriteMetadata(&out, prototype.metadata)) {
      diagnostic.error_message = "failed_to_write_binary_record";
      return diagnostic;
    }
    for (float value : prototype.embedding) {
      if (!WriteFloat32(&out, value)) {
        diagnostic.error_message = "failed_to_write_embedding_value";
        return diagnostic;
      }
    }
  }

  diagnostic.ok = true;
  return diagnostic;
}

EnrollmentImportDiagnostic SaveBaselinePackageArtifacts(
    const BaselinePrototypePackage& package,
    int person_id,
    int embedding_dim,
    float baseline_weight,
    const std::string& baseline_package_path,
    const std::string& search_index_path) {
  EnrollmentImportDiagnostic diagnostic =
      SaveBaselinePrototypePackageBinary(package, baseline_package_path);
  if (!diagnostic.ok) {
    return diagnostic;
  }

  sentriface::search::FaceSearchV2IndexPackage index_package;
  diagnostic = BuildFaceSearchV2IndexPackageFromBaselinePrototypePackage(
      package, person_id, embedding_dim, baseline_weight, &index_package);
  if (!diagnostic.ok) {
    return diagnostic;
  }

  const auto save_index_result =
      sentriface::search::SaveFaceSearchV2IndexPackageBinary(index_package,
                                                             search_index_path);
  if (!save_index_result.ok) {
    diagnostic.ok = false;
    diagnostic.error_message = save_index_result.error_message;
    return diagnostic;
  }

  diagnostic.ok = true;
  diagnostic.error_message.clear();
  return diagnostic;
}

EnrollmentImportDiagnostic LoadBaselinePrototypePackageBinary(
    const std::string& input_path,
    BaselinePrototypePackage* out_package) {
  EnrollmentImportDiagnostic diagnostic;
  if (out_package == nullptr) {
    diagnostic.error_message = "null_output_package";
    return diagnostic;
  }

  std::ifstream in(input_path, std::ios::binary);
  if (!in.good()) {
    diagnostic.error_message = "failed_to_open_binary_package";
    return diagnostic;
  }

  char magic[4] = {};
  if (!ReadBytes(&in, magic, sizeof(magic)) ||
      std::memcmp(magic, "SFBP", sizeof(magic)) != 0) {
    diagnostic.error_message = "invalid_binary_magic";
    return diagnostic;
  }

  std::uint32_t version = 0;
  std::uint32_t embedding_dim = 0;
  std::uint32_t prototype_count = 0;
  BaselinePrototypePackage package;
  if (!ReadU32(&in, &version) || version != 1u ||
      !ReadU32(&in, &embedding_dim) || embedding_dim == 0u ||
      !ReadU32(&in, &prototype_count) ||
      !ReadString(&in, &package.user_id) ||
      !ReadString(&in, &package.user_name)) {
    diagnostic.error_message = "invalid_binary_header";
    return diagnostic;
  }

  package.prototypes.reserve(prototype_count);
  for (std::uint32_t i = 0; i < prototype_count; ++i) {
    BaselinePrototypeRecord record;
    std::uint32_t sample_index = 0;
    if (!ReadU32(&in, &sample_index) ||
        !ReadString(&in, &record.slot) ||
        !ReadString(&in, &record.source_image_path) ||
        !ReadString(&in, &record.source_image_digest) ||
        !ReadMetadata(&in, &record.metadata)) {
      diagnostic.error_message = "invalid_binary_record";
      return diagnostic;
    }
    record.sample_index = static_cast<int>(sample_index);
    record.embedding.resize(embedding_dim, 0.0f);
    for (std::uint32_t dim = 0; dim < embedding_dim; ++dim) {
      if (!ReadFloat32(&in, &record.embedding[dim])) {
        diagnostic.error_message = "invalid_binary_embedding";
        return diagnostic;
      }
    }
    package.prototypes.push_back(std::move(record));
  }

  *out_package = std::move(package);
  diagnostic.ok = true;
  return diagnostic;
}

EnrollmentImportDiagnostic LoadBaselinePrototypePackage(
    const std::string& input_path,
    const BaselineEmbeddingCsvImportConfig& config,
    BaselinePrototypePackage* out_package) {
  if (HasBinaryPackageExtension(input_path)) {
    return LoadBaselinePrototypePackageBinary(input_path, out_package);
  }
  return LoadBaselinePrototypePackageFromEmbeddingCsv(input_path, config,
                                                      out_package);
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

EnrollmentImportDiagnostic LoadBaselinePrototypePackageIntoStoreV2(
    const std::string& input_path,
    const BaselineEmbeddingCsvImportConfig& config,
    int person_id,
    EnrollmentStoreV2* store) {
  EnrollmentImportDiagnostic diagnostic;
  if (store == nullptr) {
    diagnostic.error_message = "null_store";
    return diagnostic;
  }

  BaselinePrototypePackage package;
  diagnostic = LoadBaselinePrototypePackage(input_path, config, &package);
  if (!diagnostic.ok) {
    return diagnostic;
  }
  return ApplyBaselinePrototypePackageToStoreV2(package, person_id, store);
}

EnrollmentImportDiagnostic BuildFaceSearchV2IndexPackageFromBaselinePrototypePackage(
    const BaselinePrototypePackage& package,
    int person_id,
    int embedding_dim,
    float baseline_weight,
    sentriface::search::FaceSearchV2IndexPackage* out_package) {
  EnrollmentImportDiagnostic diagnostic;
  if (out_package == nullptr) {
    diagnostic.error_message = "null_output_package";
    return diagnostic;
  }
  if (person_id < 0) {
    diagnostic.error_message = "invalid_person_id";
    return diagnostic;
  }
  if (embedding_dim <= 0) {
    diagnostic.error_message = "invalid_embedding_dim";
    return diagnostic;
  }
  if (baseline_weight < 0.0f) {
    diagnostic.error_message = "invalid_baseline_weight";
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

  std::vector<sentriface::search::FacePrototypeV2> search_prototypes;
  search_prototypes.reserve(package.prototypes.size());
  int prototype_id = 0;
  for (const auto& prototype : package.prototypes) {
    if (static_cast<int>(prototype.embedding.size()) != embedding_dim) {
      diagnostic.error_message = "embedding_dim_mismatch";
      return diagnostic;
    }
    sentriface::search::FacePrototypeV2 search_prototype;
    search_prototype.person_id = person_id;
    search_prototype.prototype_id = prototype_id;
    search_prototype.zone = static_cast<int>(PrototypeZone::kBaseline);
    search_prototype.label = package.user_name;
    search_prototype.embedding = prototype.embedding;
    search_prototype.prototype_weight = baseline_weight;
    search_prototypes.push_back(std::move(search_prototype));
    ++prototype_id;
  }

  const auto build_result = sentriface::search::BuildFaceSearchV2IndexPackage(
      search_prototypes, embedding_dim, out_package);
  if (!build_result.ok) {
    diagnostic.error_message = build_result.error_message;
    return diagnostic;
  }

  diagnostic.ok = true;
  return diagnostic;
}

EnrollmentImportDiagnostic LoadOrBuildFaceSearchV2IndexPackage(
    const std::string& input_path,
    const BaselinePrototypePackage& package,
    int person_id,
    int embedding_dim,
    float baseline_weight,
    sentriface::search::FaceSearchV2IndexPackage* out_package,
    std::string* out_index_input) {
  EnrollmentImportDiagnostic diagnostic;
  if (out_package == nullptr) {
    diagnostic.error_message = "null_output_package";
    return diagnostic;
  }
  if (out_index_input == nullptr) {
    diagnostic.error_message = "null_output_index_input";
    return diagnostic;
  }

  const std::filesystem::path search_index_path(
      MakeFaceSearchV2IndexPath(input_path));
  if (std::filesystem::exists(search_index_path)) {
    const auto load_result =
        sentriface::search::LoadFaceSearchV2IndexPackageBinary(
            search_index_path.string(), out_package);
    if (!load_result.ok) {
      diagnostic.error_message = load_result.error_message;
      return diagnostic;
    }
    *out_index_input = search_index_path.string();
    diagnostic.ok = true;
    return diagnostic;
  }

  diagnostic = BuildFaceSearchV2IndexPackageFromBaselinePrototypePackage(
      package, person_id, embedding_dim, baseline_weight, out_package);
  if (!diagnostic.ok) {
    return diagnostic;
  }
  const auto save_result =
      sentriface::search::SaveFaceSearchV2IndexPackageBinary(
          *out_package, search_index_path.string());
  if (!save_result.ok) {
    diagnostic.ok = false;
    diagnostic.error_message = save_result.error_message;
    return diagnostic;
  }
  *out_index_input = "package_rebuild";
  diagnostic.ok = true;
  return diagnostic;
}

}  // namespace sentriface::enroll
