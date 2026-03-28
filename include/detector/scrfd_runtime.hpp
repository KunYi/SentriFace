#ifndef SENTRIFACE_DETECTOR_SCRFD_RUNTIME_HPP_
#define SENTRIFACE_DETECTOR_SCRFD_RUNTIME_HPP_

#include <memory>
#include <vector>

#include "detector/face_detector.hpp"

namespace sentriface::detector {

struct ScrfdTensorView {
  const float* data = nullptr;
  int rows = 0;
  int cols = 0;
};

struct ScrfdInferenceOutputs {
  std::vector<ScrfdTensorView> score_heads;
  std::vector<ScrfdTensorView> bbox_heads;
  std::vector<ScrfdTensorView> landmark_heads;
};

class IScrfdRuntime {
 public:
  virtual ~IScrfdRuntime() = default;

  virtual bool Initialize(const ScrfdDetectorConfig& config) = 0;
  virtual bool IsReady() const = 0;
  virtual bool Run(const ScrfdInputTensor& input,
                   std::vector<ScrfdRawCandidate>& candidates) = 0;
};

bool ValidateScrfdInferenceOutputs(const ScrfdDetectorConfig& config,
                                   const ScrfdInferenceOutputs& outputs,
                                   int input_width,
                                   int input_height);

std::vector<ScrfdHeadOutputView> BuildScrfdHeadViews(
    const ScrfdDetectorConfig& config,
    const ScrfdInferenceOutputs& outputs,
    int input_width,
    int input_height);

std::shared_ptr<IScrfdRuntime> CreateDefaultScrfdRuntime(
    const ScrfdDetectorConfig& config);

}  // namespace sentriface::detector

#endif  // SENTRIFACE_DETECTOR_SCRFD_RUNTIME_HPP_
