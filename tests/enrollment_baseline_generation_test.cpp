#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "enroll/enrollment_baseline_generation.hpp"

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Expectation failed: " << message << std::endl;
    std::exit(1);
  }
}

}  // namespace

int main() {
  const std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() / "sentriface_baseline_generation_test";
  std::filesystem::create_directories(temp_dir);
  const std::filesystem::path sample_a_path = temp_dir / "sample_01_face.bmp";
  const std::filesystem::path sample_b_path = temp_dir / "sample_02_face.bmp";
  {
    std::ofstream out(sample_a_path, std::ios::binary);
    out << "fake_bmp_a";
  }
  {
    std::ofstream out(sample_b_path, std::ios::binary);
    out << "fake_bmp_b";
  }

  sentriface::enroll::BaselineEnrollmentPlan plan;
  plan.user_id = "E3001";
  plan.user_name = "Baseline_User";

  sentriface::enroll::BaselineEnrollmentSample sample_a;
  sample_a.sample_index = 1;
  sample_a.slot = "frontal";
  sample_a.preferred_image_path = sample_a_path.string();
  sample_a.frame_image_path = "artifacts/sample_01_frame.bmp";
  sample_a.face_crop_path = sample_a_path.string();
  sample_a.quality_score = 0.95f;
  sample_a.sample_weight = 1.45f;
  plan.samples.push_back(sample_a);

  sentriface::enroll::BaselineEnrollmentSample sample_b;
  sample_b.sample_index = 2;
  sample_b.slot = "left_quarter";
  sample_b.preferred_image_path = sample_b_path.string();
  sample_b.frame_image_path = "artifacts/sample_02_frame.bmp";
  sample_b.face_crop_path = sample_b_path.string();
  sample_b.quality_score = 0.90f;
  sample_b.sample_weight = 1.12f;
  plan.samples.push_back(sample_b);

  sentriface::enroll::BaselineGenerationConfig config;
  config.backend =
      sentriface::enroll::BaselineGenerationBackend::kMockDeterministic;
  config.embedding_dim = 8;
  config.max_samples = 2;

  sentriface::enroll::BaselinePrototypePackage package;
  const auto generate_result =
      sentriface::enroll::GenerateBaselinePrototypePackage(plan, config, &package);
  Expect(generate_result.ok, "baseline prototype package should generate");
  Expect(package.user_id == "E3001", "user_id should propagate");
  Expect(package.prototypes.size() == 2U, "two prototypes should be generated");
  Expect(package.prototypes[0].embedding.size() == 8U,
         "embedding size should match config");
  Expect(package.prototypes[0].metadata.manually_enrolled,
         "baseline metadata should be manually enrolled");
  Expect(package.prototypes[0].metadata.sample_weight > 1.4f,
         "sample weight should propagate into metadata");
  Expect(package.prototypes[0].source_image_path == sample_a.preferred_image_path,
         "preferred image path should propagate");
  Expect(!package.prototypes[0].source_image_digest.empty(),
         "image digest should be populated");
  Expect(package.prototypes[0].embedding != package.prototypes[1].embedding,
         "different source images should produce different embeddings");

  sentriface::enroll::BaselinePrototypePackage package_repeat;
  const auto repeat_result =
      sentriface::enroll::GenerateBaselinePrototypePackage(
          plan, config, &package_repeat);
  Expect(repeat_result.ok, "repeat generation should succeed");
  Expect(package.prototypes[0].embedding == package_repeat.prototypes[0].embedding,
         "generation should be deterministic");

  sentriface::enroll::BaselineEmbeddingInputManifest manifest;
  const auto manifest_result =
      sentriface::enroll::BuildBaselineEmbeddingInputManifest(plan, &manifest);
  Expect(manifest_result.ok, "embedding input manifest should build");
  Expect(manifest.records.size() == 2U, "manifest should preserve sample count");
  Expect(manifest.records[0].image_path == sample_a.preferred_image_path,
         "manifest should use preferred image path");
  Expect(manifest.records[0].sample_weight > 1.4f,
         "manifest should preserve sample weight");

  sentriface::enroll::EnrollmentStoreV2 store(
      sentriface::enroll::EnrollmentConfigV2 {.embedding_dim = 8});
  const auto apply_result =
      sentriface::enroll::ApplyBaselinePrototypePackageToStoreV2(package, 11, &store);
  Expect(apply_result.ok, "baseline package should apply to store");
  Expect(store.PersonCount() == 1U, "store should contain one person");
  Expect(store.PrototypeCount(11, sentriface::enroll::PrototypeZone::kBaseline) == 2U,
         "baseline prototypes should be stored");

  std::filesystem::remove_all(temp_dir);

  return 0;
}
