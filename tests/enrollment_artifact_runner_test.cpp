#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifndef SENTRIFACE_ENROLLMENT_ARTIFACT_RUNNER_PATH
#define SENTRIFACE_ENROLLMENT_ARTIFACT_RUNNER_PATH "enrollment_artifact_runner"
#endif

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Expectation failed: " << message << std::endl;
    std::exit(1);
  }
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  const std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() / "sentriface_enrollment_artifact_runner_test";
  std::filesystem::remove_all(temp_dir);
  std::filesystem::create_directories(temp_dir);

  const std::filesystem::path frame_path = temp_dir / "sample_01_frame.bmp";
  const std::filesystem::path face_path = temp_dir / "sample_01_face.bmp";
  const std::filesystem::path summary_path = temp_dir / "enrollment_summary.txt";
  const std::filesystem::path output_path = temp_dir / "baseline_artifact_summary.txt";
  const std::filesystem::path baseline_package_path = temp_dir / "baseline_artifact_summary.sfbp";
  const std::filesystem::path search_index_path = temp_dir / "baseline_artifact_summary.sfsi";
  const std::filesystem::path manifest_path =
      temp_dir / "baseline_artifact_summary_embedding_input_manifest.txt";

  {
    std::ofstream frame(frame_path, std::ios::binary);
    frame << "fake_frame_bmp";
  }
  {
    std::ofstream face(face_path, std::ios::binary);
    face << "fake_face_bmp";
  }
  {
    std::ofstream summary(summary_path);
    summary << "user_id=E9001\n";
    summary << "user_name=Runner_User\n";
    summary << "source=mock\n";
    summary << "observation=none\n";
    summary << "capture_plan=standard_v1\n";
    summary << "\n[sample]\n";
    summary << "index=1\n";
    summary << "slot=frontal\n";
    summary << "elapsed_ms=1200\n";
    summary << "has_face=1\n";
    summary << "bbox_x=10\n";
    summary << "bbox_y=20\n";
    summary << "bbox_w=220\n";
    summary << "bbox_h=300\n";
    summary << "yaw_deg=0\n";
    summary << "pitch_deg=0\n";
    summary << "roll_deg=0\n";
    summary << "quality_score=0.95\n";
    summary << "preview_width=1280\n";
    summary << "preview_height=720\n";
    summary << "frame_image=" << frame_path.filename().string() << "\n";
    summary << "face_crop=" << face_path.filename().string() << "\n";
  }

  const std::string command =
      std::string("\"") +
      std::string(SENTRIFACE_ENROLLMENT_ARTIFACT_RUNNER_PATH) +
      "\" \"" + summary_path.string() + "\" 77 \"" + output_path.string() +
      "\" 1 16";
  const int rc = std::system(command.c_str());
  Expect(rc == 0, "runner command should succeed");
  Expect(std::filesystem::exists(output_path), "runner should produce summary file");

  const std::string text = ReadTextFile(output_path);
  Expect(text.find("user_id=E9001") != std::string::npos,
         "summary should include user_id");
  Expect(text.find("person_id=77") != std::string::npos,
         "summary should include person_id");
  Expect(text.find("baseline_prototypes=1") != std::string::npos,
         "summary should include baseline count");
  Expect(text.find("baseline_backend=mock_deterministic") != std::string::npos,
         "summary should include backend");
  Expect(text.find("baseline_binary_package=" + baseline_package_path.string()) !=
             std::string::npos,
         "summary should include baseline package path");
  Expect(text.find("search_index_package=" + search_index_path.string()) !=
             std::string::npos,
         "summary should include search index path");
  Expect(text.find("embedding_dim=16") != std::string::npos,
         "summary should include embedding dim");
  Expect(text.find("source_image_digest=") != std::string::npos,
         "summary should include digest");
  Expect(std::filesystem::exists(baseline_package_path),
         "runner should produce baseline binary package");
  Expect(std::filesystem::exists(search_index_path),
         "runner should produce search index package");
  Expect(std::filesystem::exists(manifest_path),
         "runner should produce embedding input manifest");
  const std::string manifest_text = ReadTextFile(manifest_path);
  Expect(manifest_text.find("image_path=" + face_path.string()) != std::string::npos,
         "manifest should include preferred image path");

  const std::filesystem::path default_output_path =
      temp_dir / "baseline_artifact_summary.txt";
  const std::filesystem::path default_package_path =
      temp_dir / "baseline_artifact_summary.sfbp";
  const std::filesystem::path default_index_path =
      temp_dir / "baseline_artifact_summary.sfsi";
  const std::filesystem::path default_manifest_path =
      temp_dir / "baseline_artifact_summary_embedding_input_manifest.txt";
  std::filesystem::remove(default_output_path);
  std::filesystem::remove(default_package_path);
  std::filesystem::remove(default_index_path);
  std::filesystem::remove(default_manifest_path);
  const std::string default_command =
      std::string("\"") +
      std::string(SENTRIFACE_ENROLLMENT_ARTIFACT_RUNNER_PATH) +
      "\" \"" + summary_path.string() + "\" 77";
  const int default_rc = std::system(default_command.c_str());
  Expect(default_rc == 0, "runner default output path should succeed");
  Expect(std::filesystem::exists(default_output_path),
         "runner should use baseline_artifact_summary.txt by default");
  Expect(std::filesystem::exists(default_package_path),
         "runner default path should also emit .sfbp");
  Expect(std::filesystem::exists(default_index_path),
         "runner default path should also emit .sfsi");
  Expect(std::filesystem::exists(default_manifest_path),
         "runner default path should also emit embedding manifest");

  std::filesystem::remove_all(temp_dir);
  return 0;
}
