#ifndef SENTRIFACE_EMBEDDING_FACE_EMBEDDER_HPP_
#define SENTRIFACE_EMBEDDING_FACE_EMBEDDER_HPP_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "camera/frame_source.hpp"

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

enum class EmbeddingRuntimeBackend : std::uint8_t {
  kStubDeterministic = 0,
  kOnnxRuntime = 1,
};

struct EmbeddingRuntimeConfig {
  EmbeddingRuntimeBackend backend =
      EmbeddingRuntimeBackend::kStubDeterministic;
  std::string model_path;
};

struct EmbeddingInputTensor {
  int width = 0;
  int height = 0;
  int channels = 0;
  std::vector<float> data;
};

class IFaceEmbeddingRuntime {
 public:
  virtual ~IFaceEmbeddingRuntime() = default;

  virtual bool Initialize(const EmbeddingConfig& config,
                          const EmbeddingRuntimeConfig& runtime_config) = 0;
  virtual bool IsReady() const = 0;
  virtual bool Run(const EmbeddingInputTensor& input,
                   std::vector<float>& raw_output) = 0;
};

const char* ToString(EmbeddingRuntimeBackend backend);

class FaceEmbedder {
 public:
  explicit FaceEmbedder(const EmbeddingConfig& config = EmbeddingConfig {});

  bool ValidateInputShape(int width, int height, int channels) const;
  EmbeddingInputTensor PrepareInputTensor(
      const sentriface::camera::Frame& image) const;
  EmbeddingResult Postprocess(const std::vector<float>& raw_output) const;
  bool InitializeRuntime(
      const EmbeddingRuntimeConfig& runtime_config = EmbeddingRuntimeConfig {});
  bool IsRuntimeReady() const;
  EmbeddingResult Run(const sentriface::camera::Frame& image) const;
  float CosineSimilarity(const std::vector<float>& a,
                         const std::vector<float>& b) const;

 private:
  EmbeddingConfig config_;
  std::shared_ptr<IFaceEmbeddingRuntime> runtime_;
};

}  // namespace sentriface::embedding

#endif  // SENTRIFACE_EMBEDDING_FACE_EMBEDDER_HPP_
