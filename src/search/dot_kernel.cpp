#include "search/dot_kernel.hpp"

#include <cmath>
#include <cstddef>

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <immintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

namespace sentriface::search {

namespace {

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#if defined(__GNUC__) || defined(__clang__)
__attribute__((target("sse2")))
#endif
float HorizontalSum128(__m128 value) {
  alignas(16) float lanes[4];
  _mm_storeu_ps(lanes, value);
  return lanes[0] + lanes[1] + lanes[2] + lanes[3];
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((target("avx2")))
#endif
float HorizontalSum256(__m256 value) {
  alignas(32) float lanes[8];
  _mm256_storeu_ps(lanes, value);
  float sum = 0.0f;
  for (float lane : lanes) {
    sum += lane;
  }
  return sum;
}
#endif

[[maybe_unused]] float DotScalar512(const float* lhs, const float* rhs) {
  float sum = 0.0f;
  for (std::size_t i = 0; i < kDotKernelDimension; ++i) {
    sum += lhs[i] * rhs[i];
  }
  return sum;
}

using Dot512Fn = float (*)(const float*, const float*);
using Dot512BatchFn = void (*)(const float*, const float*, std::size_t, float*);

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#if defined(__GNUC__) || defined(__clang__)
__attribute__((target("avx2")))
#endif
float DotAvx2512(const float* lhs, const float* rhs) {
  __m256 acc0 = _mm256_setzero_ps();
  __m256 acc1 = _mm256_setzero_ps();
  for (std::size_t i = 0; i < kDotKernelDimension; i += 16U) {
    const __m256 lhs0 = _mm256_loadu_ps(lhs + i);
    const __m256 rhs0 = _mm256_loadu_ps(rhs + i);
    const __m256 lhs1 = _mm256_loadu_ps(lhs + i + 8U);
    const __m256 rhs1 = _mm256_loadu_ps(rhs + i + 8U);
    acc0 = _mm256_add_ps(acc0, _mm256_mul_ps(lhs0, rhs0));
    acc1 = _mm256_add_ps(acc1, _mm256_mul_ps(lhs1, rhs1));
  }
  return HorizontalSum256(_mm256_add_ps(acc0, acc1));
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((target("sse2")))
#endif
float DotSse2512(const float* lhs, const float* rhs) {
  __m128 acc0 = _mm_setzero_ps();
  __m128 acc1 = _mm_setzero_ps();
  __m128 acc2 = _mm_setzero_ps();
  __m128 acc3 = _mm_setzero_ps();
  for (std::size_t i = 0; i < kDotKernelDimension; i += 16U) {
    const __m128 lhs0 = _mm_loadu_ps(lhs + i);
    const __m128 rhs0 = _mm_loadu_ps(rhs + i);
    const __m128 lhs1 = _mm_loadu_ps(lhs + i + 4U);
    const __m128 rhs1 = _mm_loadu_ps(rhs + i + 4U);
    const __m128 lhs2 = _mm_loadu_ps(lhs + i + 8U);
    const __m128 rhs2 = _mm_loadu_ps(rhs + i + 8U);
    const __m128 lhs3 = _mm_loadu_ps(lhs + i + 12U);
    const __m128 rhs3 = _mm_loadu_ps(rhs + i + 12U);
    acc0 = _mm_add_ps(acc0, _mm_mul_ps(lhs0, rhs0));
    acc1 = _mm_add_ps(acc1, _mm_mul_ps(lhs1, rhs1));
    acc2 = _mm_add_ps(acc2, _mm_mul_ps(lhs2, rhs2));
    acc3 = _mm_add_ps(acc3, _mm_mul_ps(lhs3, rhs3));
  }
  return HorizontalSum128(_mm_add_ps(_mm_add_ps(acc0, acc1),
                                     _mm_add_ps(acc2, acc3)));
}

bool CpuSupportsAvx2() {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_cpu_init();
  return __builtin_cpu_supports("avx2");
#else
  return false;
#endif
}

bool CpuSupportsSse2() {
#if defined(__x86_64__) || defined(_M_X64)
  return true;
#elif defined(__GNUC__) || defined(__clang__)
  __builtin_cpu_init();
  return __builtin_cpu_supports("sse2");
#else
  return false;
#endif
}

Dot512Fn ResolveDot512Fn() {
  if (CpuSupportsAvx2()) {
    return &DotAvx2512;
  }
  if (CpuSupportsSse2()) {
    return &DotSse2512;
  }
  return &DotScalar512;
}

DotKernelInfo ResolveDotKernelInfo() {
  if (CpuSupportsAvx2()) {
    return DotKernelInfo {
        DotKernelBackend::kAvx2, "avx2", 8U, true};
  }
  if (CpuSupportsSse2()) {
    return DotKernelInfo {
        DotKernelBackend::kSse2, "sse2", 4U, true};
  }
  return DotKernelInfo {
      DotKernelBackend::kScalar, "scalar", 1U, true};
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((target("avx2")))
#endif
void DotAvx2Batch512(const float* query,
                     const float* matrix,
                     std::size_t count,
                     float* out_scores) {
  std::size_t row = 0U;
  for (; row + 1U < count; row += 2U) {
    const float* row0 = matrix + (row * kDotKernelDimension);
    const float* row1 = row0 + kDotKernelDimension;
    __m256 acc0 = _mm256_setzero_ps();
    __m256 acc1 = _mm256_setzero_ps();
    for (std::size_t i = 0; i < kDotKernelDimension; i += 8U) {
      const __m256 q = _mm256_loadu_ps(query + i);
      acc0 = _mm256_add_ps(acc0, _mm256_mul_ps(q, _mm256_loadu_ps(row0 + i)));
      acc1 = _mm256_add_ps(acc1, _mm256_mul_ps(q, _mm256_loadu_ps(row1 + i)));
    }
    out_scores[row] = HorizontalSum256(acc0);
    out_scores[row + 1U] = HorizontalSum256(acc1);
  }

  if (row < count) {
    out_scores[row] = DotAvx2512(query, matrix + (row * kDotKernelDimension));
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((target("sse2")))
#endif
void DotSse2Batch512(const float* query,
                     const float* matrix,
                     std::size_t count,
                     float* out_scores) {
  std::size_t row = 0U;
  for (; row + 1U < count; row += 2U) {
    const float* row0 = matrix + (row * kDotKernelDimension);
    const float* row1 = row0 + kDotKernelDimension;
    __m128 acc00 = _mm_setzero_ps();
    __m128 acc01 = _mm_setzero_ps();
    __m128 acc10 = _mm_setzero_ps();
    __m128 acc11 = _mm_setzero_ps();
    for (std::size_t i = 0; i < kDotKernelDimension; i += 8U) {
      const __m128 q0 = _mm_loadu_ps(query + i);
      const __m128 q1 = _mm_loadu_ps(query + i + 4U);
      acc00 = _mm_add_ps(acc00, _mm_mul_ps(q0, _mm_loadu_ps(row0 + i)));
      acc01 = _mm_add_ps(acc01, _mm_mul_ps(q1, _mm_loadu_ps(row0 + i + 4U)));
      acc10 = _mm_add_ps(acc10, _mm_mul_ps(q0, _mm_loadu_ps(row1 + i)));
      acc11 = _mm_add_ps(acc11, _mm_mul_ps(q1, _mm_loadu_ps(row1 + i + 4U)));
    }
    out_scores[row] = HorizontalSum128(_mm_add_ps(acc00, acc01));
    out_scores[row + 1U] = HorizontalSum128(_mm_add_ps(acc10, acc11));
  }

  if (row < count) {
    out_scores[row] = DotSse2512(query, matrix + (row * kDotKernelDimension));
  }
}

Dot512BatchFn ResolveDot512BatchFn() {
  if (CpuSupportsAvx2()) {
    return &DotAvx2Batch512;
  }
  if (CpuSupportsSse2()) {
    return &DotSse2Batch512;
  }
  return nullptr;
}
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
float DotNeon512(const float* lhs, const float* rhs) {
  float32x4_t acc0 = vdupq_n_f32(0.0f);
  float32x4_t acc1 = vdupq_n_f32(0.0f);
  float32x4_t acc2 = vdupq_n_f32(0.0f);
  float32x4_t acc3 = vdupq_n_f32(0.0f);
  for (std::size_t i = 0; i < kDotKernelDimension; i += 16U) {
    const float32x4_t lhs0 = vld1q_f32(lhs + i);
    const float32x4_t rhs0 = vld1q_f32(rhs + i);
    const float32x4_t lhs1 = vld1q_f32(lhs + i + 4U);
    const float32x4_t rhs1 = vld1q_f32(rhs + i + 4U);
    const float32x4_t lhs2 = vld1q_f32(lhs + i + 8U);
    const float32x4_t rhs2 = vld1q_f32(rhs + i + 8U);
    const float32x4_t lhs3 = vld1q_f32(lhs + i + 12U);
    const float32x4_t rhs3 = vld1q_f32(rhs + i + 12U);
    acc0 = vmlaq_f32(acc0, lhs0, rhs0);
    acc1 = vmlaq_f32(acc1, lhs1, rhs1);
    acc2 = vmlaq_f32(acc2, lhs2, rhs2);
    acc3 = vmlaq_f32(acc3, lhs3, rhs3);
  }
  const float32x4_t sum01 = vaddq_f32(acc0, acc1);
  const float32x4_t sum23 = vaddq_f32(acc2, acc3);
  const float32x4_t sum = vaddq_f32(sum01, sum23);
  const float32x2_t pair_sum = vadd_f32(vget_low_f32(sum), vget_high_f32(sum));
  return vget_lane_f32(vpadd_f32(pair_sum, pair_sum), 0);
}

Dot512Fn ResolveDot512Fn() {
  return &DotNeon512;
}

DotKernelInfo ResolveDotKernelInfo() {
  return DotKernelInfo {
      DotKernelBackend::kNeon, "neon", 4U, true};
}

void DotNeonBatch512(const float* query,
                     const float* matrix,
                     std::size_t count,
                     float* out_scores) {
  std::size_t row = 0U;
  for (; row + 1U < count; row += 2U) {
    const float* row0 = matrix + (row * kDotKernelDimension);
    const float* row1 = row0 + kDotKernelDimension;
    float32x4_t acc00 = vdupq_n_f32(0.0f);
    float32x4_t acc01 = vdupq_n_f32(0.0f);
    float32x4_t acc10 = vdupq_n_f32(0.0f);
    float32x4_t acc11 = vdupq_n_f32(0.0f);
    for (std::size_t i = 0; i < kDotKernelDimension; i += 8U) {
      const float32x4_t q0 = vld1q_f32(query + i);
      const float32x4_t q1 = vld1q_f32(query + i + 4U);
      acc00 = vmlaq_f32(acc00, q0, vld1q_f32(row0 + i));
      acc01 = vmlaq_f32(acc01, q1, vld1q_f32(row0 + i + 4U));
      acc10 = vmlaq_f32(acc10, q0, vld1q_f32(row1 + i));
      acc11 = vmlaq_f32(acc11, q1, vld1q_f32(row1 + i + 4U));
    }
    const float32x4_t sum0 = vaddq_f32(acc00, acc01);
    const float32x4_t sum1 = vaddq_f32(acc10, acc11);
    const float32x2_t pair0 = vadd_f32(vget_low_f32(sum0), vget_high_f32(sum0));
    const float32x2_t pair1 = vadd_f32(vget_low_f32(sum1), vget_high_f32(sum1));
    out_scores[row] = vget_lane_f32(vpadd_f32(pair0, pair0), 0);
    out_scores[row + 1U] = vget_lane_f32(vpadd_f32(pair1, pair1), 0);
  }

  if (row < count) {
    out_scores[row] = DotNeon512(query, matrix + (row * kDotKernelDimension));
  }
}

Dot512BatchFn ResolveDot512BatchFn() {
  return &DotNeonBatch512;
}
#else
Dot512Fn ResolveDot512Fn() {
  return &DotScalar512;
}

DotKernelInfo ResolveDotKernelInfo() {
  return DotKernelInfo {
      DotKernelBackend::kScalar, "scalar", 1U, true};
}

Dot512BatchFn ResolveDot512BatchFn() {
  return nullptr;
}
#endif

}  // namespace

float Dot512(const float* lhs, const float* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return 0.0f;
  }

  static const Dot512Fn kDot512Fn = ResolveDot512Fn();
  return kDot512Fn(lhs, rhs);
}

void Dot512Batch(const float* query,
                 const float* matrix,
                 std::size_t count,
                 float* out_scores) {
  if (query == nullptr || matrix == nullptr || out_scores == nullptr) {
    return;
  }
  static const Dot512BatchFn kDot512BatchFn = ResolveDot512BatchFn();
  if (kDot512BatchFn != nullptr) {
    kDot512BatchFn(query, matrix, count, out_scores);
    return;
  }

  static const Dot512Fn kDot512Fn = ResolveDot512Fn();
  for (std::size_t row = 0; row < count; ++row) {
    out_scores[row] = kDot512Fn(query, matrix + (row * kDotKernelDimension));
  }
}

void Normalize512InPlace(float* values) {
  if (values == nullptr) {
    return;
  }
  const float norm_sq = Dot512(values, values);
  if (norm_sq <= 0.0f) {
    return;
  }
  const float inv_norm = 1.0f / std::sqrt(norm_sq);
  for (std::size_t i = 0; i < kDotKernelDimension; ++i) {
    values[i] *= inv_norm;
  }
}

DotKernelInfo GetDotKernelInfo() { return ResolveDotKernelInfo(); }

}  // namespace sentriface::search
