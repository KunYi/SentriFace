#include "search/dot_kernel.hpp"
#include "search/face_search.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace sentriface::search {

namespace {

constexpr float kMinNormDenom = 1e-9f;

bool UseDotKernelPathForDim(int embedding_dim) {
  return embedding_dim == static_cast<int>(kDotKernelDimension);
}

void AppendNormalized512(const std::vector<float>& embedding,
                         std::vector<float>* out_matrix) {
  if (out_matrix == nullptr) {
    return;
  }
  const std::size_t offset = out_matrix->size();
  out_matrix->resize(offset + kDotKernelDimension);
  std::copy(embedding.begin(), embedding.end(), out_matrix->begin() + offset);
  Normalize512InPlace(out_matrix->data() + offset);
}

void NormalizeVectorInPlace(float* values, std::size_t size) {
  if (values == nullptr || size == 0U) {
    return;
  }
  float norm = 0.0f;
  for (std::size_t i = 0; i < size; ++i) {
    norm += values[i] * values[i];
  }
  norm = std::sqrt(norm);
  if (norm < kMinNormDenom) {
    for (std::size_t i = 0; i < size; ++i) {
      values[i] = 0.0f;
    }
    return;
  }
  for (std::size_t i = 0; i < size; ++i) {
    values[i] /= norm;
  }
}

void AppendNormalizedEmbedding(const std::vector<float>& embedding,
                               std::vector<float>* out_matrix) {
  if (out_matrix == nullptr) {
    return;
  }
  if (embedding.size() == kDotKernelDimension) {
    AppendNormalized512(embedding, out_matrix);
    return;
  }
  const std::size_t offset = out_matrix->size();
  out_matrix->resize(offset + embedding.size());
  std::copy(embedding.begin(), embedding.end(), out_matrix->begin() + offset);
  NormalizeVectorInPlace(out_matrix->data() + offset, embedding.size());
}

float CosineSimilarityImpl(const std::vector<float>& a,
                           const std::vector<float>& b) {
  if (a.size() != b.size() || a.empty()) {
    return 0.0f;
  }

  if (a.size() == kDotKernelDimension) {
    const float dot = Dot512(a.data(), b.data());
    const float norm_a = std::sqrt(Dot512(a.data(), a.data()));
    const float norm_b = std::sqrt(Dot512(b.data(), b.data()));
    const float denom = norm_a * norm_b;
    if (denom < kMinNormDenom) {
      return 0.0f;
    }
    return dot / denom;
  }

  float dot = 0.0f;
  float norm_a = 0.0f;
  float norm_b = 0.0f;
  for (std::size_t i = 0; i < a.size(); ++i) {
    dot += a[i] * b[i];
    norm_a += a[i] * a[i];
    norm_b += b[i] * b[i];
  }

  const float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
  if (denom < kMinNormDenom) {
    return 0.0f;
  }
  return dot / denom;
}

bool SearchHitLess(const SearchHit& a, const SearchHit& b) {
  if (a.score != b.score) {
    return a.score > b.score;
  }
  if (a.person_id != b.person_id) {
    return a.person_id < b.person_id;
  }
  return a.label < b.label;
}

bool PrototypeHitLess(const PrototypeHit& a, const PrototypeHit& b) {
  if (a.weighted_score != b.weighted_score) {
    return a.weighted_score > b.weighted_score;
  }
  if (a.raw_score != b.raw_score) {
    return a.raw_score > b.raw_score;
  }
  if (a.person_id != b.person_id) {
    return a.person_id < b.person_id;
  }
  return a.prototype_id < b.prototype_id;
}

bool SearchHitV2Less(const SearchHitV2& a, const SearchHitV2& b) {
  if (a.score != b.score) {
    return a.score > b.score;
  }
  if (a.person_id != b.person_id) {
    return a.person_id < b.person_id;
  }
  return a.best_prototype_id < b.best_prototype_id;
}

template <typename T, typename Compare>
void SortTopK(std::vector<T>* values, int top_k, Compare compare) {
  if (values == nullptr || values->empty()) {
    return;
  }

  if (top_k <= 0 || static_cast<std::size_t>(top_k) >= values->size()) {
    std::sort(values->begin(), values->end(), compare);
    return;
  }

  auto middle = values->begin() + top_k;
  std::partial_sort(values->begin(), middle, values->end(), compare);
  values->resize(static_cast<std::size_t>(top_k));
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

bool IsValidIndexPackage(const FaceSearchV2IndexPackage& package) {
  if (package.embedding_dim <= 0 || package.entries.empty()) {
    return false;
  }
  return package.normalized_matrix.size() ==
         package.entries.size() * static_cast<std::size_t>(package.embedding_dim);
}

void DotNormalizedMatrixScalar(const std::vector<float>& normalized_query,
                               const std::vector<float>& normalized_matrix,
                               std::size_t row_count,
                               std::size_t embedding_dim,
                               std::vector<float>* out_scores) {
  if (out_scores == nullptr) {
    return;
  }
  out_scores->assign(row_count, 0.0f);
  for (std::size_t row = 0; row < row_count; ++row) {
    float dot = 0.0f;
    const std::size_t offset = row * embedding_dim;
    for (std::size_t col = 0; col < embedding_dim; ++col) {
      dot += normalized_query[col] * normalized_matrix[offset + col];
    }
    (*out_scores)[row] = dot;
  }
}

}  // namespace

FaceSearch::FaceSearch(const SearchConfig& config) : config_(config) {}

bool FaceSearch::AddPrototype(const FacePrototype& prototype) {
  if (static_cast<int>(prototype.embedding.size()) != config_.embedding_dim) {
    return false;
  }

  FaceSearchIndexEntry entry;
  entry.person_id = prototype.person_id;
  entry.label = prototype.label;
  if (!UseDotKernelPath()) {
    entry.embedding = prototype.embedding;
  }
  entries_.push_back(std::move(entry));

  if (UseDotKernelPath()) {
    AppendNormalizedEmbedding(prototype.embedding, &normalized_matrix_);
  }
  return true;
}

bool FaceSearch::RebuildFromPrototypes(
    const std::vector<FacePrototype>& prototypes) {
  Clear();
  entries_.reserve(prototypes.size());
  if (UseDotKernelPath()) {
    normalized_matrix_.reserve(prototypes.size() * kDotKernelDimension);
  }
  for (const auto& prototype : prototypes) {
    if (!AddPrototype(prototype)) {
      Clear();
      return false;
    }
  }
  return true;
}

void FaceSearch::Clear() {
  entries_.clear();
  normalized_matrix_.clear();
}

std::size_t FaceSearch::PrototypeCount() const { return entries_.size(); }

SearchResult FaceSearch::Query(const std::vector<float>& embedding) const {
  SearchResult result;
  if (static_cast<int>(embedding.size()) != config_.embedding_dim ||
      entries_.empty()) {
    return result;
  }

  if (UseDotKernelPath() &&
      normalized_matrix_.size() == entries_.size() * kDotKernelDimension) {
    std::vector<float> normalized_query = embedding;
    Normalize512InPlace(normalized_query.data());

    std::vector<float> scores(entries_.size(), 0.0f);
    Dot512Batch(normalized_query.data(), normalized_matrix_.data(),
                entries_.size(), scores.data());

    result.hits.reserve(entries_.size());
    for (std::size_t i = 0; i < entries_.size(); ++i) {
      SearchHit hit;
      hit.person_id = entries_[i].person_id;
      hit.label = entries_[i].label;
      hit.score = scores[i];
      result.hits.push_back(hit);
    }
  } else {
    result.hits.reserve(entries_.size());
    for (const auto& entry : entries_) {
      SearchHit hit;
      hit.person_id = entry.person_id;
      hit.label = entry.label;
      hit.score = CosineSimilarity(embedding, entry.embedding);
      result.hits.push_back(hit);
    }
  }

  SortTopK(&result.hits, config_.top_k, SearchHitLess);

  if (!result.hits.empty()) {
    result.ok = true;
    if (result.hits.size() >= 2) {
      result.top1_top2_margin = result.hits[0].score - result.hits[1].score;
    } else {
      result.top1_top2_margin = result.hits[0].score;
    }
  }

  return result;
}

float FaceSearch::CosineSimilarity(const std::vector<float>& a,
                                   const std::vector<float>& b) const {
  return CosineSimilarityImpl(a, b);
}

bool FaceSearch::UseDotKernelPath() const {
  return UseDotKernelPathForDim(config_.embedding_dim);
}

FaceSearchV2::FaceSearchV2(const SearchConfig& config) : config_(config) {}

bool FaceSearchV2::AddPrototype(const FacePrototypeV2& prototype) {
  if (static_cast<int>(prototype.embedding.size()) != config_.embedding_dim ||
      prototype.person_id < 0 || prototype.prototype_id < 0 ||
      prototype.prototype_weight < 0.0f) {
    return false;
  }

  FaceSearchV2IndexEntry entry;
  entry.person_id = prototype.person_id;
  entry.prototype_id = prototype.prototype_id;
  entry.zone = prototype.zone;
  entry.label = prototype.label;
  entry.prototype_weight = prototype.prototype_weight;
  if (!UseDotKernelPath()) {
    entry.embedding = prototype.embedding;
  }
  entries_.push_back(std::move(entry));

  if (UseDotKernelPath()) {
    AppendNormalizedEmbedding(prototype.embedding, &normalized_matrix_);
  }
  return true;
}

bool FaceSearchV2::RebuildFromPrototypes(
    const std::vector<FacePrototypeV2>& prototypes) {
  Clear();
  entries_.reserve(prototypes.size());
  if (UseDotKernelPath()) {
    normalized_matrix_.reserve(prototypes.size() * kDotKernelDimension);
  }
  for (const auto& prototype : prototypes) {
    if (!AddPrototype(prototype)) {
      Clear();
      return false;
    }
  }
  return true;
}

bool FaceSearchV2::LoadFromIndexPackage(const FaceSearchV2IndexPackage& package) {
  if (config_.embedding_dim != package.embedding_dim ||
      !IsValidIndexPackage(package)) {
    return false;
  }

  Clear();
  entries_.reserve(package.entries.size());
  for (const auto& record : package.entries) {
    if (record.person_id < 0 || record.prototype_id < 0 ||
        record.prototype_weight < 0.0f) {
      Clear();
      return false;
    }

    FaceSearchV2IndexEntry entry;
    entry.person_id = record.person_id;
    entry.prototype_id = record.prototype_id;
    entry.zone = record.zone;
    entry.label = record.label;
    entry.prototype_weight = record.prototype_weight;
    entries_.push_back(std::move(entry));
  }
  normalized_matrix_ = package.normalized_matrix;
  return true;
}

bool FaceSearchV2::LoadFromIndexPackagePath(const std::string& input_path) {
  FaceSearchV2IndexPackage package;
  const auto diagnostic =
      LoadFaceSearchV2IndexPackageBinary(input_path, &package);
  return diagnostic.ok && LoadFromIndexPackage(package);
}

bool FaceSearchV2::ExportIndexPackage(FaceSearchV2IndexPackage* out_package) const {
  if (out_package == nullptr || entries_.empty()) {
    return false;
  }

  FaceSearchV2IndexPackage package;
  package.embedding_dim = config_.embedding_dim;
  package.entries.reserve(entries_.size());
  for (const auto& entry : entries_) {
    FaceSearchV2IndexRecord record;
    record.person_id = entry.person_id;
    record.prototype_id = entry.prototype_id;
    record.zone = entry.zone;
    record.label = entry.label;
    record.prototype_weight = entry.prototype_weight;
    package.entries.push_back(std::move(record));
  }
  if (normalized_matrix_.size() ==
      entries_.size() * static_cast<std::size_t>(config_.embedding_dim)) {
    package.normalized_matrix = normalized_matrix_;
  } else {
    package.normalized_matrix.reserve(
        entries_.size() * static_cast<std::size_t>(config_.embedding_dim));
    for (const auto& entry : entries_) {
      if (static_cast<int>(entry.embedding.size()) != config_.embedding_dim) {
        return false;
      }
      AppendNormalizedEmbedding(entry.embedding, &package.normalized_matrix);
    }
  }
  *out_package = std::move(package);
  return true;
}

bool FaceSearchV2::SaveIndexPackageBinary(const std::string& output_path) const {
  FaceSearchV2IndexPackage package;
  if (!ExportIndexPackage(&package)) {
    return false;
  }
  const auto diagnostic =
      SaveFaceSearchV2IndexPackageBinary(package, output_path);
  return diagnostic.ok;
}

void FaceSearchV2::Clear() {
  entries_.clear();
  normalized_matrix_.clear();
}

std::size_t FaceSearchV2::PrototypeCount() const { return entries_.size(); }

SearchResultV2 FaceSearchV2::Query(const std::vector<float>& embedding) const {
  SearchResultV2 result;
  if (static_cast<int>(embedding.size()) != config_.embedding_dim ||
      entries_.empty()) {
    return result;
  }

  struct PersonBestHit {
    std::string label;
    float score = 0.0f;
    int best_prototype_id = -1;
    int best_zone = 0;
    bool initialized = false;
  };

  std::unordered_map<int, PersonBestHit> best_by_person;
  best_by_person.reserve(entries_.size());

  std::vector<float> raw_scores(entries_.size(), 0.0f);
  if (normalized_matrix_.size() ==
      entries_.size() * static_cast<std::size_t>(config_.embedding_dim)) {
    std::vector<float> normalized_query = embedding;
    if (UseDotKernelPath()) {
      Normalize512InPlace(normalized_query.data());
      Dot512Batch(normalized_query.data(), normalized_matrix_.data(),
                  entries_.size(), raw_scores.data());
    } else {
      NormalizeVectorInPlace(normalized_query.data(), normalized_query.size());
      DotNormalizedMatrixScalar(normalized_query, normalized_matrix_,
                                entries_.size(),
                                static_cast<std::size_t>(config_.embedding_dim),
                                &raw_scores);
    }
  } else {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
      raw_scores[i] = CosineSimilarityImpl(embedding, entries_[i].embedding);
    }
  }

  result.prototype_hits.reserve(entries_.size());
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const auto& entry = entries_[i];

    PrototypeHit hit;
    hit.person_id = entry.person_id;
    hit.prototype_id = entry.prototype_id;
    hit.zone = entry.zone;
    hit.label = entry.label;
    hit.raw_score = raw_scores[i];
    hit.weighted_score = hit.raw_score * entry.prototype_weight;
    result.prototype_hits.push_back(hit);

    auto& best = best_by_person[entry.person_id];
    if (!best.initialized || hit.weighted_score > best.score) {
      best.label = entry.label;
      best.score = hit.weighted_score;
      best.best_prototype_id = entry.prototype_id;
      best.best_zone = entry.zone;
      best.initialized = true;
    }
  }

  std::sort(result.prototype_hits.begin(), result.prototype_hits.end(),
            PrototypeHitLess);

  result.hits.reserve(best_by_person.size());
  for (const auto& entry : best_by_person) {
    SearchHitV2 hit;
    hit.person_id = entry.first;
    hit.label = entry.second.label;
    hit.score = entry.second.score;
    hit.best_prototype_id = entry.second.best_prototype_id;
    hit.best_zone = entry.second.best_zone;
    result.hits.push_back(hit);
  }

  SortTopK(&result.hits, config_.top_k, SearchHitV2Less);

  if (!result.hits.empty()) {
    result.ok = true;
    if (result.hits.size() >= 2U) {
      result.top1_top2_margin = result.hits[0].score - result.hits[1].score;
    } else {
      result.top1_top2_margin = result.hits[0].score;
    }
  }

  return result;
}

bool FaceSearchV2::UseDotKernelPath() const {
  return UseDotKernelPathForDim(config_.embedding_dim);
}

SearchResult ToSearchResult(const SearchResultV2& result_v2) {
  SearchResult result;
  result.ok = result_v2.ok;
  result.top1_top2_margin = result_v2.top1_top2_margin;
  result.hits.reserve(result_v2.hits.size());
  for (const auto& hit_v2 : result_v2.hits) {
    SearchHit hit;
    hit.person_id = hit_v2.person_id;
    hit.label = hit_v2.label;
    hit.score = hit_v2.score;
    result.hits.push_back(hit);
  }
  return result;
}

SearchIndexIoDiagnostic BuildFaceSearchV2IndexPackage(
    const std::vector<FacePrototypeV2>& prototypes,
    int embedding_dim,
    FaceSearchV2IndexPackage* out_package) {
  SearchIndexIoDiagnostic diagnostic;
  if (out_package == nullptr) {
    diagnostic.error_message = "null_output_package";
    return diagnostic;
  }
  if (embedding_dim <= 0) {
    diagnostic.error_message = "invalid_embedding_dim";
    return diagnostic;
  }
  if (prototypes.empty()) {
    diagnostic.error_message = "missing_prototypes";
    return diagnostic;
  }

  SearchConfig config;
  config.embedding_dim = embedding_dim;
  config.top_k = 5;
  FaceSearchV2 search(config);
  if (!search.RebuildFromPrototypes(prototypes)) {
    diagnostic.error_message = "rebuild_from_prototypes_failed";
    return diagnostic;
  }
  if (!search.ExportIndexPackage(out_package)) {
    diagnostic.error_message = "export_index_package_failed";
    return diagnostic;
  }

  diagnostic.ok = true;
  return diagnostic;
}

SearchIndexIoDiagnostic BuildAndSaveFaceSearchV2IndexPackageBinary(
    const std::vector<FacePrototypeV2>& prototypes,
    int embedding_dim,
    const std::string& output_path) {
  FaceSearchV2IndexPackage package;
  SearchIndexIoDiagnostic diagnostic =
      BuildFaceSearchV2IndexPackage(prototypes, embedding_dim, &package);
  if (!diagnostic.ok) {
    return diagnostic;
  }
  return SaveFaceSearchV2IndexPackageBinary(package, output_path);
}

SearchIndexIoDiagnostic SaveFaceSearchV2IndexPackageBinary(
    const FaceSearchV2IndexPackage& package,
    const std::string& output_path) {
  SearchIndexIoDiagnostic diagnostic;
  if (output_path.empty()) {
    diagnostic.error_message = "missing_output_path";
    return diagnostic;
  }
  if (!IsValidIndexPackage(package)) {
    diagnostic.error_message = "invalid_index_package";
    return diagnostic;
  }

  std::ofstream out(output_path, std::ios::binary);
  if (!out.good()) {
    diagnostic.error_message = "failed_to_open_index_package";
    return diagnostic;
  }

  static constexpr char kMagic[4] = {'S', 'F', 'S', 'I'};
  if (!WriteBytes(&out, kMagic, sizeof(kMagic)) ||
      !WriteU32(&out, 1u) ||
      !WriteU32(&out, static_cast<std::uint32_t>(package.embedding_dim)) ||
      !WriteU32(&out, static_cast<std::uint32_t>(package.entries.size()))) {
    diagnostic.error_message = "failed_to_write_index_header";
    return diagnostic;
  }

  for (const auto& entry : package.entries) {
    if (!WriteU32(&out, static_cast<std::uint32_t>(entry.person_id)) ||
        !WriteU32(&out, static_cast<std::uint32_t>(entry.prototype_id)) ||
        !WriteU32(&out, static_cast<std::uint32_t>(entry.zone)) ||
        !WriteString(&out, entry.label) ||
        !WriteFloat32(&out, entry.prototype_weight)) {
      diagnostic.error_message = "failed_to_write_index_record";
      return diagnostic;
    }
  }

  for (float value : package.normalized_matrix) {
    if (!WriteFloat32(&out, value)) {
      diagnostic.error_message = "failed_to_write_index_matrix";
      return diagnostic;
    }
  }

  diagnostic.ok = true;
  return diagnostic;
}

SearchIndexIoDiagnostic LoadFaceSearchV2IndexPackageBinary(
    const std::string& input_path,
    FaceSearchV2IndexPackage* out_package) {
  SearchIndexIoDiagnostic diagnostic;
  if (out_package == nullptr) {
    diagnostic.error_message = "null_output_package";
    return diagnostic;
  }

  std::ifstream in(input_path, std::ios::binary);
  if (!in.good()) {
    diagnostic.error_message = "failed_to_open_index_package";
    return diagnostic;
  }

  char magic[4] = {};
  if (!ReadBytes(&in, magic, sizeof(magic)) ||
      std::memcmp(magic, "SFSI", sizeof(magic)) != 0) {
    diagnostic.error_message = "invalid_index_magic";
    return diagnostic;
  }

  FaceSearchV2IndexPackage package;
  std::uint32_t version = 0;
  std::uint32_t embedding_dim = 0;
  std::uint32_t entry_count = 0;
  if (!ReadU32(&in, &version) || version != 1u ||
      !ReadU32(&in, &embedding_dim) || embedding_dim == 0u ||
      !ReadU32(&in, &entry_count)) {
    diagnostic.error_message = "invalid_index_header";
    return diagnostic;
  }

  package.embedding_dim = static_cast<int>(embedding_dim);
  package.entries.reserve(entry_count);
  for (std::uint32_t i = 0; i < entry_count; ++i) {
    FaceSearchV2IndexRecord entry;
    std::uint32_t person_id = 0;
    std::uint32_t prototype_id = 0;
    std::uint32_t zone = 0;
    if (!ReadU32(&in, &person_id) ||
        !ReadU32(&in, &prototype_id) ||
        !ReadU32(&in, &zone) ||
        !ReadString(&in, &entry.label) ||
        !ReadFloat32(&in, &entry.prototype_weight)) {
      diagnostic.error_message = "invalid_index_record";
      return diagnostic;
    }
    entry.person_id = static_cast<int>(person_id);
    entry.prototype_id = static_cast<int>(prototype_id);
    entry.zone = static_cast<int>(zone);
    package.entries.push_back(std::move(entry));
  }

  package.normalized_matrix.resize(
      static_cast<std::size_t>(entry_count) * embedding_dim, 0.0f);
  for (float& value : package.normalized_matrix) {
    if (!ReadFloat32(&in, &value)) {
      diagnostic.error_message = "invalid_index_matrix";
      return diagnostic;
    }
  }

  if (!IsValidIndexPackage(package)) {
    diagnostic.error_message = "invalid_index_package";
    return diagnostic;
  }

  *out_package = std::move(package);
  diagnostic.ok = true;
  return diagnostic;
}

}  // namespace sentriface::search
