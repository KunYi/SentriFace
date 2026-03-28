#include <vector>

#include "enroll/enrollment_store.hpp"

int main() {
  using sentriface::enroll::EnrollmentConfig;
  using sentriface::enroll::EnrollmentStore;

  EnrollmentConfig config;
  config.embedding_dim = 4;
  config.max_prototypes_per_person = 2;

  EnrollmentStore store(config);

  if (!store.UpsertPerson(1, "alice")) {
    return 1;
  }
  if (!store.UpsertPerson(2, "bob")) {
    return 2;
  }
  if (store.PersonCount() != 2U) {
    return 3;
  }

  if (!store.AddPrototype(1, std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f})) {
    return 4;
  }
  if (!store.AddPrototype(1, std::vector<float> {0.9f, 0.1f, 0.0f, 0.0f})) {
    return 5;
  }
  if (!store.AddPrototype(1, std::vector<float> {0.8f, 0.2f, 0.0f, 0.0f})) {
    return 6;
  }
  if (store.PrototypeCount(1) != 2U) {
    return 7;
  }

  if (!store.AddPrototype(2, std::vector<float> {0.0f, 1.0f, 0.0f, 0.0f})) {
    return 8;
  }

  const auto exported = store.ExportPrototypes();
  if (exported.size() != 3U) {
    return 9;
  }

  if (!store.RemovePerson(2)) {
    return 10;
  }
  if (store.PersonCount() != 1U) {
    return 11;
  }

  store.Clear();
  if (store.PersonCount() != 0U) {
    return 12;
  }

  return 0;
}
