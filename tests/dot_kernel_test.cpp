#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "search/dot_kernel.hpp"

namespace {

bool AlmostEqual(float lhs, float rhs, float eps = 1e-3f) {
  return std::fabs(lhs - rhs) <= eps;
}

bool AlmostEqualScaled(float lhs,
                       float rhs,
                       float abs_eps = 1e-3f,
                       float rel_eps = 2e-6f) {
  const float diff = std::fabs(lhs - rhs);
  const float scale = std::fmax(std::fabs(lhs), std::fabs(rhs));
  return diff <= abs_eps + (rel_eps * scale);
}

float DotScalarReference(const float* lhs, const float* rhs, std::size_t count) {
  float sum = 0.0f;
  for (std::size_t i = 0; i < count; ++i) {
    sum += lhs[i] * rhs[i];
  }
  return sum;
}

void NormalizeScalarReference(std::vector<float>* values) {
  if (values == nullptr || values->empty()) {
    return;
  }

  const float norm_sq =
      DotScalarReference(values->data(), values->data(), values->size());
  if (norm_sq <= 0.0f) {
    return;
  }

  const float inv_norm = 1.0f / std::sqrt(norm_sq);
  for (float& value : *values) {
    value *= inv_norm;
  }
}

void FillPatternCase(std::size_t case_index,
                     float* lhs,
                     float* rhs,
                     std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    switch (case_index) {
      case 0:
        lhs[i] = static_cast<float>(i + 1U);
        rhs[i] = 2.0f;
        break;
      case 1:
        lhs[i] = ((i % 17U) < 8U) ? 1.0f : -1.0f;
        rhs[i] = static_cast<float>(static_cast<int>(i % 7U) - 3);
        break;
      case 2:
        lhs[i] = (i % 2U == 0U) ? 0.25f : -0.75f;
        rhs[i] = static_cast<float>((i % 5U) * 0.5f);
        break;
      case 3:
        lhs[i] = static_cast<float>((static_cast<int>(i % 13U) - 6) * 0.125f);
        rhs[i] = static_cast<float>((static_cast<int>(i % 11U) - 5) * 0.2f);
        break;
      case 4:
        lhs[i] = (i % 3U == 0U) ? 3.0f : 0.0f;
        rhs[i] = (i % 3U == 0U) ? -2.0f : 1.0f;
        break;
      case 5:
        lhs[i] = static_cast<float>((static_cast<int>((i * 37U + 11U) % 101U) -
                                     50) *
                                    0.03125f);
        rhs[i] = static_cast<float>((static_cast<int>((i * 19U + 7U) % 89U) -
                                     44) *
                                    0.0625f);
        break;
      default:
        lhs[i] = 0.0f;
        rhs[i] = 0.0f;
        break;
    }
  }
}

template <std::size_t N>
void ScatterToDense(const std::array<int, N>& indices,
                    const std::array<float, N>& values,
                    float* out_dense,
                    std::size_t count) {
  if (out_dense == nullptr) {
    return;
  }

  for (std::size_t i = 0; i < count; ++i) {
    out_dense[i] = 0.0f;
  }
  for (std::size_t i = 0; i < N; ++i) {
    out_dense[static_cast<std::size_t>(indices[i])] = values[i];
  }
}

struct NumpySparseCase {
  std::array<int, 12U> lhs_idx;
  std::array<float, 12U> lhs_vals;
  std::array<int, 12U> rhs_idx;
  std::array<float, 12U> rhs_vals;
  std::array<int, 10U> matrix0_idx;
  std::array<float, 10U> matrix0_vals;
  std::array<int, 10U> matrix1_idx;
  std::array<float, 10U> matrix1_vals;
  std::array<int, 10U> matrix2_idx;
  std::array<float, 10U> matrix2_vals;
  float expected_dot = 0.0f;
  std::array<float, 3U> expected_batch {};
  float expected_norm = 0.0f;
  std::array<int, 12U> expected_normalized_idx;
  std::array<float, 12U> expected_normalized_vals;
};

constexpr NumpySparseCase kNumpySparseCases[] = {
    {
        {40, 54, 91, 97, 110, 161, 168, 232, 252, 439, 444, 488},
        {1.15169454f, 0.17772140f, -0.76952851f, 0.99493665f, 0.24769156f,
         -2.83665895f, 2.63394380f, -0.88804322f, 0.67400187f, 0.07132856f,
         -0.26615930f, -0.35937709f},
        {61, 83, 180, 199, 210, 289, 298, 307, 417, 445, 490, 499},
        {0.16598196f, -0.09786165f, -0.11493796f, 1.58615851f, 0.54305381f,
         -0.14608459f, -1.19939232f, -0.01759937f, -0.15412883f, 1.43247938f,
         0.50994664f, -0.16624263f},
        {2, 19, 38, 57, 100, 222, 275, 318, 431, 445},
        {-0.33777061f, 0.18436211f, 0.00837850f, -0.52233827f, 1.09768260f,
         0.22281852f, 1.10195160f, -0.12824026f, 1.21652830f, -0.83658582f},
        {64, 70, 190, 202, 319, 353, 369, 416, 500, 509},
        {-0.13300449f, -0.18829143f, -0.53223467f, -0.92887926f, -0.58281291f,
         0.53876561f, 0.65677041f, 0.42266384f, 0.47018227f, -1.22305155f},
        {28, 35, 80, 209, 244, 281, 341, 354, 466, 509},
        {1.12303531f, -0.25226226f, 0.38107660f, 0.23076940f, -0.26019636f,
         -0.81162584f, -0.63019812f, -0.15712912f, 0.93773609f, 0.21876933f},
        0.0f,
        {0.0f, 0.0f, 0.0f},
        4.40836525f,
        {40, 54, 91, 97, 110, 161, 168, 232, 252, 439, 444, 488},
        {0.26125208f, 0.04031458f, -0.17456096f, 0.22569288f, 0.05618671f,
         -0.64347184f, 0.59748763f, -0.20144501f, 0.15289156f, 0.01618028f,
         -0.06037596f, -0.08152162f},
    },
    {
        {5, 60, 89, 164, 170, 216, 277, 342, 395, 427, 459, 485},
        {-0.64674723f, 0.82945645f, 0.20305105f, 0.43644968f, -1.62537599f,
         1.08669233f, 0.02474966f, -0.52576971f, -1.13107765f, 0.50506991f,
         -0.10411493f, -0.78174174f},
        {37, 58, 132, 176, 226, 258, 299, 325, 349, 350, 414, 421},
        {0.49272391f, 0.45379484f, 1.78685677f, 0.90651369f, 1.08402681f,
         1.11432171f, 1.26932847f, -0.11193794f, -1.72804201f, -1.16241539f,
         0.34140852f, -0.34418300f},
        {4, 35, 96, 378, 397, 406, 427, 463, 500, 509},
        {0.70704770f, 0.58936745f, -0.81768590f, 0.94988358f, -1.19990528f,
         1.07753944f, 0.16640614f, 0.84829110f, -0.06731828f, -0.24779947f},
        {14, 64, 69, 152, 171, 262, 267, 364, 388, 449},
        {0.37310269f, -0.40083191f, 1.22919548f, -0.85150033f, 0.66562486f,
         -0.50712717f, -0.11600539f, 1.13038504f, 0.61403173f, 1.01784301f},
        {51, 70, 78, 79, 127, 151, 159, 286, 364, 391},
        {0.14809693f, 0.13727193f, 0.85409266f, -0.94183373f, -1.20937693f,
         0.84483594f, -0.92780805f, -0.14934354f, 1.07236779f, -0.24920268f},
        0.0f,
        {0.08404674f, 0.0f, 0.0f},
        2.75575471f,
        {5, 60, 89, 164, 170, 216, 277, 342, 395, 427, 459, 485},
        {-0.23468970f, 0.30099067f, 0.07368255f, 0.15837754f, -0.58981156f,
         0.39433566f, 0.00898108f, -0.19078973f, -0.41044205f, 0.18327825f,
         -0.03778091f, -0.28367609f},
    },
    {
        {20, 47, 95, 114, 130, 171, 264, 282, 292, 296, 326, 367},
        {0.62564635f, -0.37319478f, 0.22515312f, -0.15816587f, 2.72571349f,
         -0.89798772f, -0.02508186f, 0.12700216f, -0.28090945f, -1.40239537f,
         2.85455441f, -2.13435864f},
        {40, 68, 102, 149, 157, 174, 210, 247, 303, 366, 412, 454},
        {0.62514430f, -0.62695986f, 1.31579828f, 0.84713119f, 0.80921555f,
         0.94098043f, 1.47576952f, 0.83403194f, -2.46677542f, 0.06661674f,
         0.37910023f, -0.49856776f},
        {7, 35, 100, 216, 306, 307, 357, 415, 473, 511},
        {-0.49875277f, 1.16743505f, 0.24350767f, 1.21919668f, -0.54381245f,
         0.68055934f, 0.14748482f, -1.23900473f, 0.04134028f, -0.96278411f},
        {98, 225, 254, 268, 291, 300, 406, 438, 463, 481},
        {0.27015039f, 0.21393736f, -0.93328440f, -0.23507671f, -0.18463282f,
         -1.20063698f, 0.43167999f, 0.42476282f, -0.12636247f, -1.11282623f},
        {21, 102, 106, 132, 143, 232, 291, 296, 317, 478},
        {-1.11056149f, -0.17506622f, -0.65367430f, 0.08333009f, -0.26657155f,
         -1.05697179f, 1.21258914f, 0.72589171f, -0.96732038f, 0.14644974f},
        0.0f,
        {0.0f, 0.0f, -1.01798713f},
        4.85887480f,
        {20, 47, 95, 114, 130, 171, 264, 282, 292, 296, 326, 367},
        {0.12876363f, -0.07680684f, 0.04633853f, -0.03255196f, 0.56097627f,
         -0.18481393f, -0.00516207f, 0.02613818f, -0.05781368f, -0.28862554f,
         0.58749288f, -0.43927014f},
    },
};

}  // namespace

int main() {
  using sentriface::search::DotKernelBackend;
  using sentriface::search::Dot512;
  using sentriface::search::Dot512Batch;
  using sentriface::search::GetDotKernelInfo;
  using sentriface::search::Normalize512InPlace;
  using sentriface::search::kDotKernelDimension;

  float lhs[kDotKernelDimension];
  float rhs[kDotKernelDimension];
  float matrix[2U * kDotKernelDimension];
  float scores[2U];

  FillPatternCase(0U, lhs, rhs, kDotKernelDimension);
  for (std::size_t i = 0; i < kDotKernelDimension; ++i) {
    matrix[i] = 1.0f;
    matrix[kDotKernelDimension + i] = 2.0f;
  }

  if (!AlmostEqual(Dot512(lhs, rhs), 262656.0f, 1e-1f)) {
    return 1;
  }
  if (!AlmostEqual(Dot512(nullptr, rhs), 0.0f)) {
    return 5;
  }

  Dot512Batch(lhs, matrix, 2U, scores);
  if (!AlmostEqual(scores[0], 131328.0f, 1e-1f)) {
    return 2;
  }
  if (!AlmostEqual(scores[1], 262656.0f, 1e-1f)) {
    return 3;
  }

  for (std::size_t i = 0; i < kDotKernelDimension; ++i) {
    lhs[i] = 3.0f;
  }
  Normalize512InPlace(lhs);
  if (!AlmostEqual(Dot512(lhs, lhs), 1.0f, 1e-3f)) {
    return 4;
  }

  float zeros[kDotKernelDimension] = {};
  Normalize512InPlace(zeros);
  if (!AlmostEqual(Dot512(zeros, zeros), 0.0f)) {
    return 6;
  }

  const auto info = GetDotKernelInfo();
  if (info.implementation == nullptr || info.lanes == 0U ||
      !info.normalized_cosine_ready) {
    return 7;
  }
  switch (info.backend) {
    case DotKernelBackend::kScalar:
      if (info.lanes != 1U) {
        return 8;
      }
      break;
    case DotKernelBackend::kSse2:
    case DotKernelBackend::kNeon:
      if (info.lanes != 4U) {
        return 9;
      }
      break;
    case DotKernelBackend::kAvx2:
      if (info.lanes != 8U) {
        return 10;
      }
      break;
  }

  constexpr std::size_t kOracleCaseCount = 6U;
  std::vector<float> lhs_ref(kDotKernelDimension, 0.0f);
  std::vector<float> rhs_ref(kDotKernelDimension, 0.0f);
  std::vector<float> batch_matrix(3U * kDotKernelDimension, 0.0f);
  std::vector<float> batch_scores(3U, 0.0f);
  std::vector<float> row_lhs(kDotKernelDimension, 0.0f);
  std::vector<float> row_rhs(kDotKernelDimension, 0.0f);

  for (std::size_t case_index = 0; case_index < kOracleCaseCount; ++case_index) {
    FillPatternCase(case_index, lhs_ref.data(), rhs_ref.data(), kDotKernelDimension);

    const float scalar_dot =
        DotScalarReference(lhs_ref.data(), rhs_ref.data(), kDotKernelDimension);
    const float kernel_dot = Dot512(lhs_ref.data(), rhs_ref.data());
    if (!AlmostEqualScaled(kernel_dot, scalar_dot)) {
      return 11;
    }

    FillPatternCase(case_index, row_lhs.data(), row_rhs.data(), kDotKernelDimension);
    std::copy(row_lhs.begin(), row_lhs.end(), batch_matrix.begin());

    FillPatternCase((case_index + 1U) % kOracleCaseCount, row_lhs.data(), row_rhs.data(),
                    kDotKernelDimension);
    std::copy(row_lhs.begin(), row_lhs.end(),
              batch_matrix.begin() + kDotKernelDimension);

    FillPatternCase((case_index + 2U) % kOracleCaseCount, row_lhs.data(), row_rhs.data(),
                    kDotKernelDimension);
    std::copy(row_lhs.begin(), row_lhs.end(),
              batch_matrix.begin() + (2U * kDotKernelDimension));

    Dot512Batch(lhs_ref.data(), batch_matrix.data(), 3U, batch_scores.data());
    for (std::size_t row = 0; row < 3U; ++row) {
      const float scalar_batch = DotScalarReference(
          lhs_ref.data(), batch_matrix.data() + (row * kDotKernelDimension),
          kDotKernelDimension);
      if (!AlmostEqualScaled(batch_scores[row], scalar_batch)) {
        return 12;
      }
    }

    std::vector<float> normalized_kernel = lhs_ref;
    std::vector<float> normalized_scalar = lhs_ref;
    Normalize512InPlace(normalized_kernel.data());
    NormalizeScalarReference(&normalized_scalar);
    for (std::size_t i = 0; i < kDotKernelDimension; ++i) {
      if (!AlmostEqual(normalized_kernel[i], normalized_scalar[i], 1e-4f)) {
        return 13;
      }
    }
  }

  for (const auto& test_case : kNumpySparseCases) {
    std::vector<float> dense_lhs(kDotKernelDimension, 0.0f);
    std::vector<float> dense_rhs(kDotKernelDimension, 0.0f);
    std::vector<float> dense_matrix(3U * kDotKernelDimension, 0.0f);
    std::vector<float> batch_out(3U, 0.0f);
    std::vector<float> normalized = dense_lhs;

    ScatterToDense(test_case.lhs_idx, test_case.lhs_vals, dense_lhs.data(),
                   kDotKernelDimension);
    ScatterToDense(test_case.rhs_idx, test_case.rhs_vals, dense_rhs.data(),
                   kDotKernelDimension);
    ScatterToDense(test_case.matrix0_idx, test_case.matrix0_vals,
                   dense_matrix.data(), kDotKernelDimension);
    ScatterToDense(test_case.matrix1_idx, test_case.matrix1_vals,
                   dense_matrix.data() + kDotKernelDimension, kDotKernelDimension);
    ScatterToDense(test_case.matrix2_idx, test_case.matrix2_vals,
                   dense_matrix.data() + (2U * kDotKernelDimension),
                   kDotKernelDimension);

    if (!AlmostEqualScaled(Dot512(dense_lhs.data(), dense_rhs.data()),
                           test_case.expected_dot, 1e-5f, 2e-6f)) {
      return 14;
    }

    Dot512Batch(dense_lhs.data(), dense_matrix.data(), 3U, batch_out.data());
    for (std::size_t row = 0; row < 3U; ++row) {
      if (!AlmostEqualScaled(batch_out[row], test_case.expected_batch[row], 1e-5f,
                             2e-6f)) {
        return 15;
      }
    }

    normalized = dense_lhs;
    Normalize512InPlace(normalized.data());
    if (!AlmostEqualScaled(std::sqrt(Dot512(dense_lhs.data(), dense_lhs.data())),
                           test_case.expected_norm, 1e-5f, 2e-6f)) {
      return 16;
    }
    if (!AlmostEqualScaled(Dot512(normalized.data(), normalized.data()), 1.0f, 1e-5f,
                           2e-6f)) {
      return 17;
    }

    for (std::size_t i = 0; i < kDotKernelDimension; ++i) {
      bool is_expected_nonzero = false;
      for (int expected_index : test_case.expected_normalized_idx) {
        if (static_cast<std::size_t>(expected_index) == i) {
          is_expected_nonzero = true;
          break;
        }
      }
      if (!is_expected_nonzero && normalized[i] != 0.0f) {
        return 18;
      }
    }

    for (std::size_t i = 0; i < test_case.expected_normalized_idx.size(); ++i) {
      const std::size_t index =
          static_cast<std::size_t>(test_case.expected_normalized_idx[i]);
      if (!AlmostEqualScaled(normalized[index],
                             test_case.expected_normalized_vals[i], 1e-5f, 2e-6f)) {
        return 19;
      }
    }
  }

  return 0;
}
