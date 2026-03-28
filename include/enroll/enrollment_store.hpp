#ifndef SENTRIFACE_ENROLL_ENROLLMENT_STORE_HPP_
#define SENTRIFACE_ENROLL_ENROLLMENT_STORE_HPP_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "search/face_search.hpp"

namespace sentriface::enroll {

struct EnrollmentConfig {
  int embedding_dim = 512;
  int max_prototypes_per_person = 5;
};

enum class PrototypeZone : std::uint8_t {
  kBaseline = 0,
  kVerifiedHistory = 1,
  kRecentAdaptive = 2,
};

struct PrototypeMetadata {
  std::uint32_t timestamp_ms = 0;
  float quality_score = 0.0f;
  float decision_score = 0.0f;
  float top1_top2_margin = 0.0f;
  float sample_weight = 1.0f;
  bool liveness_ok = false;
  bool manually_enrolled = false;
};

struct PrototypeAcceptanceDiagnostic {
  bool accepted = false;
  bool quality_ok = false;
  bool decision_score_ok = false;
  bool margin_ok = false;
  bool liveness_ok = false;
  std::string rejection_reason;
};

struct EnrollmentConfigV2 {
  int embedding_dim = 512;
  int max_baseline_prototypes = 3;
  int max_verified_history_prototypes = 5;
  int max_recent_adaptive_prototypes = 4;
  float baseline_weight = 1.0f;
  float verified_history_weight = 0.8f;
  float recent_adaptive_weight = 0.6f;
  float min_adaptive_quality_score = 0.80f;
  float min_adaptive_decision_score = 0.85f;
  float min_adaptive_margin = 0.05f;
  bool require_adaptive_liveness = true;
  float min_verified_quality_score = 0.85f;
  float min_verified_decision_score = 0.90f;
  float min_verified_margin = 0.08f;
  bool require_verified_liveness = true;
};

struct PersonRecord {
  int person_id = -1;
  std::string label;
  std::vector<std::vector<float>> prototypes;
};

struct ZonedPrototype {
  int prototype_id = -1;
  PrototypeZone zone = PrototypeZone::kBaseline;
  std::vector<float> embedding;
  PrototypeMetadata metadata;
  float search_weight = 1.0f;
};

struct PersonRecordV2 {
  int person_id = -1;
  std::string label;
  std::vector<ZonedPrototype> prototypes;
};

class EnrollmentStore {
 public:
  explicit EnrollmentStore(const EnrollmentConfig& config = EnrollmentConfig {});

  bool UpsertPerson(int person_id, const std::string& label);
  bool AddPrototype(int person_id, const std::vector<float>& embedding);
  bool RemovePerson(int person_id);
  void Clear();

  std::size_t PersonCount() const;
  std::size_t PrototypeCount(int person_id) const;
  std::vector<PersonRecord> GetPersons() const;
  std::vector<sentriface::search::FacePrototype> ExportPrototypes() const;

 private:
  EnrollmentConfig config_;
  std::unordered_map<int, PersonRecord> persons_;
};

class EnrollmentStoreV2 {
 public:
  explicit EnrollmentStoreV2(
      const EnrollmentConfigV2& config = EnrollmentConfigV2 {});

  bool UpsertPerson(int person_id, const std::string& label);
  bool AddPrototype(int person_id,
                    PrototypeZone zone,
                    const std::vector<float>& embedding,
                    const PrototypeMetadata& metadata);
  bool AddRecentAdaptivePrototype(int person_id,
                                  const std::vector<float>& embedding,
                                  const PrototypeMetadata& metadata);
  bool AddVerifiedHistoryPrototype(int person_id,
                                   const std::vector<float>& embedding,
                                   const PrototypeMetadata& metadata);
  bool RemovePerson(int person_id);
  void Clear();

  std::size_t PersonCount() const;
  std::size_t PrototypeCount(int person_id) const;
  std::size_t PrototypeCount(int person_id, PrototypeZone zone) const;
  std::vector<PersonRecordV2> GetPersons() const;
  std::vector<sentriface::search::FacePrototypeV2> ExportWeightedPrototypes() const;
  bool ShouldAcceptAdaptivePrototype(const PrototypeMetadata& metadata) const;
  bool ShouldAcceptVerifiedHistoryPrototype(const PrototypeMetadata& metadata) const;
  PrototypeAcceptanceDiagnostic DiagnoseAdaptivePrototype(
      const PrototypeMetadata& metadata) const;
  PrototypeAcceptanceDiagnostic DiagnoseVerifiedHistoryPrototype(
      const PrototypeMetadata& metadata) const;

 private:
  int ZoneCapacity(PrototypeZone zone) const;
  float ZoneWeight(PrototypeZone zone) const;
  void EnforceZoneCapacity(PersonRecordV2& person, PrototypeZone zone);

  EnrollmentConfigV2 config_;
  int next_prototype_id_ = 0;
  std::unordered_map<int, PersonRecordV2> persons_;
};

}  // namespace sentriface::enroll

#endif  // SENTRIFACE_ENROLL_ENROLLMENT_STORE_HPP_
