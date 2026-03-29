# Dot Kernel Strategy

## Scope

The search hot path is a fixed 512-dimensional float dot product.
This is the core similarity operation for enrollment baselines and 1:N search.
The first production-facing goal is not ANN indexing. It is to make the local
exact-search path predictable and efficient for the current `200 ~ 500 persons`
local-first baseline.

## Goals

- Keep the API small and explicit.
- Optimize the fixed 512-d hot path first.
- Support both x86 and ARMv7 Cortex-A7 efficiently.
- Preserve a scalar fallback for portability and tests.
- Keep the search layer simple enough to remain `RV1106` friendly.

## Kernel API

- `Dot512(const float* lhs, const float* rhs)`
- `Dot512Batch(const float* query, const float* matrix, std::size_t count, float* out_scores)`
- `Normalize512InPlace(float* values)`
- `GetDotKernelInfo()`

## Backend Plan

- Intel / x86:
  - Resolve backend at runtime.
  - Prefer `AVX2` when the CPU supports it.
  - Fall back to `SSE2`.
  - Fall back to scalar only when SIMD is unavailable or the compiler cannot expose the SIMD path safely.
- ARMv7 Cortex-A7:
  - Use `NEON` intrinsics.
  - Backend selection is compile-target driven rather than runtime dispatched.
  - If the cross build does not enable NEON, the scalar backend remains the safety fallback.
- Portable fallback:
  - Scalar implementation for all builds.

## Search Usage

- Normalize 512-d prototypes once when they enter the search index.
- Normalize the query vector once per search.
- Store prototypes as a flat contiguous `count x 512` matrix.
- Use `Dot512Batch(...)` for exact search scoring.
- Keep person-level aggregation outside the kernel.

This means `FaceSearch` and `FaceSearchV2` should treat the 512-d path as:

1. import prototype
2. keep lightweight metadata for ranking / debug
3. copy embedding to contiguous matrix
4. normalize once
5. score query against the normalized matrix
6. apply weighted/person-level logic outside the kernel

To stay friendlier to embedded memory limits, the 512-d fast path should avoid
keeping a second long-lived full embedding copy when the normalized matrix is
already available.

For replay / regression stability, equal-score candidates should also use a
deterministic tie-break rule after score comparison.

For non-512-d embeddings, the generic cosine path remains valid.

## Indexing Note

For the current local-first target of roughly 200 to 500 persons, flat exact search with SIMD is the default.
ANN indexing is a later optimization if latency or corpus size grows.

## Build Notes

- Do not rely on old machine-specific absolute paths.
- Native x86 validation should use normal repo-relative CMake build directories.
- ARMv7 validation should use a cross toolchain / sysroot that targets the actual board ABI.
- The NEON backend is only expected when the target triple and compiler flags really describe an `armv7-a + neon` style target.

Example native configure:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Example ARMv7 compile intent:

```bash
cmake -S . -B build-armv7 \
  -DCMAKE_TOOLCHAIN_FILE=<toolchain-file>
cmake --build build-armv7
```

## Validation

- Unit tests cover the 512-d dot kernel.
- Unit tests cover normalized 512-d search and weighted search v2 on top of the kernel path.
- x86 builds should report `avx2` or `sse2` through `GetDotKernelInfo()` depending on the host CPU.
- ARMv7 builds should report `neon` when cross compiled for NEON.
- ARMv7 builds can be exercised through cross build and QEMU execution when that environment is available.
- Repo-side microbenchmark entry:
  - `./build/dot_kernel_benchmark_runner`
  - `tools/run_dot_kernel_benchmark.sh`
