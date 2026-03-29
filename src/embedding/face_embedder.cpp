#include "embedding/face_embedder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#ifdef SENTRIFACE_HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace sentriface::embedding {
namespace {

float L2Norm(const std::vector<float>& values) {
  float sum = 0.0f;
  for (float v : values) {
    sum += v * v;
  }
  return std::sqrt(sum);
}

float ReadFrameChannel(const sentriface::camera::Frame& image,
                       int x,
                       int y,
                       int channel) {
  if (x < 0 || y < 0 || x >= image.width || y >= image.height ||
      channel < 0 || channel >= image.channels) {
    return 0.0f;
  }
  const std::size_t index =
      static_cast<std::size_t>((y * image.width + x) * image.channels + channel);
  return static_cast<float>(image.data[index]);
}

float SampleResizedChannel(const sentriface::camera::Frame& image,
                           float src_x,
                           float src_y,
                           int channel) {
  const float clamped_x = std::max(0.0f, std::min(src_x, image.width - 1.0f));
  const float clamped_y = std::max(0.0f, std::min(src_y, image.height - 1.0f));
  const int x0 = static_cast<int>(std::floor(clamped_x));
  const int y0 = static_cast<int>(std::floor(clamped_y));
  const int x1 = std::min(x0 + 1, image.width - 1);
  const int y1 = std::min(y0 + 1, image.height - 1);
  const float wx = clamped_x - x0;
  const float wy = clamped_y - y0;
  const float v00 = ReadFrameChannel(image, x0, y0, channel);
  const float v10 = ReadFrameChannel(image, x1, y0, channel);
  const float v01 = ReadFrameChannel(image, x0, y1, channel);
  const float v11 = ReadFrameChannel(image, x1, y1, channel);
  const float top = v00 * (1.0f - wx) + v10 * wx;
  const float bottom = v01 * (1.0f - wx) + v11 * wx;
  return top * (1.0f - wy) + bottom * wy;
}

int ResolveSourceChannel(const sentriface::camera::Frame& image, int dest_channel) {
  if (image.channels <= 0) {
    return -1;
  }
  if (image.channels == 1) {
    return 0;
  }
  if (image.pixel_format == sentriface::camera::PixelFormat::kRgb888) {
    return 2 - dest_channel;
  }
  return dest_channel;
}

class StubDeterministicEmbeddingRuntime final : public IFaceEmbeddingRuntime {
 public:
  bool Initialize(const EmbeddingConfig& config,
                  const EmbeddingRuntimeConfig& runtime_config) override {
    (void)runtime_config;
    config_ = config;
    ready_ = config.embedding_dim > 0;
    return ready_;
  }

  bool IsReady() const override { return ready_; }

  bool Run(const EmbeddingInputTensor& input,
           std::vector<float>& raw_output) override {
    raw_output.clear();
    if (!ready_ || input.data.empty()) {
      return false;
    }

    raw_output.resize(static_cast<std::size_t>(config_.embedding_dim), 0.0f);
    for (std::size_t i = 0; i < input.data.size(); ++i) {
      const std::size_t slot =
          i % static_cast<std::size_t>(config_.embedding_dim);
      raw_output[slot] += input.data[i] * static_cast<float>((i % 7U) + 1U);
    }
    return true;
  }

 private:
  EmbeddingConfig config_ {};
  bool ready_ = false;
};

#ifdef SENTRIFACE_HAS_ONNXRUNTIME
class OnnxRuntimeFaceEmbeddingRuntime final : public IFaceEmbeddingRuntime {
 public:
  bool Initialize(const EmbeddingConfig& config,
                  const EmbeddingRuntimeConfig& runtime_config) override {
    config_ = config;
    try {
      session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
      env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING,
                                        "sentriface_embedding");
      session_ = std::make_unique<Ort::Session>(
          *env_, runtime_config.model_path.c_str(), session_options_);

      Ort::AllocatorWithDefaultOptions allocator;
      if (session_->GetInputCount() != 1U || session_->GetOutputCount() != 1U) {
        ready_ = false;
        return false;
      }
      auto input_name = session_->GetInputNameAllocated(0, allocator);
      auto output_name = session_->GetOutputNameAllocated(0, allocator);
      input_name_storage_ = input_name.get();
      output_name_storage_ = output_name.get();
      input_name_ = input_name_storage_.c_str();
      output_name_ = output_name_storage_.c_str();
      ready_ = true;
      return true;
    } catch (const Ort::Exception&) {
      ready_ = false;
      session_.reset();
      env_.reset();
      return false;
    }
  }

  bool IsReady() const override { return ready_; }

  bool Run(const EmbeddingInputTensor& input,
           std::vector<float>& raw_output) override {
    raw_output.clear();
    if (!ready_ || session_ == nullptr || input.data.empty()) {
      return false;
    }
    try {
      const std::array<std::int64_t, 4> input_shape = {
          1,
          static_cast<std::int64_t>(input.channels),
          static_cast<std::int64_t>(input.height),
          static_cast<std::int64_t>(input.width),
      };
      auto memory_info =
          Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
      Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
          memory_info,
          const_cast<float*>(input.data.data()),
          input.data.size(),
          input_shape.data(),
          input_shape.size());
      const char* input_names[] = {input_name_};
      const char* output_names[] = {output_name_};
      auto output_tensors = session_->Run(Ort::RunOptions {nullptr},
                                          input_names,
                                          &input_tensor,
                                          1,
                                          output_names,
                                          1);
      if (output_tensors.empty() || !output_tensors.front().IsTensor()) {
        return false;
      }
      auto info = output_tensors.front().GetTensorTypeAndShapeInfo();
      const auto shape = info.GetShape();
      std::size_t element_count = 1;
      for (std::int64_t dim : shape) {
        if (dim <= 0) {
          return false;
        }
        element_count *= static_cast<std::size_t>(dim);
      }
      auto* data = output_tensors.front().GetTensorMutableData<float>();
      raw_output.assign(data, data + element_count);
      return true;
    } catch (const Ort::Exception&) {
      raw_output.clear();
      return false;
    }
  }

 private:
  EmbeddingConfig config_ {};
  bool ready_ = false;
  Ort::SessionOptions session_options_ {};
  std::unique_ptr<Ort::Env> env_;
  std::unique_ptr<Ort::Session> session_;
  std::string input_name_storage_;
  std::string output_name_storage_;
  const char* input_name_ = nullptr;
  const char* output_name_ = nullptr;
};
#endif

std::shared_ptr<IFaceEmbeddingRuntime> CreateRuntime(
    const EmbeddingRuntimeConfig& runtime_config) {
  switch (runtime_config.backend) {
    case EmbeddingRuntimeBackend::kStubDeterministic:
      return std::make_shared<StubDeterministicEmbeddingRuntime>();
    case EmbeddingRuntimeBackend::kOnnxRuntime:
#ifdef SENTRIFACE_HAS_ONNXRUNTIME
      return std::make_shared<OnnxRuntimeFaceEmbeddingRuntime>();
#else
      return nullptr;
#endif
  }
  return nullptr;
}

}  // namespace

FaceEmbedder::FaceEmbedder(const EmbeddingConfig& config) : config_(config) {}

const char* ToString(EmbeddingRuntimeBackend backend) {
  switch (backend) {
    case EmbeddingRuntimeBackend::kStubDeterministic:
      return "stub_deterministic";
    case EmbeddingRuntimeBackend::kOnnxRuntime:
      return "onnxruntime";
  }
  return "unknown";
}

bool FaceEmbedder::ValidateInputShape(int width, int height, int channels) const {
  return width == config_.input_width &&
         height == config_.input_height &&
         channels == config_.input_channels;
}

EmbeddingInputTensor FaceEmbedder::PrepareInputTensor(
    const sentriface::camera::Frame& image) const {
  EmbeddingInputTensor tensor;
  tensor.width = config_.input_width;
  tensor.height = config_.input_height;
  tensor.channels = config_.input_channels;
  if (image.width <= 0 || image.height <= 0 || image.channels <= 0 ||
      image.data.empty() || config_.input_width <= 0 ||
      config_.input_height <= 0 || config_.input_channels != 3) {
    return tensor;
  }

  tensor.data.resize(static_cast<std::size_t>(
                         config_.input_width * config_.input_height *
                         config_.input_channels),
                     0.0f);
  for (int channel = 0; channel < config_.input_channels; ++channel) {
    const int source_channel = ResolveSourceChannel(image, channel);
    if (source_channel < 0) {
      continue;
    }
    for (int y = 0; y < config_.input_height; ++y) {
      const float src_y =
          (static_cast<float>(y) + 0.5f) * image.height / config_.input_height - 0.5f;
      for (int x = 0; x < config_.input_width; ++x) {
        const float src_x =
            (static_cast<float>(x) + 0.5f) * image.width / config_.input_width - 0.5f;
        const float value = SampleResizedChannel(image, src_x, src_y, source_channel);
        const std::size_t index = static_cast<std::size_t>(
            channel * config_.input_width * config_.input_height +
            y * config_.input_width + x);
        tensor.data[index] = (value - 127.5f) / 127.5f;
      }
    }
  }
  return tensor;
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

bool FaceEmbedder::InitializeRuntime(
    const EmbeddingRuntimeConfig& runtime_config) {
  runtime_ = CreateRuntime(runtime_config);
  return runtime_ != nullptr && runtime_->Initialize(config_, runtime_config);
}

bool FaceEmbedder::IsRuntimeReady() const {
  return runtime_ != nullptr && runtime_->IsReady();
}

EmbeddingResult FaceEmbedder::Run(const sentriface::camera::Frame& image) const {
  EmbeddingResult result;
  if (runtime_ == nullptr || !runtime_->IsReady()) {
    return result;
  }
  const EmbeddingInputTensor tensor = PrepareInputTensor(image);
  if (tensor.data.empty()) {
    return result;
  }
  std::vector<float> raw_output;
  if (!runtime_->Run(tensor, raw_output)) {
    return result;
  }
  return Postprocess(raw_output);
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
