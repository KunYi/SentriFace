#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "enroll/enrollment_artifact_import.hpp"

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Expectation failed: " << message << std::endl;
    std::exit(1);
  }
}

std::filesystem::path WriteSampleArtifactSummary() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "sentriface_enrollment_artifact_test";
  std::filesystem::create_directories(dir);
  const std::filesystem::path summary_path = dir / "enrollment_summary.txt";
  std::ofstream out(summary_path);
  out
      << "user_id=E2001\n"
      << "user_name=Test_User\n"
      << "source=mock_preview_source:running:1280x720\n"
      << "observation=disabled\n"
      << "capture_plan=frontal:ok | left_quarter:ok | right_quarter:ok\n"
      << "captured_samples=1\n"
      << "\n"
      << "[sample]\n"
      << "index=1\n"
      << "slot=frontal\n"
      << "elapsed_ms=1234\n"
      << "has_face=1\n"
      << "bbox_x=210.00\n"
      << "bbox_y=120.00\n"
      << "bbox_w=220.00\n"
      << "bbox_h=300.00\n"
      << "yaw_deg=0.00\n"
      << "pitch_deg=1.50\n"
      << "roll_deg=-1.00\n"
      << "quality_score=0.91\n"
      << "sample_weight=1.37\n"
      << "preview_width=1280\n"
      << "preview_height=720\n"
      << "frame_image=sample_01_frontal_frame.bmp\n"
      << "face_crop=sample_01_frontal_face.bmp\n";
  return summary_path;
}

}  // namespace

int main() {
  const std::filesystem::path summary_path = WriteSampleArtifactSummary();

  sentriface::enroll::EnrollmentArtifactPackage package;
  const auto load_result = sentriface::enroll::LoadEnrollmentArtifactPackage(
      summary_path.string(), &package);
  Expect(load_result.ok, "artifact summary should load");
  Expect(package.user_id == "E2001", "user_id should parse");
  Expect(package.user_name == "Test_User", "user_name should parse");
  Expect(package.samples.size() == 1U, "sample count should parse");
  Expect(package.samples[0].slot == "frontal", "slot should parse");
  Expect(package.samples[0].elapsed_ms == 1234U, "elapsed_ms should parse");
  Expect(package.samples[0].preview_width == 1280, "preview_width should parse");
  Expect(package.samples[0].frame_image_path.find("sample_01_frontal_frame.bmp") !=
             std::string::npos,
         "frame image path should resolve");
  Expect(package.samples[0].face_crop_path.find("sample_01_frontal_face.bmp") !=
             std::string::npos,
         "face crop path should resolve");
  Expect(package.samples[0].sample_weight > 1.3f,
         "sample weight should parse");

  sentriface::enroll::EnrollmentStoreV2 store;
  const auto import_result =
      sentriface::enroll::ApplyArtifactIdentityToStoreV2(package, 7, &store);
  Expect(import_result.ok, "identity import into store should succeed");
  Expect(store.PersonCount() == 1U, "store should contain one person");
  const auto persons = store.GetPersons();
  Expect(!persons.empty(), "persons should be available");
  Expect(persons.front().person_id == 7, "person_id should match import target");
  Expect(persons.front().label == "Test_User", "label should come from artifact");

  sentriface::enroll::BaselineEnrollmentPlan plan;
  const auto plan_result =
      sentriface::enroll::BuildBaselineEnrollmentPlan(package, 2, &plan);
  Expect(plan_result.ok, "baseline plan should build");
  Expect(plan.user_id == "E2001", "plan user_id should match");
  Expect(plan.samples.size() == 1U, "plan should contain one usable sample");
  Expect(plan.samples[0].preferred_image_path.find("sample_01_frontal_face.bmp") !=
             std::string::npos,
         "plan should prefer face crop path");
  Expect(plan.samples[0].frame_image_path.find("sample_01_frontal_frame.bmp") !=
             std::string::npos,
         "plan should preserve frame image path");
  Expect(plan.samples[0].sample_weight > 1.3f,
         "plan should preserve sample weight");

  std::filesystem::remove_all(summary_path.parent_path());
  return 0;
}
