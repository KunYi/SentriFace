#include <cmath>
#include <vector>

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

  return 0;
}
