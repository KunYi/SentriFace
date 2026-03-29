#include "search/dot_kernel.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

struct BenchmarkConfig {
  std::size_t prototypes = 1000U;
  std::size_t iterations = 200U;
  std::uint32_t seed = 20260328U;
};

void PrintUsage() {
  std::cout
      << "Usage: ./build/dot_kernel_benchmark_runner [prototypes] [iterations] [seed]\n"
      << "Example: ./build/dot_kernel_benchmark_runner\n"
      << "Example: ./build/dot_kernel_benchmark_runner 5000 300 20260328\n";
}

bool ParseSizeArg(const char* text, std::size_t* out_value) {
  if (text == nullptr || out_value == nullptr) {
    return false;
  }
  char* end = nullptr;
  const unsigned long long value = std::strtoull(text, &end, 10);
  if (end == text || (end != nullptr && *end != '\0')) {
    return false;
  }
  *out_value = static_cast<std::size_t>(value);
  return true;
}

bool ParseSeedArg(const char* text, std::uint32_t* out_value) {
  if (text == nullptr || out_value == nullptr) {
    return false;
  }
  char* end = nullptr;
  const unsigned long value = std::strtoul(text, &end, 10);
  if (end == text || (end != nullptr && *end != '\0')) {
    return false;
  }
  *out_value = static_cast<std::uint32_t>(value);
  return true;
}

bool ParseArgs(int argc, char** argv, BenchmarkConfig* out_config) {
  if (out_config == nullptr) {
    return false;
  }
  if (argc >= 2 && std::string(argv[1]) == "--help") {
    PrintUsage();
    return false;
  }

  BenchmarkConfig config;
  if (argc >= 2 && !ParseSizeArg(argv[1], &config.prototypes)) {
    return false;
  }
  if (argc >= 3 && !ParseSizeArg(argv[2], &config.iterations)) {
    return false;
  }
  if (argc >= 4 && !ParseSeedArg(argv[3], &config.seed)) {
    return false;
  }
  if (argc > 4 || config.prototypes == 0U || config.iterations == 0U) {
    return false;
  }

  *out_config = config;
  return true;
}

void FillRandomVector(std::mt19937* rng, float* out_values, std::size_t count) {
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  for (std::size_t i = 0; i < count; ++i) {
    out_values[i] = dist(*rng);
  }
}

double RunDotBenchmark(const float* lhs,
                       const float* rhs,
                       std::size_t iterations,
                       float* out_accumulator) {
  const auto start = std::chrono::steady_clock::now();
  float accumulator = 0.0f;
  for (std::size_t i = 0; i < iterations; ++i) {
    accumulator += sentriface::search::Dot512(lhs, rhs);
  }
  const auto end = std::chrono::steady_clock::now();
  *out_accumulator = accumulator;
  return std::chrono::duration<double>(end - start).count();
}

double RunBatchBenchmark(const float* query,
                         const float* matrix,
                         std::size_t prototypes,
                         std::size_t iterations,
                         float* out_accumulator) {
  std::vector<float> scores(prototypes, 0.0f);
  const auto start = std::chrono::steady_clock::now();
  float accumulator = 0.0f;
  for (std::size_t i = 0; i < iterations; ++i) {
    sentriface::search::Dot512Batch(query, matrix, prototypes, scores.data());
    accumulator += scores[i % prototypes];
  }
  const auto end = std::chrono::steady_clock::now();
  *out_accumulator = accumulator;
  return std::chrono::duration<double>(end - start).count();
}

}  // namespace

int main(int argc, char** argv) {
  BenchmarkConfig config;
  if (!ParseArgs(argc, argv, &config)) {
    PrintUsage();
    return 1;
  }

  constexpr std::size_t kDim = sentriface::search::kDotKernelDimension;
  std::mt19937 rng(config.seed);
  std::vector<float> lhs(kDim, 0.0f);
  std::vector<float> rhs(kDim, 0.0f);
  std::vector<float> query(kDim, 0.0f);
  std::vector<float> matrix(config.prototypes * kDim, 0.0f);

  FillRandomVector(&rng, lhs.data(), kDim);
  FillRandomVector(&rng, rhs.data(), kDim);
  FillRandomVector(&rng, query.data(), kDim);
  FillRandomVector(&rng, matrix.data(), matrix.size());

  sentriface::search::Normalize512InPlace(lhs.data());
  sentriface::search::Normalize512InPlace(rhs.data());
  sentriface::search::Normalize512InPlace(query.data());
  for (std::size_t row = 0; row < config.prototypes; ++row) {
    sentriface::search::Normalize512InPlace(matrix.data() + (row * kDim));
  }

  const auto info = sentriface::search::GetDotKernelInfo();

  float dot_accumulator = 0.0f;
  const double dot_seconds =
      RunDotBenchmark(lhs.data(), rhs.data(), config.iterations, &dot_accumulator);

  float batch_accumulator = 0.0f;
  const double batch_seconds = RunBatchBenchmark(query.data(), matrix.data(),
                                                 config.prototypes,
                                                 config.iterations,
                                                 &batch_accumulator);

  const double dot_ops_per_sec =
      static_cast<double>(config.iterations) / dot_seconds;
  const double batch_ops_per_sec =
      static_cast<double>(config.iterations * config.prototypes) / batch_seconds;
  const double avg_batch_latency_ms =
      (batch_seconds * 1000.0) / static_cast<double>(config.iterations);

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "backend=" << info.implementation << '\n';
  std::cout << "lanes=" << info.lanes << '\n';
  std::cout << "prototypes=" << config.prototypes << '\n';
  std::cout << "iterations=" << config.iterations << '\n';
  std::cout << "seed=" << config.seed << '\n';
  std::cout << "dot_seconds=" << dot_seconds << '\n';
  std::cout << "dot_ops_per_sec=" << dot_ops_per_sec << '\n';
  std::cout << "batch_seconds=" << batch_seconds << '\n';
  std::cout << "batch_ops_per_sec=" << batch_ops_per_sec << '\n';
  std::cout << "avg_batch_latency_ms=" << avg_batch_latency_ms << '\n';
  std::cout << "dot_accumulator=" << dot_accumulator << '\n';
  std::cout << "batch_accumulator=" << batch_accumulator << '\n';

  return 0;
}
