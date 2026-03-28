#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "camera/frame_source.hpp"
#include "detector/face_detector.hpp"

namespace {

void PrintUsage() {
  std::cerr
      << "Usage: scrfd_ppm_runner <model_path> <input_ppm> <output_csv> "
      << "[score_threshold] [nms_threshold] [max_num]\n";
}

bool ReadPpmToken(std::istream& input, std::string& token) {
  token.clear();
  while (input >> token) {
    if (!token.empty() && token[0] == '#') {
      input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      continue;
    }
    return true;
  }
  return false;
}

bool LoadPpmImage(const std::string& path, sentriface::camera::Frame& frame) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return false;
  }

  std::string magic;
  if (!ReadPpmToken(input, magic) || magic != "P6") {
    return false;
  }

  std::string width_token;
  std::string height_token;
  std::string max_value_token;
  if (!ReadPpmToken(input, width_token) ||
      !ReadPpmToken(input, height_token) ||
      !ReadPpmToken(input, max_value_token)) {
    return false;
  }

  const int width = std::stoi(width_token);
  const int height = std::stoi(height_token);
  const int max_value = std::stoi(max_value_token);
  if (width <= 0 || height <= 0 || max_value != 255) {
    return false;
  }

  input.get();

  frame.width = width;
  frame.height = height;
  frame.channels = 3;
  frame.pixel_format = sentriface::camera::PixelFormat::kRgb888;
  frame.timestamp_ms = static_cast<std::uint64_t>(0);
  frame.data.resize(static_cast<std::size_t>(width * height * 3));
  input.read(reinterpret_cast<char*>(frame.data.data()),
             static_cast<std::streamsize>(frame.data.size()));
  return input.good() || input.gcount() == static_cast<std::streamsize>(frame.data.size());
}

bool WriteCsv(const std::string& path,
              const std::string& image_path,
              const sentriface::detector::DetectionBatch& batch) {
  std::ofstream output(path);
  if (!output.is_open()) {
    return false;
  }

  output << "frame_index,image_path,x,y,w,h,score,"
         << "l0x,l0y,l1x,l1y,l2x,l2y,l3x,l3y,l4x,l4y\n";
  for (const auto& detection : batch.detections) {
    output << 0 << ','
           << image_path << ','
           << detection.bbox.x << ','
           << detection.bbox.y << ','
           << detection.bbox.w << ','
           << detection.bbox.h << ','
           << detection.score;
    for (const auto& point : detection.landmarks.points) {
      output << ',' << point.x << ',' << point.y;
    }
    output << '\n';
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4 || argc > 7) {
    PrintUsage();
    return 1;
  }

#ifndef SENTRIFACE_HAS_ONNXRUNTIME
  std::cerr << "scrfd_ppm_runner requires SENTRIFACE_ENABLE_ONNXRUNTIME=ON build\n";
  return 2;
#else
  const std::string model_path = argv[1];
  const std::string input_path = argv[2];
  const std::string output_path = argv[3];
  const float score_threshold = argc >= 5 ? std::stof(argv[4]) : 0.02f;
  const float nms_threshold = argc >= 6 ? std::stof(argv[5]) : 0.40f;
  const int max_num = argc >= 7 ? std::stoi(argv[6]) : 5;

  sentriface::camera::Frame frame;
  if (!LoadPpmImage(input_path, frame)) {
    std::cerr << "Failed to load PPM image: " << input_path << '\n';
    return 3;
  }

  sentriface::detector::ScrfdDetectorConfig config;
  config.runtime = sentriface::detector::ScrfdRuntime::kCpu;
  config.model_path = model_path;
  config.detection.input_width = frame.width;
  config.detection.input_height = frame.height;
  config.detection.input_channels = frame.channels;
  config.detection.score_threshold = score_threshold;
  config.nms_threshold = nms_threshold;
  config.max_detections = max_num;

  sentriface::detector::ScrfdFaceDetector detector(config, "scrfd_ppm_runner");
  if (!detector.IsInitialized() || !detector.HasRuntime()) {
    std::cerr << "Failed to initialize SCRFD detector runtime\n";
    return 4;
  }

  const auto batch = detector.Detect(frame);
  if (!WriteCsv(output_path, input_path, batch)) {
    std::cerr << "Failed to write CSV: " << output_path << '\n';
    return 5;
  }

  float max_score = 0.0f;
  for (const auto& detection : batch.detections) {
    if (detection.score > max_score) {
      max_score = detection.score;
    }
  }

  std::cout << "detections=" << batch.detections.size() << '\n';
  std::cout << "max_score=" << max_score << '\n';
  std::cout << "output_csv=" << output_path << '\n';
  return 0;
#endif
}
