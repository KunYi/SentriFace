#include <cmath>
#include <cstdint>
#include <vector>

#include "camera/frame_source.hpp"
#include "embedding/face_embedder.hpp"

namespace {

bool AlmostEqual(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}

}  // namespace

int main() {
  using sentriface::embedding::EmbeddingConfig;
  using sentriface::embedding::FaceEmbedder;

  EmbeddingConfig config;
  config.embedding_dim = 4;
  FaceEmbedder embedder(config);

  if (!embedder.ValidateInputShape(112, 112, 3)) {
    return 1;
  }
  if (embedder.ValidateInputShape(224, 112, 3)) {
    return 2;
  }

  const auto invalid = embedder.Postprocess(std::vector<float> {1.0f, 2.0f});
  if (invalid.ok) {
    return 3;
  }

  const auto normalized =
      embedder.Postprocess(std::vector<float> {3.0f, 4.0f, 0.0f, 0.0f});
  if (!normalized.ok) {
    return 4;
  }
  if (!AlmostEqual(normalized.values[0], 0.6f) ||
      !AlmostEqual(normalized.values[1], 0.8f)) {
    return 5;
  }

  const float sim_same =
      embedder.CosineSimilarity(normalized.values, normalized.values);
  if (!AlmostEqual(sim_same, 1.0f)) {
    return 6;
  }

  const auto ortho_a =
      embedder.Postprocess(std::vector<float> {1.0f, 0.0f, 0.0f, 0.0f});
  const auto ortho_b =
      embedder.Postprocess(std::vector<float> {0.0f, 1.0f, 0.0f, 0.0f});
  if (!ortho_a.ok || !ortho_b.ok) {
    return 7;
  }

  const float sim_ortho =
      embedder.CosineSimilarity(ortho_a.values, ortho_b.values);
  if (!AlmostEqual(sim_ortho, 0.0f)) {
    return 8;
  }

  sentriface::camera::Frame image;
  image.width = 2;
  image.height = 2;
  image.channels = 3;
  image.pixel_format = sentriface::camera::PixelFormat::kRgb888;
  image.data = {
      255, 0, 0,    0, 255, 0,
      0, 0, 255,    255, 255, 255,
  };

  EmbeddingConfig prep_config;
  prep_config.input_width = 2;
  prep_config.input_height = 2;
  prep_config.input_channels = 3;
  prep_config.embedding_dim = 4;
  FaceEmbedder prep_embedder(prep_config);
  const auto tensor = prep_embedder.PrepareInputTensor(image);
  if (tensor.data.size() != 12U) {
    return 9;
  }
  if (!AlmostEqual(tensor.data[0], -1.0f) ||
      !AlmostEqual(tensor.data[4], -1.0f) ||
      !AlmostEqual(tensor.data[8], 1.0f)) {
    return 10;
  }

  sentriface::embedding::EmbeddingRuntimeConfig runtime_config;
  runtime_config.backend =
      sentriface::embedding::EmbeddingRuntimeBackend::kStubDeterministic;
  if (!prep_embedder.InitializeRuntime(runtime_config)) {
    return 11;
  }
  if (!prep_embedder.IsRuntimeReady()) {
    return 12;
  }
  const auto runtime_result = prep_embedder.Run(image);
  if (!runtime_result.ok || runtime_result.values.size() != 4U) {
    return 13;
  }
  const float runtime_norm = std::sqrt(
      runtime_result.values[0] * runtime_result.values[0] +
      runtime_result.values[1] * runtime_result.values[1] +
      runtime_result.values[2] * runtime_result.values[2] +
      runtime_result.values[3] * runtime_result.values[3]);
  if (!AlmostEqual(runtime_norm, 1.0f, 1e-3f)) {
    return 14;
  }

  return 0;
}
