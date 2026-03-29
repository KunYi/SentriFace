#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "enroll/enrollment_baseline_generation.hpp"
#include "search/face_search.hpp"

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
  sentriface::enroll::BaselineGenerationConfig invalid_runtime_config = config;
  invalid_runtime_config.backend =
      sentriface::enroll::BaselineGenerationBackend::kOnnxRuntime;
  sentriface::enroll::BaselinePrototypePackage invalid_runtime_package;
  const auto invalid_runtime_result =
      sentriface::enroll::GenerateBaselinePrototypePackage(
          plan, invalid_runtime_config, &invalid_runtime_package);
  Expect(!invalid_runtime_result.ok &&
             invalid_runtime_result.error_message == "missing_model_path",
         "onnxruntime backend should require model path");
  const std::filesystem::path csv_path = temp_dir / "baseline_embedding_output.csv";
  Expect(sentriface::enroll::MakeBaselinePrototypePackagePath(csv_path.string()) ==
             (temp_dir / "baseline_embedding_output.sfbp").string(),
         "baseline package path helper should replace extension with .sfbp");
  Expect(sentriface::enroll::MakeFaceSearchV2IndexPath(csv_path.string()) ==
             (temp_dir / "baseline_embedding_output.sfsi").string(),
         "search index path helper should replace extension with .sfsi");
  Expect(sentriface::enroll::InferBaselinePrototypePackageEmbeddingDim(package) == 8,
         "embedding dim helper should infer from package");
  sentriface::enroll::BaselinePrototypePackage invalid_dim_package = package;
  invalid_dim_package.prototypes[1].embedding.resize(7);
  Expect(sentriface::enroll::InferBaselinePrototypePackageEmbeddingDim(
             invalid_dim_package) == 0,
         "embedding dim helper should reject inconsistent package dims");

  const std::filesystem::path binary_path = temp_dir / "baseline_embedding_output.sfbp";
  const auto save_binary_result =
      sentriface::enroll::SaveBaselinePrototypePackageBinary(package,
                                                             binary_path.string());
  Expect(save_binary_result.ok, "binary package should save");
  Expect(std::filesystem::exists(binary_path), "binary package should exist");

  sentriface::enroll::BaselinePrototypePackage loaded_binary_package;
  const auto load_binary_result =
      sentriface::enroll::LoadBaselinePrototypePackageBinary(
          binary_path.string(), &loaded_binary_package);
  Expect(load_binary_result.ok, "binary package should load");
  Expect(loaded_binary_package.user_id == package.user_id,
         "binary round-trip should preserve user_id");
  Expect(loaded_binary_package.user_name == package.user_name,
         "binary round-trip should preserve user_name");
  Expect(loaded_binary_package.prototypes.size() == package.prototypes.size(),
         "binary round-trip should preserve prototype count");
  Expect(loaded_binary_package.prototypes[0].embedding ==
             package.prototypes[0].embedding,
         "binary round-trip should preserve embeddings");

  sentriface::enroll::BaselineEmbeddingCsvImportConfig auto_load_config;
  auto_load_config.embedding_dim = 8;
  sentriface::enroll::BaselinePrototypePackage loaded_auto_package;
  const auto load_auto_result = sentriface::enroll::LoadBaselinePrototypePackage(
      binary_path.string(), auto_load_config, &loaded_auto_package);
  Expect(load_auto_result.ok, "auto-load should dispatch binary package");
  Expect(loaded_auto_package.prototypes[1].slot == package.prototypes[1].slot,
         "auto-load should preserve record fields");

  sentriface::search::FaceSearchV2IndexPackage search_index_package;
  const auto build_index_result =
      sentriface::enroll::BuildFaceSearchV2IndexPackageFromBaselinePrototypePackage(
          package, 11, 8, 1.0f, &search_index_package);
  Expect(build_index_result.ok, "search index package should build from baseline package");
  Expect(search_index_package.entries.size() == package.prototypes.size(),
         "search index package should preserve prototype count");

  sentriface::search::SearchConfig search_config;
  search_config.embedding_dim = 8;
  search_config.top_k = 2;
  sentriface::search::FaceSearchV2 search(search_config);
  Expect(search.LoadFromIndexPackage(search_index_package),
         "search should load from generated index package");
  const auto search_result = search.Query(package.prototypes[0].embedding);
  Expect(search_result.ok && !search_result.hits.empty(),
         "generated search index should answer query");
  Expect(search_result.hits[0].person_id == 11,
         "generated search index should preserve person_id");

  const std::filesystem::path artifacts_binary_path =
      temp_dir / "baseline_artifacts_output.sfbp";
  const std::filesystem::path artifacts_index_path =
      temp_dir / "baseline_artifacts_output.sfsi";
  const auto save_artifacts_result =
      sentriface::enroll::SaveBaselinePackageArtifacts(
          package, 11, 8, 1.0f, artifacts_binary_path.string(),
          artifacts_index_path.string());
  Expect(save_artifacts_result.ok, "artifact helper should save both packages");
  Expect(std::filesystem::exists(artifacts_binary_path),
         "artifact helper should write baseline package");
  Expect(std::filesystem::exists(artifacts_index_path),
         "artifact helper should write search index package");

  sentriface::search::FaceSearchV2IndexPackage loaded_index_package;
  std::string index_input;
  const auto load_or_build_result =
      sentriface::enroll::LoadOrBuildFaceSearchV2IndexPackage(
          artifacts_binary_path.string(), package, 11, 8, 1.0f,
          &loaded_index_package, &index_input);
  Expect(load_or_build_result.ok, "load/build helper should succeed");
  Expect(index_input == artifacts_index_path.string(),
         "load/build helper should prefer sibling search index package");
  Expect(loaded_index_package.entries.size() == package.prototypes.size(),
         "load/build helper should preserve entry count");

  const std::filesystem::path rebuild_only_binary_path =
      temp_dir / "baseline_rebuild_only.sfbp";
  const auto save_rebuild_binary_result =
      sentriface::enroll::SaveBaselinePrototypePackageBinary(
          package, rebuild_only_binary_path.string());
  Expect(save_rebuild_binary_result.ok,
         "fallback test binary package should save");
  sentriface::search::FaceSearchV2IndexPackage rebuilt_index_package;
  std::string rebuilt_index_input;
  const auto load_or_build_fallback_result =
      sentriface::enroll::LoadOrBuildFaceSearchV2IndexPackage(
          rebuild_only_binary_path.string(), package, 11, 8, 1.0f,
          &rebuilt_index_package, &rebuilt_index_input);
  Expect(load_or_build_fallback_result.ok,
         "load/build helper should rebuild when sibling index is missing");
  Expect(rebuilt_index_input == "package_rebuild",
         "load/build helper should report package rebuild fallback");
  Expect(rebuilt_index_package.entries.size() == package.prototypes.size(),
         "rebuild fallback should preserve entry count");
  Expect(std::filesystem::exists(
             sentriface::enroll::MakeFaceSearchV2IndexPath(
                 rebuild_only_binary_path.string())),
         "rebuild fallback should persist sibling search index package");

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

  sentriface::enroll::EnrollmentStoreV2 loaded_store(
      sentriface::enroll::EnrollmentConfigV2 {.embedding_dim = 8});
  sentriface::enroll::BaselineEmbeddingCsvImportConfig load_store_config;
  load_store_config.embedding_dim = 8;
  const auto load_store_result =
      sentriface::enroll::LoadBaselinePrototypePackageIntoStoreV2(
          binary_path.string(), load_store_config, 12, &loaded_store);
  Expect(load_store_result.ok, "baseline package path should import into store");
  Expect(loaded_store.PersonCount() == 1U,
         "baseline package path import should create one person");
  Expect(
      loaded_store.PrototypeCount(12, sentriface::enroll::PrototypeZone::kBaseline) ==
          2U,
      "baseline package path import should preserve baseline prototypes");

  std::filesystem::remove_all(temp_dir);

  return 0;
}
