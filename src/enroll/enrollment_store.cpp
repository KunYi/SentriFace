#include "enroll/enrollment_store.hpp"

#include <algorithm>

namespace sentriface::enroll {

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
