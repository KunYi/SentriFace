#ifndef SENTRIFACE_EMBEDDING_FACE_EMBEDDER_HPP_
#define SENTRIFACE_EMBEDDING_FACE_EMBEDDER_HPP_

#include <cstddef>
#include <vector>

namespace sentriface::embedding {

struct EmbeddingConfig {
  int input_width = 112;
  int input_height = 112;
  int input_channels = 3;
  int embedding_dim = 512;
  bool l2_normalize = true;
};

struct EmbeddingResult {
  bool ok = false;
  std::vector<float> values;
};

class FaceEmbedder {
 public:
  explicit FaceEmbedder(const EmbeddingConfig& config = EmbeddingConfig {});

  bool ValidateInputShape(int width, int height, int channels) const;
  EmbeddingResult Postprocess(const std::vector<float>& raw_output) const;
  float CosineSimilarity(const std::vector<float>& a,
                         const std::vector<float>& b) const;

 private:
  EmbeddingConfig config_;
};

}  // namespace sentriface::embedding

#endif  // SENTRIFACE_EMBEDDING_FACE_EMBEDDER_HPP_
