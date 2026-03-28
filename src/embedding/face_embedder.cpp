#include "embedding/face_embedder.hpp"

#include <cmath>
#include <numeric>

namespace sentriface::embedding {
namespace {

float L2Norm(const std::vector<float>& values) {
  float sum = 0.0f;
  for (float v : values) {
    sum += v * v;
  }
  return std::sqrt(sum);
}

}  // namespace

FaceEmbedder::FaceEmbedder(const EmbeddingConfig& config) : config_(config) {}

bool FaceEmbedder::ValidateInputShape(int width, int height, int channels) const {
  return width == config_.input_width &&
         height == config_.input_height &&
         channels == config_.input_channels;
}

EmbeddingResult FaceEmbedder::Postprocess(
    const std::vector<float>& raw_output) const {
  EmbeddingResult result;
  if (static_cast<int>(raw_output.size()) != config_.embedding_dim) {
    return result;
  }

  result.values = raw_output;
  if (config_.l2_normalize) {
    const float norm = L2Norm(result.values);
    if (norm < 1e-9f) {
      return result;
    }
    for (float& value : result.values) {
      value /= norm;
    }
  }

  result.ok = true;
  return result;
}

float FaceEmbedder::CosineSimilarity(const std::vector<float>& a,
                                     const std::vector<float>& b) const {
  if (a.size() != b.size() || a.empty()) {
    return 0.0f;
  }

  float dot = 0.0f;
  float norm_a = 0.0f;
  float norm_b = 0.0f;
  for (std::size_t i = 0; i < a.size(); ++i) {
    dot += a[i] * b[i];
    norm_a += a[i] * a[i];
    norm_b += b[i] * b[i];
  }

  const float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
  if (denom < 1e-9f) {
    return 0.0f;
  }
  return dot / denom;
}

}  // namespace sentriface::embedding
