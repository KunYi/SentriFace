#include "enroll/enrollment_store.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace sentriface::enroll {

namespace {

void NormalizeEmbedding(std::vector<float>* embedding) {
  if (embedding == nullptr || embedding->empty()) {
    return;
  }
  float norm = 0.0f;
  for (float value : *embedding) {
    norm += value * value;
  }
  norm = std::sqrt(norm);
  if (norm <= 1e-9f) {
    std::fill(embedding->begin(), embedding->end(), 0.0f);
    return;
  }
  for (float& value : *embedding) {
    value /= norm;
  }
}

bool IsValidZoneValue(int zone_value) {
  return zone_value == static_cast<int>(PrototypeZone::kBaseline) ||
         zone_value == static_cast<int>(PrototypeZone::kVerifiedHistory) ||
         zone_value == static_cast<int>(PrototypeZone::kRecentAdaptive);
}

}  // namespace

EnrollmentStore::EnrollmentStore(const EnrollmentConfig& config) : config_(config) {}

bool EnrollmentStore::UpsertPerson(int person_id, const std::string& label) {
  if (person_id < 0 || label.empty()) {
    return false;
  }

  auto it = persons_.find(person_id);
  if (it == persons_.end()) {
    PersonRecord record;
    record.person_id = person_id;
    record.label = label;
    persons_.emplace(person_id, record);
  } else {
    it->second.label = label;
  }
  return true;
}

bool EnrollmentStore::AddPrototype(int person_id, const std::vector<float>& embedding) {
  if (person_id < 0 ||
      static_cast<int>(embedding.size()) != config_.embedding_dim) {
    return false;
  }

  auto it = persons_.find(person_id);
  if (it == persons_.end()) {
    return false;
  }

  auto& prototypes = it->second.prototypes;
  if (static_cast<int>(prototypes.size()) >= config_.max_prototypes_per_person) {
    prototypes.erase(prototypes.begin());
  }
  prototypes.push_back(embedding);
  return true;
}

bool EnrollmentStore::RemovePerson(int person_id) {
  return persons_.erase(person_id) > 0U;
}

void EnrollmentStore::Clear() { persons_.clear(); }

std::size_t EnrollmentStore::PersonCount() const { return persons_.size(); }

std::size_t EnrollmentStore::PrototypeCount(int person_id) const {
  const auto it = persons_.find(person_id);
  if (it == persons_.end()) {
    return 0U;
  }
  return it->second.prototypes.size();
}

std::vector<PersonRecord> EnrollmentStore::GetPersons() const {
  std::vector<PersonRecord> out;
  out.reserve(persons_.size());
  for (const auto& entry : persons_) {
    out.push_back(entry.second);
  }
  return out;
}

std::vector<sentriface::search::FacePrototype> EnrollmentStore::ExportPrototypes() const {
  std::vector<sentriface::search::FacePrototype> out;
  for (const auto& entry : persons_) {
    const PersonRecord& person = entry.second;
    for (std::size_t i = 0; i < person.prototypes.size(); ++i) {
      sentriface::search::FacePrototype prototype;
      prototype.person_id = person.person_id;
      prototype.label = person.label + "_" + std::to_string(i);
      prototype.embedding = person.prototypes[i];
      out.push_back(prototype);
    }
  }
  return out;
}

EnrollmentStoreV2::EnrollmentStoreV2(const EnrollmentConfigV2& config)
    : config_(config) {}

bool EnrollmentStoreV2::UpsertPerson(int person_id, const std::string& label) {
  if (person_id < 0 || label.empty()) {
    return false;
  }

  auto it = persons_.find(person_id);
  if (it == persons_.end()) {
    PersonRecordV2 record;
    record.person_id = person_id;
    record.label = label;
    persons_.emplace(person_id, record);
  } else {
    it->second.label = label;
  }
  return true;
}

bool EnrollmentStoreV2::AddPrototype(int person_id,
                                     PrototypeZone zone,
                                     const std::vector<float>& embedding,
                                     const PrototypeMetadata& metadata) {
  if (person_id < 0 || static_cast<int>(embedding.size()) != config_.embedding_dim) {
    return false;
  }

  auto it = persons_.find(person_id);
  if (it == persons_.end()) {
    return false;
  }

  PersonRecordV2& person = it->second;
  if (zone == PrototypeZone::kBaseline &&
      static_cast<int>(PrototypeCount(person_id, zone)) >= ZoneCapacity(zone)) {
    return false;
  }

  ZonedPrototype prototype;
  prototype.prototype_id = next_prototype_id_++;
  prototype.zone = zone;
  prototype.embedding = embedding;
  prototype.metadata = metadata;
  const float sample_weight =
      metadata.sample_weight > 0.0f ? metadata.sample_weight : 1.0f;
  prototype.search_weight = ZoneWeight(zone) * sample_weight;
  person.prototypes.push_back(prototype);
  EnforceZoneCapacity(person, zone);
  return true;
}

bool EnrollmentStoreV2::AddRecentAdaptivePrototype(
    int person_id,
    const std::vector<float>& embedding,
    const PrototypeMetadata& metadata) {
  if (!ShouldAcceptAdaptivePrototype(metadata)) {
    return false;
  }
  return AddPrototype(person_id, PrototypeZone::kRecentAdaptive, embedding, metadata);
}

bool EnrollmentStoreV2::AddVerifiedHistoryPrototype(
    int person_id,
    const std::vector<float>& embedding,
    const PrototypeMetadata& metadata) {
  if (!ShouldAcceptVerifiedHistoryPrototype(metadata)) {
    return false;
  }
  return AddPrototype(person_id, PrototypeZone::kVerifiedHistory, embedding, metadata);
}

bool EnrollmentStoreV2::RemovePerson(int person_id) {
  return persons_.erase(person_id) > 0U;
}

void EnrollmentStoreV2::Clear() {
  persons_.clear();
  next_prototype_id_ = 0;
}

std::size_t EnrollmentStoreV2::PersonCount() const { return persons_.size(); }

std::size_t EnrollmentStoreV2::PrototypeCount(int person_id) const {
  const auto it = persons_.find(person_id);
  if (it == persons_.end()) {
    return 0U;
  }
  return it->second.prototypes.size();
}

std::size_t EnrollmentStoreV2::PrototypeCount(int person_id, PrototypeZone zone) const {
  const auto it = persons_.find(person_id);
  if (it == persons_.end()) {
    return 0U;
  }
  std::size_t count = 0U;
  for (const auto& prototype : it->second.prototypes) {
    if (prototype.zone == zone) {
      count += 1U;
    }
  }
  return count;
}

std::vector<PersonRecordV2> EnrollmentStoreV2::GetPersons() const {
  std::vector<PersonRecordV2> out;
  out.reserve(persons_.size());
  for (const auto& entry : persons_) {
    out.push_back(entry.second);
  }
  return out;
}

std::vector<sentriface::search::FacePrototypeV2> EnrollmentStoreV2::ExportWeightedPrototypes() const {
  std::vector<sentriface::search::FacePrototypeV2> out;
  for (const auto& entry : persons_) {
    const PersonRecordV2& person = entry.second;
    for (const auto& prototype : person.prototypes) {
      sentriface::search::FacePrototypeV2 exported;
      exported.person_id = person.person_id;
      exported.prototype_id = prototype.prototype_id;
      exported.zone = static_cast<int>(prototype.zone);
      exported.label = person.label;
      exported.embedding = prototype.embedding;
      exported.prototype_weight = prototype.search_weight;
      out.push_back(exported);
    }
  }
  return out;
}

bool EnrollmentStoreV2::LoadFromSearchIndexPackage(
    const sentriface::search::FaceSearchV2IndexPackage& package) {
  if (package.embedding_dim != config_.embedding_dim ||
      package.entries.empty() ||
      package.normalized_matrix.size() !=
          package.entries.size() * static_cast<std::size_t>(config_.embedding_dim)) {
    return false;
  }

  Clear();
  int next_prototype_id = 0;
  std::unordered_set<std::string> prototype_keys;
  prototype_keys.reserve(package.entries.size());
  for (std::size_t i = 0; i < package.entries.size(); ++i) {
    const auto& record = package.entries[i];
    if (record.person_id < 0 || record.prototype_id < 0 ||
        record.prototype_weight < 0.0f || record.label.empty() ||
        !IsValidZoneValue(record.zone)) {
      Clear();
      return false;
    }
    const std::string prototype_key =
        std::to_string(record.person_id) + ":" + std::to_string(record.prototype_id);
    if (!prototype_keys.insert(prototype_key).second) {
      Clear();
      return false;
    }

    auto person_it = persons_.find(record.person_id);
    if (person_it == persons_.end()) {
      PersonRecordV2 person;
      person.person_id = record.person_id;
      person.label = record.label;
      person_it = persons_.emplace(record.person_id, std::move(person)).first;
    } else if (person_it->second.label.empty()) {
      person_it->second.label = record.label;
    } else if (person_it->second.label != record.label) {
      Clear();
      return false;
    }

    ZonedPrototype prototype;
    prototype.prototype_id = record.prototype_id;
    prototype.zone = static_cast<PrototypeZone>(record.zone);
    prototype.search_weight = record.prototype_weight;
    prototype.embedding.assign(
        package.normalized_matrix.begin() +
            i * static_cast<std::size_t>(config_.embedding_dim),
        package.normalized_matrix.begin() +
            (i + 1U) * static_cast<std::size_t>(config_.embedding_dim));
    const float zone_weight = ZoneWeight(prototype.zone);
    if (zone_weight > 0.0f) {
      prototype.metadata.sample_weight = prototype.search_weight / zone_weight;
    }
    prototype.metadata.manually_enrolled =
        prototype.zone == PrototypeZone::kBaseline;
    person_it->second.prototypes.push_back(std::move(prototype));
    if (record.prototype_id >= next_prototype_id) {
      next_prototype_id = record.prototype_id + 1;
    }
  }

  next_prototype_id_ = next_prototype_id;
  return true;
}

bool EnrollmentStoreV2::LoadFromSearchIndexPackagePath(
    const std::string& input_path) {
  sentriface::search::FaceSearchV2IndexPackage package;
  const auto diagnostic =
      sentriface::search::LoadFaceSearchV2IndexPackageBinary(input_path, &package);
  return diagnostic.ok && LoadFromSearchIndexPackage(package);
}

sentriface::search::SearchIndexIoDiagnostic
EnrollmentStoreV2::BuildSearchIndexPackage(
    sentriface::search::FaceSearchV2IndexPackage* out_package) const {
  sentriface::search::SearchIndexIoDiagnostic diagnostic;
  if (out_package == nullptr) {
    diagnostic.error_message = "null_output_package";
    return diagnostic;
  }
  if (config_.embedding_dim <= 0) {
    diagnostic.error_message = "invalid_embedding_dim";
    return diagnostic;
  }

  sentriface::search::FaceSearchV2IndexPackage package;
  package.embedding_dim = config_.embedding_dim;

  std::size_t prototype_count = 0U;
  for (const auto& person_entry : persons_) {
    prototype_count += person_entry.second.prototypes.size();
  }
  if (prototype_count == 0U) {
    diagnostic.error_message = "missing_prototypes";
    return diagnostic;
  }

  package.entries.reserve(prototype_count);
  package.normalized_matrix.reserve(
      prototype_count * static_cast<std::size_t>(config_.embedding_dim));
  for (const auto& person_entry : persons_) {
    const auto& person = person_entry.second;
    if (person.person_id < 0 || person.label.empty()) {
      diagnostic.error_message = "invalid_person_record";
      return diagnostic;
    }
    for (const auto& prototype : person.prototypes) {
      if (static_cast<int>(prototype.embedding.size()) != config_.embedding_dim ||
          prototype.prototype_id < 0 || prototype.search_weight < 0.0f) {
        diagnostic.error_message = "invalid_prototype_record";
        return diagnostic;
      }

      sentriface::search::FaceSearchV2IndexRecord record;
      record.person_id = person.person_id;
      record.prototype_id = prototype.prototype_id;
      record.zone = static_cast<int>(prototype.zone);
      record.label = person.label;
      record.prototype_weight = prototype.search_weight;
      package.entries.push_back(std::move(record));

      std::vector<float> normalized = prototype.embedding;
      NormalizeEmbedding(&normalized);
      package.normalized_matrix.insert(
          package.normalized_matrix.end(),
          normalized.begin(),
          normalized.end());
    }
  }

  *out_package = std::move(package);
  diagnostic.ok = true;
  return diagnostic;
}

sentriface::search::SearchIndexIoDiagnostic
EnrollmentStoreV2::SaveSearchIndexPackageBinary(
    const std::string& output_path) const {
  sentriface::search::FaceSearchV2IndexPackage package;
  auto diagnostic = BuildSearchIndexPackage(&package);
  if (!diagnostic.ok) {
    return diagnostic;
  }
  return sentriface::search::SaveFaceSearchV2IndexPackageBinary(
      package, output_path);
}

bool EnrollmentStoreV2::ShouldAcceptAdaptivePrototype(
    const PrototypeMetadata& metadata) const {
  return DiagnoseAdaptivePrototype(metadata).accepted;
}

bool EnrollmentStoreV2::ShouldAcceptVerifiedHistoryPrototype(
    const PrototypeMetadata& metadata) const {
  return DiagnoseVerifiedHistoryPrototype(metadata).accepted;
}

PrototypeAcceptanceDiagnostic EnrollmentStoreV2::DiagnoseAdaptivePrototype(
    const PrototypeMetadata& metadata) const {
  PrototypeAcceptanceDiagnostic diagnostic;
  diagnostic.quality_ok = metadata.quality_score >= config_.min_adaptive_quality_score;
  diagnostic.decision_score_ok =
      metadata.decision_score >= config_.min_adaptive_decision_score;
  diagnostic.margin_ok = metadata.top1_top2_margin >= config_.min_adaptive_margin;
  diagnostic.liveness_ok =
      !config_.require_adaptive_liveness || metadata.liveness_ok;
  diagnostic.accepted = diagnostic.quality_ok &&
                        diagnostic.decision_score_ok &&
                        diagnostic.margin_ok &&
                        diagnostic.liveness_ok;
  if (diagnostic.accepted) {
    diagnostic.rejection_reason = "accepted";
  } else if (!diagnostic.quality_ok) {
    diagnostic.rejection_reason = "quality_below_threshold";
  } else if (!diagnostic.decision_score_ok) {
    diagnostic.rejection_reason = "decision_score_below_threshold";
  } else if (!diagnostic.margin_ok) {
    diagnostic.rejection_reason = "margin_below_threshold";
  } else if (!diagnostic.liveness_ok) {
    diagnostic.rejection_reason = "liveness_not_satisfied";
  }
  return diagnostic;
}

PrototypeAcceptanceDiagnostic EnrollmentStoreV2::DiagnoseVerifiedHistoryPrototype(
    const PrototypeMetadata& metadata) const {
  PrototypeAcceptanceDiagnostic diagnostic;
  diagnostic.quality_ok = metadata.quality_score >= config_.min_verified_quality_score;
  diagnostic.decision_score_ok =
      metadata.decision_score >= config_.min_verified_decision_score;
  diagnostic.margin_ok = metadata.top1_top2_margin >= config_.min_verified_margin;
  diagnostic.liveness_ok =
      !config_.require_verified_liveness || metadata.liveness_ok;
  diagnostic.accepted = diagnostic.quality_ok &&
                        diagnostic.decision_score_ok &&
                        diagnostic.margin_ok &&
                        diagnostic.liveness_ok;
  if (diagnostic.accepted) {
    diagnostic.rejection_reason = "accepted";
  } else if (!diagnostic.quality_ok) {
    diagnostic.rejection_reason = "quality_below_threshold";
  } else if (!diagnostic.decision_score_ok) {
    diagnostic.rejection_reason = "decision_score_below_threshold";
  } else if (!diagnostic.margin_ok) {
    diagnostic.rejection_reason = "margin_below_threshold";
  } else if (!diagnostic.liveness_ok) {
    diagnostic.rejection_reason = "liveness_not_satisfied";
  }
  return diagnostic;
}

int EnrollmentStoreV2::ZoneCapacity(PrototypeZone zone) const {
  switch (zone) {
    case PrototypeZone::kBaseline:
      return config_.max_baseline_prototypes;
    case PrototypeZone::kVerifiedHistory:
      return config_.max_verified_history_prototypes;
    case PrototypeZone::kRecentAdaptive:
      return config_.max_recent_adaptive_prototypes;
  }
  return 0;
}

float EnrollmentStoreV2::ZoneWeight(PrototypeZone zone) const {
  switch (zone) {
    case PrototypeZone::kBaseline:
      return config_.baseline_weight;
    case PrototypeZone::kVerifiedHistory:
      return config_.verified_history_weight;
    case PrototypeZone::kRecentAdaptive:
      return config_.recent_adaptive_weight;
  }
  return 1.0f;
}

void EnrollmentStoreV2::EnforceZoneCapacity(PersonRecordV2& person, PrototypeZone zone) {
  const int capacity = ZoneCapacity(zone);
  if (capacity <= 0) {
    return;
  }

  int zone_count = 0;
  for (const auto& prototype : person.prototypes) {
    if (prototype.zone == zone) {
      zone_count += 1;
    }
  }

  while (zone_count > capacity) {
    auto it = std::find_if(person.prototypes.begin(), person.prototypes.end(),
                           [zone](const ZonedPrototype& prototype) {
                             return prototype.zone == zone;
                           });
    if (it == person.prototypes.end()) {
      break;
    }
    person.prototypes.erase(it);
    zone_count -= 1;
  }
}

}  // namespace sentriface::enroll
