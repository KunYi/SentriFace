#include "tracker/kalman_filter.hpp"

#include <algorithm>
#include <cmath>

namespace sentriface::tracker {
namespace {

constexpr int kStateDim = 8;
constexpr int kMeasDim = 4;

using StateVector = std::array<float, kStateDim>;
using StateMatrix = std::array<float, kStateDim * kStateDim>;
using MeasVector = std::array<float, kMeasDim>;
using MeasMatrix = std::array<float, kMeasDim * kMeasDim>;
using GainMatrix = std::array<float, kStateDim * kMeasDim>;

float& M8(StateMatrix& m, int r, int c) { return m[r * kStateDim + c]; }
float M8(const StateMatrix& m, int r, int c) { return m[r * kStateDim + c]; }

float& M4(MeasMatrix& m, int r, int c) { return m[r * kMeasDim + c]; }
float M4(const MeasMatrix& m, int r, int c) { return m[r * kMeasDim + c]; }

float& K84(GainMatrix& m, int r, int c) { return m[r * kMeasDim + c]; }

StateMatrix MakeIdentity8() {
  StateMatrix out {};
  for (int i = 0; i < kStateDim; ++i) {
    M8(out, i, i) = 1.0f;
  }
  return out;
}

StateMatrix Multiply8(const StateMatrix& a, const StateMatrix& b) {
  StateMatrix out {};
  for (int r = 0; r < kStateDim; ++r) {
    for (int c = 0; c < kStateDim; ++c) {
      float sum = 0.0f;
      for (int k = 0; k < kStateDim; ++k) {
        sum += M8(a, r, k) * M8(b, k, c);
      }
      M8(out, r, c) = sum;
    }
  }
  return out;
}

StateMatrix Transpose8(const StateMatrix& m) {
  StateMatrix out {};
  for (int r = 0; r < kStateDim; ++r) {
    for (int c = 0; c < kStateDim; ++c) {
      M8(out, c, r) = M8(m, r, c);
    }
  }
  return out;
}

StateVector Multiply8x1(const StateMatrix& m, const StateVector& x) {
  StateVector out {};
  for (int r = 0; r < kStateDim; ++r) {
    float sum = 0.0f;
    for (int c = 0; c < kStateDim; ++c) {
      sum += M8(m, r, c) * x[c];
    }
    out[r] = sum;
  }
  return out;
}

MeasMatrix Invert4x4(const MeasMatrix& in) {
  float aug[kMeasDim][kMeasDim * 2] {};
  for (int r = 0; r < kMeasDim; ++r) {
    for (int c = 0; c < kMeasDim; ++c) {
      aug[r][c] = M4(in, r, c);
    }
    aug[r][kMeasDim + r] = 1.0f;
  }

  for (int col = 0; col < kMeasDim; ++col) {
    int pivot = col;
    float max_abs = std::fabs(aug[col][col]);
    for (int r = col + 1; r < kMeasDim; ++r) {
      const float value = std::fabs(aug[r][col]);
      if (value > max_abs) {
        max_abs = value;
        pivot = r;
      }
    }

    if (max_abs < 1e-6f) {
      MeasMatrix fallback {};
      for (int i = 0; i < kMeasDim; ++i) {
        M4(fallback, i, i) = 1.0f;
      }
      return fallback;
    }

    if (pivot != col) {
      for (int c = 0; c < kMeasDim * 2; ++c) {
        std::swap(aug[pivot][c], aug[col][c]);
      }
    }

    const float pivot_value = aug[col][col];
    for (int c = 0; c < kMeasDim * 2; ++c) {
      aug[col][c] /= pivot_value;
    }

    for (int r = 0; r < kMeasDim; ++r) {
      if (r == col) {
        continue;
      }
      const float factor = aug[r][col];
      for (int c = 0; c < kMeasDim * 2; ++c) {
        aug[r][c] -= factor * aug[col][c];
      }
    }
  }

  MeasMatrix out {};
  for (int r = 0; r < kMeasDim; ++r) {
    for (int c = 0; c < kMeasDim; ++c) {
      M4(out, r, c) = aug[r][kMeasDim + c];
    }
  }
  return out;
}

StateMatrix MakeTransition(float dt) {
  StateMatrix f = MakeIdentity8();
  M8(f, 0, 4) = dt;
  M8(f, 1, 5) = dt;
  M8(f, 2, 6) = dt;
  M8(f, 3, 7) = dt;
  return f;
}

StateMatrix MakeProcessNoise(const TrackerConfig& config) {
  StateMatrix q {};
  M8(q, 0, 0) = config.q_pos;
  M8(q, 1, 1) = config.q_pos;
  M8(q, 2, 2) = config.q_size;
  M8(q, 3, 3) = config.q_size;
  M8(q, 4, 4) = config.q_vel;
  M8(q, 5, 5) = config.q_vel;
  M8(q, 6, 6) = config.q_vsize;
  M8(q, 7, 7) = config.q_vsize;
  return q;
}

MeasMatrix MakeMeasurementNoise(const TrackerConfig& config) {
  MeasMatrix r {};
  M4(r, 0, 0) = config.r_pos;
  M4(r, 1, 1) = config.r_pos;
  M4(r, 2, 2) = config.r_size;
  M4(r, 3, 3) = config.r_size;
  return r;
}

}  // namespace

KalmanFilter::KalmanFilter(const TrackerConfig& config) : config_(config) {}

void KalmanFilter::Initialize(std::array<float, 8>& x, std::array<float, 64>& p,
                              float cx, float cy, float w, float h) const {
  x = {cx, cy, w, h, 0.0f, 0.0f, 0.0f, 0.0f};
  p.fill(0.0f);
  for (int i = 0; i < 4; ++i) {
    p[i * 8 + i] = 25.0f;
  }
  for (int i = 4; i < 8; ++i) {
    p[i * 8 + i] = 100.0f;
  }
}

void KalmanFilter::Predict(std::array<float, 8>& x, std::array<float, 64>& p,
                           float dt) const {
  const float dt_clamped = std::max(0.0f, dt);
  const StateMatrix f = MakeTransition(dt_clamped);
  const StateMatrix ft = Transpose8(f);
  const StateMatrix q = MakeProcessNoise(config_);

  x = Multiply8x1(f, x);
  x[2] = std::max(1.0f, x[2]);
  x[3] = std::max(1.0f, x[3]);

  const StateMatrix fp = Multiply8(f, p);
  const StateMatrix fpf_t = Multiply8(fp, ft);
  for (int r = 0; r < kStateDim; ++r) {
    for (int c = 0; c < kStateDim; ++c) {
      M8(p, r, c) = M8(fpf_t, r, c) + M8(q, r, c);
    }
  }
}

void KalmanFilter::Update(std::array<float, 8>& x, std::array<float, 64>& p,
                          float cx, float cy, float w, float h) const {
  const MeasVector z {cx, cy, w, h};
  const MeasVector y {
      z[0] - x[0],
      z[1] - x[1],
      z[2] - x[2],
      z[3] - x[3],
  };

  MeasMatrix s {};
  const MeasMatrix r = MakeMeasurementNoise(config_);
  for (int row = 0; row < kMeasDim; ++row) {
    for (int col = 0; col < kMeasDim; ++col) {
      M4(s, row, col) = M8(p, row, col) + M4(r, row, col);
    }
  }
  const MeasMatrix s_inv = Invert4x4(s);

  GainMatrix k {};
  for (int row = 0; row < kStateDim; ++row) {
    for (int col = 0; col < kMeasDim; ++col) {
      float sum = 0.0f;
      for (int t = 0; t < kMeasDim; ++t) {
        sum += M8(p, row, t) * M4(s_inv, t, col);
      }
      K84(k, row, col) = sum;
    }
  }

  for (int row = 0; row < kStateDim; ++row) {
    float delta = 0.0f;
    for (int col = 0; col < kMeasDim; ++col) {
      delta += K84(k, row, col) * y[col];
    }
    x[row] += delta;
  }
  x[2] = std::max(1.0f, x[2]);
  x[3] = std::max(1.0f, x[3]);

  StateMatrix kh {};
  for (int row = 0; row < kStateDim; ++row) {
    for (int col = 0; col < kStateDim; ++col) {
      if (col < kMeasDim) {
        M8(kh, row, col) = K84(k, row, col);
      }
    }
  }

  const StateMatrix i = MakeIdentity8();
  StateMatrix i_minus_kh {};
  for (int row = 0; row < kStateDim; ++row) {
    for (int col = 0; col < kStateDim; ++col) {
      M8(i_minus_kh, row, col) = M8(i, row, col) - M8(kh, row, col);
    }
  }

  const StateMatrix temp = Multiply8(i_minus_kh, p);
  const StateMatrix i_minus_kh_t = Transpose8(i_minus_kh);
  StateMatrix new_p = Multiply8(temp, i_minus_kh_t);

  StateMatrix krkt {};
  for (int row = 0; row < kStateDim; ++row) {
    for (int col = 0; col < kStateDim; ++col) {
      float sum = 0.0f;
      for (int a = 0; a < kMeasDim; ++a) {
        for (int b = 0; b < kMeasDim; ++b) {
          sum += K84(k, row, a) * M4(r, a, b) * K84(k, col, b);
        }
      }
      M8(krkt, row, col) = sum;
    }
  }

  for (int row = 0; row < kStateDim; ++row) {
    for (int col = 0; col < kStateDim; ++col) {
      M8(p, row, col) = M8(new_p, row, col) + M8(krkt, row, col);
    }
  }
}

}  // namespace sentriface::tracker
