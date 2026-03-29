# Dot Kernel Strategy

This file is for readers who already want the implementation-facing view of the
search hot path.

If you want to understand the math and decision intuition first, start with:

- [embedding_search_intuition.md](embedding_search_intuition.md)

If the formulas and score semantics still feel unfamiliar, do not start here.
This file assumes the geometric intuition is already in place.

Then come back here for:

- kernel design
- data layout
- normalization contract
- package-first runtime strategy

## Scope

The search hot path is a fixed `512`-dimensional float dot product.
This is the core similarity operation for enrollment baselines and `1:N`
identification search.

The current goal is not ANN indexing.
The goal is to make the local exact-search path predictable, reproducible, and
efficient for the current `200 ~ 500 persons` local-first baseline.

For geometric intuition and decision-facing explanations, see:

- [embedding_search_intuition.md](embedding_search_intuition.md)

## Goals

- Keep the API small and explicit.
- Optimize the fixed `512-d` hot path first.
- Support both x86 and ARMv7 Cortex-A7 efficiently.
- Preserve a scalar fallback for portability and tests.
- Keep the search layer simple enough to remain `RV1106` friendly.
- Preserve deterministic replay and regression behavior.

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
  - Fall back to scalar only when SIMD is unavailable or the compiler cannot
    expose the SIMD path safely.
- ARMv7 Cortex-A7:
  - Use `NEON` intrinsics.
  - Backend selection is compile-target driven rather than runtime dispatched.
  - If the cross build does not enable NEON, the scalar backend remains the
    safety fallback.
- Portable fallback:
  - Scalar implementation for all builds.

## Search Usage

- Normalize `512-d` prototypes once when they enter the search index.
- Normalize the query vector once per search.
- Store prototypes as a flat contiguous `count x 512` matrix.
- Use `Dot512Batch(...)` for exact search scoring.
- Keep person-level aggregation outside the kernel.

This means `FaceSearch` and `FaceSearchV2` should treat the `512-d` path as:

1. import prototype data
2. keep lightweight metadata for ranking and debug
3. copy embeddings into a contiguous matrix
4. normalize prototypes once
5. normalize each query once
6. score the query against the normalized matrix
7. apply weighted or person-level logic outside the kernel

For non-`512-d` embeddings, the generic cosine path remains valid.

The important architectural point is that this section describes the mainline
runtime shape, not every possible compatibility or import path.

The system may still support:

- prototype rebuild helpers
- import-time compatibility conversions
- tooling paths that construct the index from higher-level objects

But the hot path should remain conceptually simple:

- one normalized query
- one normalized contiguous matrix
- one batch dot-product sweep
- metadata interpretation outside the kernel

## Matrix-First Runtime Layout

The runtime hot path is fundamentally:

- one query vector
- against many stored prototype vectors

That is why the preferred layout is:

- a contiguous normalized `count x 512` matrix
- plus lightweight parallel metadata arrays

This layout is preferred because it is:

- more cache-friendly
- more batch-friendly
- more SIMD-friendly
- easier to package directly
- easier to reason about in replay and regression tests

The matrix stores numeric hot-path data.
Metadata such as `person_id`, `prototype_id`, zone, and label should remain
outside the numeric matrix and stay aligned by row index.

This separation is worth emphasizing because it prevents two different concerns
from getting tangled together:

- numeric scoring efficiency
- human-meaningful metadata interpretation

The kernel wants:

- dense float arrays
- predictable row traversal
- low overhead inside the scoring loop

Debugging and decision code want:

- labels
- IDs
- zone and weighting context

Keeping these concerns separate usually produces a cleaner and faster design
than trying to treat every row as a rich object inside the hot loop.

## Normalization Contract

Normalization is not a cosmetic preprocessing step.
It is part of the mathematical contract of the score.

For the intended hot path, the score semantics are:

- normalized query dot normalized prototype

That means:

- prototypes should be normalized at import or package-build time
- queries should be normalized once at search time

If one side is normalized and the other is not, the score is no longer pure
cosine-style similarity.
Thresholds, margins, replay interpretation, and debug conclusions then become
harder to trust.

This is why normalization should be viewed as part of the score definition, not
just a utility step.

If the intended semantics are:

- cosine-style similarity on normalized embeddings

then that expectation has to be protected across:

- import
- package build
- runtime load
- query preprocessing
- replay tooling
- tests

Without that consistency, people may talk about the score as though it means one
thing while the implementation is actually computing something slightly
different.

## Why Prototype Normalization Happens Earlier

Prototypes are long-lived and reused across many searches.
Queries are short-lived and generated per search request.

So the preferred timing is:

- normalize prototypes once when building or loading the index
- normalize the query once when that query is created

This keeps the runtime hot loop focused on:

- multiply
- add
- accumulate

rather than repeatedly renormalizing stored data.

This timing difference is asymmetric in wall-clock time but symmetric in score
meaning.

The system still wants:

- normalized query dot normalized prototype

It simply reaches that state by different routes:

- prototypes are prepared early because they are reused
- queries are prepared late because they do not exist until search time

That is a useful design distinction, especially for readers who wonder why the
two sides are not normalized at the same moment.

## Package-First Search

The runtime mainline should prefer loading an explicit search-ready package
rather than rebuilding search state ad hoc from miscellaneous inputs.

In this project, that maps to boundaries such as:

- `.sfbp` for baseline prototype package
- `.sfsi` for search-ready index package

Conceptually:

- `.sfbp` represents structured prototype data
- `.sfsi` represents search-ready normalized runtime state

The package-first path matters because it protects:

- normalization semantics
- runtime simplicity
- reproducibility
- direct-load performance
- clear separation between build/import logic and runtime search

Compatibility helpers such as rebuild or import paths are still useful, but they
should not replace the direct-load runtime mental model.

The package-first idea also helps narrow down debugging questions.

If runtime is expected to load a search-ready package, then the team can ask:

- Was the package built correctly?
- Was the normalized matrix preserved correctly?
- Did runtime load the expected package?

Those are cleaner questions than a design where each caller implicitly rebuilds
the search state in its own way.

Package-first design therefore improves not only runtime efficiency, but also
architectural clarity and operational reproducibility.

## Deterministic Tie-Break

Equal-score candidates should use a deterministic secondary ordering rule after
score comparison.

This matters for:

- replay stability
- regression clarity
- backend comparability
- logging consistency

The exact fallback rule may vary, but it should be:

- explicit
- documented
- applied consistently

Tie-break policy is not a semantic claim that the fallback field is "more
correct".
It is an engineering choice for stable and reproducible outputs.

This matters because equal-score instability can create noise that looks like a
real algorithm change even when the underlying score set stayed the same.

Stable tie-break rules help preserve trust in:

- replay diffs
- regression comparisons
- backend comparisons
- log interpretation

That makes them part of engineering quality, not just sort-order housekeeping.

## Why Exact Search Comes Before ANN

For the current local-first target of roughly `200 ~ 500` persons, flat exact
search with SIMD is the default.

This is preferred first because it offers:

- clearer semantics
- easier debugging
- easier replay analysis
- fewer moving parts
- simpler embedded deployment

ANN remains a later optimization if corpus size or latency requirements grow
beyond what the exact path can comfortably support.

Another reason to prefer exact search first is that it provides a clean
reference baseline.

If the system later experiments with ANN, the exact path can answer:

- what the true nearest candidates were
- how much quality approximation changed
- whether the added complexity is actually buying something worthwhile

Without a trusted exact baseline, ANN evaluation becomes much harder to judge.

## Indexing Note

At the current scale, exact search with a strong `512-d` dot kernel is the
mainline design.

If future scale moves substantially beyond the current local-first target, ANN
can be evaluated on top of the same normalization and packaging foundations.

## Related Reading

For mathematical and system-level intuition, see:

- [embedding_search_intuition.md](embedding_search_intuition.md)

Especially relevant sections include:

- `From x, y Distance to Dot Score`
- `Cosine Similarity vs Euclidean Distance`
- `Thresholds, Margins, and Decision Intuition`
- `Why the Best Match Is Not Always Safe Enough to Unlock`
- `How Enrollment Quality Shapes the Whole Geometry`
- `Why Search Scores Are Useful, but Not Directly Probabilities`
- `Why Open-Set Rejection Is One of the Hardest Parts`
- `Why Replay Matters More Than Intuition`

Readers who want the "why" behind these engineering choices should usually start
with [embedding_search_intuition.md](embedding_search_intuition.md) first, then
come back here for the runtime-oriented strategy.

## Build Notes

- Do not rely on old machine-specific absolute paths.
- Native x86 validation should use normal repo-relative CMake build directories.
- ARMv7 validation should use a cross toolchain or sysroot that targets the
  actual board ABI.
- The NEON backend is only expected when the target triple and compiler flags
  really describe an `armv7-a + neon` style target.

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

- Unit tests cover the `512-d` dot kernel.
- Unit tests cover normalized `512-d` search and weighted search v2 on top of
  the kernel path.
- x86 builds should report `avx2` or `sse2` through `GetDotKernelInfo()`
  depending on the host CPU.
- ARMv7 builds should report `neon` when cross compiled for NEON.
- ARMv7 builds can be exercised through cross build and QEMU execution when that
  environment is available.
- Repo-side microbenchmark entry:
  - `./build/dot_kernel_benchmark_runner`
  - `tools/run_dot_kernel_benchmark.sh`

Validation should not stop at "the code runs".
For this strategy, the most important properties to preserve are:

- score semantics remain consistent across paths
- scalar and SIMD results remain aligned
- direct-load packages preserve normalized layout and row ordering
- tie-break behavior remains deterministic
- replay outputs remain interpretable after optimizations

That is the difference between a fast kernel and a dependable search kernel.
