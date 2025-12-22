# Appendix A — Tools & Technology Stack (with rationale)

## Purpose

This appendix specifies the recommended tools and technology stack for implementing, building, testing, packaging, and releasing the benchmark suite. It provides rationale aligned to cross-platform constraints, reproducibility, and maintainability.

## A.1 Implementation Language and Standard Library

1. The implementation **MUST** be in **C++** to maximize compatibility with C/C++ rendering libraries and enable low-overhead benchmarking.
2. The project **MUST** target **C++20** as the minimum language standard to:

   * simplify cross-platform abstractions,
   * enable safer resource management patterns,
   * support modern build and tooling ecosystems.
3. The C++ standard library **MUST** be the default foundational runtime; additional runtime frameworks are optional and must be justified.

## A.2 Build System

1. The build system **MUST** be **CMake** (as specified in project constraints).
2. CMake **MUST** be the single build entry point to:

   * configure and build the suite,
   * enable/disable adapters,
   * orchestrate dependencies,
   * produce installable artifacts and packaging outputs.
3. The project **SHOULD** use CMake presets to standardize developer, CI, and release builds.

**Rationale**: CMake is the most interoperable cross-platform build system for C/C++ projects and integrates with diverse third-party libraries while supporting multi-config generators on Windows.

## A.3 Benchmarking Framework

1. The harness **SHOULD** use a standard microbenchmark framework compatible with C++ and cross-platform execution.
2. **Google Benchmark** is the default recommendation.

**Rationale**: Provides repetitions, iteration scaling, and standard output patterns; reduces risk of biased measurement logic.

## A.4 Serialization and Output Libraries

1. JSON output **MUST** be supported; CSV output **MUST** be supported.
2. The project **SHOULD** use a lightweight, widely available JSON library that:

   * supports deterministic field ordering where needed (or the project imposes deterministic ordering),
   * has a permissive license and broad compiler support.

Examples (non-normative): nlohmann/json, RapidJSON.

3. CSV output **SHOULD** be implemented in project code (simple writer) to avoid dependency weight.

**Rationale**: Output is part of the reproducibility contract; dependencies should be minimal and stable.

## A.5 Hashing and Cryptography

1. SceneHash **MUST** be SHA-256 (Chapter 5).
2. The project **SHOULD** use a well-maintained cryptographic implementation suitable for SHA-256.

Options (non-normative):

* a small embedded SHA-256 implementation with clear licensing,
* a common crypto library already required by other dependencies (only if it does not complicate portability).

**Rationale**: Hashing must be deterministic and trustworthy; SHA-256 is standard and widely supported.

## A.6 Platform Abstraction and System Introspection

1. The PAL **MUST** use OS-native APIs for:

   * monotonic wall-clock timing,
   * CPU-time measurement,
   * system metadata (CPU model, memory).
2. Where cross-platform libraries reduce risk without adding heavy dependencies, the project **MAY** use them, but OS-native APIs remain preferred for timing correctness.

**Rationale**: Timing accuracy is core to benchmark credibility; OS-native APIs are best for resolution and semantics.

## A.7 Dependency Management Approach

## A.7 Dependency Management Approach

### A.7.1 Pinned Versions Table

The project **MUST** defaults to the following versions for reproducible baselines (overridable by CMake options for testing newer builds):

| Library | Version / Commit | Notes |
|---|---|---|
| **Blend2D** | `beta-1` (or latest stable) | Static link preferred |
| **Skia** | `m116` (chrome/116 branch) | Use minimal build flags (no GPU, no PDF, no text) |
| **ThorVG** | `v0.11.2` | |
| **Cairo** | `1.17.8` | |
| **Vello** | `v0.0.1` (or shader-set equivalent) | Requires Rust toolchain integration |
| **Google Benchmark** | `v1.8.3` | |

### A.7.2 Integration

1. The project **SHOULD** use CMake `FetchContent` or `git submodule` to managing these versions.
2. The project **MUST** allow system-installed dependencies if versions are pinned and recorded.
3. The build system **MUST** emit dependency pin records into build artifacts and run metadata.

**Rationale**: Ensures reproducibility while accommodating platform-specific packaging realities.

## A.8 Packaging and Release Tooling

1. The release process **MUST** produce per-OS/arch archives.

2. The project **SHOULD** use CMake packaging support (e.g., CPack) or a small set of scripts to assemble:

   * executables,
   * assets bundle,
   * metadata (deps lock, build-info),
   * license notices,
   * checksums.

3. Release automation **SHOULD** be implemented via GitHub Actions (or equivalent CI) to produce GitHub Releases artifacts.

**Rationale**: Automating packaging reduces human error and ensures consistent artifact contents.

## A.9 Testing Tooling (High-Level)

1. The project **SHOULD** use a C++ unit test framework compatible with CMake (e.g., Catch2 or GoogleTest).
2. Test tooling **MUST** support:

   * IR parsing/validation tests,
   * manifest hash validation tests,
   * adapter smoke tests (where feasible).

**Rationale**: IR correctness and adapter stability are critical; tests reduce regression risk.

## A.10 Recommended Developer Tooling (Non-Normative)

1. Formatting: clang-format
2. Static analysis: clang-tidy (where supported)
3. Sanitizers: ASan/UBSan on supported platforms for validation builds
4. Profiling: platform tools (Instruments, perf, Windows Performance Analyzer) as applicable

## Acceptance Criteria

1. The appendix identifies concrete technology choices aligned to constraints: C++20, CMake, cross-platform timing, JSON/CSV outputs, SHA-256 hashing.
2. Dependency management includes both fetched and system-installed options with pinning/recording requirements.
3. Packaging and release tooling is described sufficiently to implement consistent GitHub Releases artifacts.

## Dependencies

1. Chapter 2 — Requirements (measurement/reporting/reproducibility requirements).
2. Chapter 5 — Data Design (hashing and IR versioning).
3. Chapter 11 — Deployment Architecture (release artifacts and environments).

---

