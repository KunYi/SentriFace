#ifndef SENTRIFACE_SEARCH_DOT_KERNEL_HPP_
#define SENTRIFACE_SEARCH_DOT_KERNEL_HPP_

#include <cstddef>

namespace sentriface::search {

constexpr std::size_t kDotKernelDimension = 512U;

enum class DotKernelBackend {
  kScalar,
  kSse2,
  kAvx2,
  kNeon,
};

struct DotKernelInfo {
  DotKernelBackend backend = DotKernelBackend::kScalar;
  const char* implementation = "scalar";
  std::size_t lanes = 1U;
  bool normalized_cosine_ready = true;
};

float Dot512(const float* lhs, const float* rhs);
void Dot512Batch(const float* query,
                 const float* matrix,
                 std::size_t count,
                 float* out_scores);
void Normalize512InPlace(float* values);
DotKernelInfo GetDotKernelInfo();

}  // namespace sentriface::search

#endif  // SENTRIFACE_SEARCH_DOT_KERNEL_HPP_
