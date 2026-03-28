#include "detector/scrfd_runtime.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>
#include <utility>
#include <vector>

#ifdef SENTRIFACE_HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace sentriface::detector {

namespace {

bool IsTensorValid(const ScrfdTensorView& tensor) {
  return tensor.data != nullptr && tensor.rows > 0 && tensor.cols > 0;
}

#ifdef SENTRIFACE_HAS_ONNXRUNTIME
float ClampValue(float value, float min_value, float max_value) {
  return std::max(min_value, std::min(max_value, value));
}

float ReadScore(const ScrfdHeadOutputView& head, int index) {
  if (head.score_dim <= 1) {
    return head.scores[index];
  }
  if (head.score_dim == 2) {
    return head.scores[index * head.score_dim + 1];
  }

  float max_score = head.scores[index * head.score_dim];
  for (int dim = 1; dim < head.score_dim; ++dim) {
    max_score = std::max(max_score, head.scores[index * head.score_dim + dim]);
  }
  return max_score;
}

std::vector<ScrfdRawCandidate> DecodeHeadCandidates(
    const ScrfdDetectorConfig& config,
    const ScrfdHeadOutputView& head,
    int input_width,
    int input_height,
    int stride) {
  std::vector<ScrfdRawCandidate> candidates;
  if (head.scores == nullptr || head.bbox == nullptr ||
      stride <= 0 || input_width <= 0 || input_height <= 0) {
    return candidates;
  }

  const int grid_w = input_width / stride;
  const int grid_h = input_height / stride;
  const int expected_locations = grid_w * grid_h * config.anchors_per_location;
  if (head.locations != expected_locations) {
    return candidates;
  }

  candidates.reserve(static_cast<std::size_t>(head.locations));
  for (int index = 0; index < head.locations; ++index) {
    const float score = ReadScore(head, index);
    if (score < config.detection.score_threshold) {
      continue;
    }

    const int cell_index = index / config.anchors_per_location;
    const int grid_x = cell_index % grid_w;
    const int grid_y = cell_index / grid_w;
    const float center_x = static_cast<float>(grid_x * stride);
    const float center_y = static_cast<float>(grid_y * stride);

    const float left = head.bbox[index * head.bbox_dim + 0] * stride;
    const float top = head.bbox[index * head.bbox_dim + 1] * stride;
    const float right = head.bbox[index * head.bbox_dim + 2] * stride;
    const float bottom = head.bbox[index * head.bbox_dim + 3] * stride;

    const float x1 = ClampValue(center_x - left, 0.0f, static_cast<float>(input_width));
    const float y1 = ClampValue(center_y - top, 0.0f, static_cast<float>(input_height));
    const float x2 = ClampValue(center_x + right, 0.0f, static_cast<float>(input_width));
    const float y2 = ClampValue(center_y + bottom, 0.0f, static_cast<float>(input_height));

    if (x2 <= x1 || y2 <= y1) {
      continue;
    }

    ScrfdRawCandidate candidate;
    candidate.score = score;
    candidate.bbox.x = x1;
    candidate.bbox.y = y1;
    candidate.bbox.w = x2 - x1;
    candidate.bbox.h = y2 - y1;

    if (config.enable_landmarks && head.landmarks != nullptr &&
        head.landmark_dim >= 10) {
      for (int point_index = 0; point_index < 5; ++point_index) {
        const float px =
            head.landmarks[index * head.landmark_dim + point_index * 2 + 0] * stride;
        const float py =
            head.landmarks[index * head.landmark_dim + point_index * 2 + 1] * stride;
        candidate.landmarks.points[point_index].x =
            ClampValue(center_x + px, 0.0f, static_cast<float>(input_width));
        candidate.landmarks.points[point_index].y =
            ClampValue(center_y + py, 0.0f, static_cast<float>(input_height));
      }
    }

    candidates.push_back(candidate);
  }

  return candidates;
}
#endif

class ScrfdCpuRuntimeStub final : public IScrfdRuntime {
 public:
  bool Initialize(const ScrfdDetectorConfig& config) override {
    ready_ = !config.model_path.empty();
    return ready_;
  }

  bool IsReady() const override { return ready_; }

  bool Run(const ScrfdInputTensor& input,
           std::vector<ScrfdRawCandidate>& candidates) override {
    candidates.clear();
    return ready_ && !input.data.empty();
  }

 private:
  bool ready_ = false;
};

#ifdef SENTRIFACE_HAS_ONNXRUNTIME
std::vector<ScrfdRawCandidate> DecodeInferenceOutputs(
    const ScrfdDetectorConfig& config,
    const ScrfdInferenceOutputs& outputs,
    int input_width,
    int input_height) {
  std::vector<ScrfdRawCandidate> candidates;
  const auto heads = BuildScrfdHeadViews(config, outputs, input_width, input_height);
  if (heads.empty()) {
    return candidates;
  }

  for (std::size_t level = 0; level < heads.size(); ++level) {
    const auto level_candidates = DecodeHeadCandidates(
        config, heads[level], input_width, input_height, config.feature_strides[level]);
    candidates.insert(candidates.end(), level_candidates.begin(), level_candidates.end());
  }
  return candidates;
}

class ScrfdCpuOnnxRuntime final : public IScrfdRuntime {
 public:
  bool Initialize(const ScrfdDetectorConfig& config) override {
    config_ = config;
    try {
      session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
      env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "sentriface_scrfd");
      session_ = std::make_unique<Ort::Session>(
          *env_, config.model_path.c_str(), session_options_);

      Ort::AllocatorWithDefaultOptions allocator;
      const std::size_t input_count = session_->GetInputCount();
      const std::size_t output_count = session_->GetOutputCount();
      if (input_count != 1U || output_count == 0U) {
        ready_ = false;
        return false;
      }

      input_names_storage_.clear();
      output_names_storage_.clear();
      input_names_.clear();
      output_names_.clear();

      for (std::size_t index = 0; index < input_count; ++index) {
        auto name = session_->GetInputNameAllocated(index, allocator);
        input_names_storage_.push_back(name.get());
      }
      for (std::size_t index = 0; index < output_count; ++index) {
        auto name = session_->GetOutputNameAllocated(index, allocator);
        output_names_storage_.push_back(name.get());
      }

      input_names_.reserve(input_names_storage_.size());
      for (const auto& name : input_names_storage_) {
        input_names_.push_back(name.c_str());
      }
      output_names_.reserve(output_names_storage_.size());
      for (const auto& name : output_names_storage_) {
        output_names_.push_back(name.c_str());
      }
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

  bool Run(const ScrfdInputTensor& input,
           std::vector<ScrfdRawCandidate>& candidates) override {
    candidates.clear();
    if (!ready_ || session_ == nullptr || env_ == nullptr || input.data.empty()) {
      return false;
    }

    try {
      const std::array<std::int64_t, 4> input_shape = {
          1,
          static_cast<std::int64_t>(input.channels),
          static_cast<std::int64_t>(input.height),
          static_cast<std::int64_t>(input.width),
      };

      auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
      Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
          memory_info,
          const_cast<float*>(input.data.data()),
          input.data.size(),
          input_shape.data(),
          input_shape.size());

      auto output_tensors = session_->Run(
          Ort::RunOptions {nullptr},
          input_names_.data(),
          &input_tensor,
          1,
          output_names_.data(),
          output_names_.size());

      ScrfdInferenceOutputs outputs;
      std::vector<ScrfdTensorView> score_heads;
      std::vector<ScrfdTensorView> bbox_heads;
      std::vector<ScrfdTensorView> landmark_heads;

      for (auto& tensor : output_tensors) {
        if (!tensor.IsTensor()) {
          continue;
        }
        auto info = tensor.GetTensorTypeAndShapeInfo();
        const auto shape = info.GetShape();
        int rows = 0;
        int cols = 0;
        if (shape.size() == 2U) {
          rows = static_cast<int>(shape[0]);
          cols = static_cast<int>(shape[1]);
        } else if (shape.size() == 3U && shape[0] == 1) {
          rows = static_cast<int>(shape[1]);
          cols = static_cast<int>(shape[2]);
        } else {
          continue;
        }

        auto* data = tensor.GetTensorMutableData<float>();
        ScrfdTensorView view {data, rows, cols};
        if (cols == 1) {
          score_heads.push_back(view);
        } else if (cols == 4) {
          bbox_heads.push_back(view);
        } else if (cols >= 10) {
          landmark_heads.push_back(view);
        }
      }

      auto by_rows_desc = [](const ScrfdTensorView& lhs, const ScrfdTensorView& rhs) {
        return lhs.rows > rhs.rows;
      };
      std::sort(score_heads.begin(), score_heads.end(), by_rows_desc);
      std::sort(bbox_heads.begin(), bbox_heads.end(), by_rows_desc);
      std::sort(landmark_heads.begin(), landmark_heads.end(), by_rows_desc);

      outputs.score_heads = std::move(score_heads);
      outputs.bbox_heads = std::move(bbox_heads);
      outputs.landmark_heads = std::move(landmark_heads);

      if (!ValidateScrfdInferenceOutputs(config_, outputs, input.width, input.height)) {
        return false;
      }

      candidates = DecodeInferenceOutputs(config_, outputs, input.width, input.height);
      return true;
    } catch (const Ort::Exception&) {
      candidates.clear();
      return false;
    }
  }

 private:
  ScrfdDetectorConfig config_ {};
  bool ready_ = false;
  Ort::SessionOptions session_options_ {};
  std::unique_ptr<Ort::Env> env_;
  std::unique_ptr<Ort::Session> session_;
  std::vector<std::string> input_names_storage_;
  std::vector<std::string> output_names_storage_;
  std::vector<const char*> input_names_;
  std::vector<const char*> output_names_;
};
#endif

class ScrfdRknnRuntimeStub final : public IScrfdRuntime {
 public:
  bool Initialize(const ScrfdDetectorConfig& config) override {
    ready_ = !config.model_path.empty();
    return ready_;
  }

  bool IsReady() const override { return ready_; }

  bool Run(const ScrfdInputTensor& input,
           std::vector<ScrfdRawCandidate>& candidates) override {
    candidates.clear();
    return ready_ && !input.data.empty();
  }

 private:
  bool ready_ = false;
};

}  // namespace

bool ValidateScrfdInferenceOutputs(const ScrfdDetectorConfig& config,
                                   const ScrfdInferenceOutputs& outputs,
                                   int input_width,
                                   int input_height) {
  if (input_width <= 0 || input_height <= 0) {
    return false;
  }

  const std::size_t levels = config.feature_strides.size();
  if (levels == 0U ||
      outputs.score_heads.size() != levels ||
      outputs.bbox_heads.size() != levels) {
    return false;
  }
  if (config.enable_landmarks && outputs.landmark_heads.size() != levels) {
    return false;
  }
  if (!config.enable_landmarks && !outputs.landmark_heads.empty() &&
      outputs.landmark_heads.size() != levels) {
    return false;
  }

  for (std::size_t level = 0; level < levels; ++level) {
    const int stride = config.feature_strides[level];
    if (stride <= 0) {
      return false;
    }
    const int grid_w = input_width / stride;
    const int grid_h = input_height / stride;
    const int expected_rows = grid_w * grid_h * config.anchors_per_location;
    if (expected_rows <= 0) {
      return false;
    }

    const auto& score_head = outputs.score_heads[level];
    const auto& bbox_head = outputs.bbox_heads[level];
    if (!IsTensorValid(score_head) || !IsTensorValid(bbox_head)) {
      return false;
    }
    if (score_head.rows != expected_rows || bbox_head.rows != expected_rows) {
      return false;
    }
    if (bbox_head.cols < 4) {
      return false;
    }

    if (config.enable_landmarks) {
      const auto& landmark_head = outputs.landmark_heads[level];
      if (!IsTensorValid(landmark_head) ||
          landmark_head.rows != expected_rows ||
          landmark_head.cols < 10) {
        return false;
      }
    }
  }

  return true;
}

std::vector<ScrfdHeadOutputView> BuildScrfdHeadViews(
    const ScrfdDetectorConfig& config,
    const ScrfdInferenceOutputs& outputs,
    int input_width,
    int input_height) {
  std::vector<ScrfdHeadOutputView> heads;
  if (!ValidateScrfdInferenceOutputs(config, outputs, input_width, input_height)) {
    return heads;
  }

  heads.reserve(config.feature_strides.size());
  for (std::size_t level = 0; level < config.feature_strides.size(); ++level) {
    ScrfdHeadOutputView head;
    head.scores = outputs.score_heads[level].data;
    head.bbox = outputs.bbox_heads[level].data;
    head.landmarks = config.enable_landmarks && !outputs.landmark_heads.empty()
                         ? outputs.landmark_heads[level].data
                         : nullptr;
    head.locations = outputs.score_heads[level].rows;
    head.score_dim = outputs.score_heads[level].cols;
    head.bbox_dim = outputs.bbox_heads[level].cols;
    head.landmark_dim = head.landmarks != nullptr
                            ? outputs.landmark_heads[level].cols
                            : 0;
    heads.push_back(head);
  }
  return heads;
}

std::shared_ptr<IScrfdRuntime> CreateDefaultScrfdRuntime(
    const ScrfdDetectorConfig& config) {
  std::shared_ptr<IScrfdRuntime> runtime =
      config.runtime == ScrfdRuntime::kRknn
          ? std::static_pointer_cast<IScrfdRuntime>(
                std::make_shared<ScrfdRknnRuntimeStub>())
          : std::static_pointer_cast<IScrfdRuntime>(
 #ifdef SENTRIFACE_HAS_ONNXRUNTIME
                std::make_shared<ScrfdCpuOnnxRuntime>());
 #else
                std::make_shared<ScrfdCpuRuntimeStub>());
 #endif
  if (!runtime->Initialize(config)) {
    return nullptr;
  }
  return runtime;
}

}  // namespace sentriface::detector
