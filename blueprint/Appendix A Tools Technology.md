<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

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

### A.7.1 Pinned Versions Table

The project **MUST** default to the following versions for reproducible baselines (overridable by CMake options for testing newer builds):

| Library | Version / Commit | Linking | Notes |
|---|---|---|---|
| **AmanithVG SRE** | `6.0.0` (or latest stable) | Dynamic | Commercial license; CPU-only SRE variant |
| **Blend2D** | `master` (rolling release) | Static | JIT compiler included; static simplifies deployment |
| **Skia (CPU Raster)** | `m124` (Aseprite prebuilt) | Static | Prebuilt binaries; x86_64 only on Linux |
| **ThorVG SW** | `v0.15.16` | Static | Built from source; SW engine only |
| **vello_cpu** | `v0.0.1` (or latest) | Dynamic | Rust `cdylib` via Corrosion |
| **Raqote** | `v0.8.5` (or latest stable) | Dynamic | Rust `cdylib` via Corrosion |
| **PlutoVG** | `v1.3.2` (or latest stable) | Static | Built from source via FetchContent |
| **Qt Raster Engine** | `6.8 LTS` | Dynamic | System library; LGPL compliant |
| **AGG** | `2.6` | Static | Built from source (ghaerr/agg-2.6 fork) |
| **Cairo (Image Surface)** | `1.18.2` (or 1.17.2 fallback) | Dynamic | System lib or prebuilt (Windows) |
| **Google Benchmark** | `v1.8.3` | Static | Benchmark harness only |

1. The project uses **static linking** for most rendering backend libraries to:
   * simplify deployment (single binary with no shared library dependencies),
   * avoid ABI compatibility issues across systems,
   * ensure consistent behavior across platforms.

2. **Exceptions** (dynamic linking required):
   * **Qt Raster Engine**: System library, cannot be statically linked due to LGPL.
   * **Cairo**: System library on Linux; prebuilt DLL on Windows.
   * **AmanithVG SRE**: Prebuilt SDK provides only shared libraries.
   * **Rust libraries** (Raqote, vello_cpu): Built as `cdylib` crates with C FFI wrappers via Corrosion.

3. **AGG**: Built from source using the `ghaerr/agg-2.6` fork with `-fPIC` and linked statically.

4. **Skia**: Uses prebuilt static libraries from Aseprite's m124 release. Note: Skia does not guarantee ABI stability, but this is acceptable as we pin to a specific version. Linux aarch64 is NOT available from Aseprite; the backend is auto-disabled on that platform.

**Rationale**: Static linking reduces deployment complexity and ensures reproducible behavior while maintaining license compliance for LGPL libraries (Qt, Cairo).

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

