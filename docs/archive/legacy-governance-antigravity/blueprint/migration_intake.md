# Migration Intake — VGCPU-Benchmark

Date: 2025-12-23 (Europe/Rome)
Input archive: `VGCPU-Benchmark.zip`
Extracted repo root: `VGCPU-Benchmark/`

## 1) Observed Facts (grounded)

### Repository & licensing
- [META-INTAKE-01] Repo name appears as **VGCPU-Benchmark** (CMake `project(VGCPU-Benchmark VERSION 0.1.0)`).
- [META-INTAKE-02] License: **MIT** (copyright 2025 Michele Fabbri).
- [META-INTAKE-03] A substantial internal design document set already exists under `/blueprint/` (chapters + backend notes).
  - Implication: our generated `/blueprint/` (per this GPT’s repo convention) will conflict unless we migrate/rename the existing directory. (See §3 risks.)

### Build system & targets
- [BUILD-INTAKE-01] Build system: **CMake** (minimum required version 3.21), C++ standard set to **C++20**.
- [BUILD-INTAKE-02] Primary build artifacts (from `CMakeLists.txt`):
  - `vgcpu_core` (STATIC)
  - `vgcpu_adapters` (STATIC)
  - `vgcpu_harness` (STATIC)
  - `vgcpu_reporting` (STATIC)
  - `vgcpu-benchmark` (executable)
- [BUILD-INTAKE-03] Backend feature toggles present (all default ON):
  - `ENABLE_NULL_BACKEND`, `ENABLE_PLUTOVG`, `ENABLE_CAIRO`, `ENABLE_BLEND2D`, `ENABLE_SKIA`,
    `ENABLE_THORVG`, `ENABLE_AGG`, `ENABLE_QT`, `ENABLE_AMANITHVG`, `ENABLE_RAQOTE`, `ENABLE_VELLO_CPU`.

### Dependencies (as configured today)
- [BUILD-INTAKE-04] Dependencies are acquired via `FetchContent` and `find_package` (Qt), plus Rust workspace import via **Corrosion**.
- [BUILD-INTAKE-05] FetchContent declarations discovered:

| Dep | Source | Pin |
|---|---|---|
| nlohmann_json | https://github.com/nlohmann/json.git | v3.11.3 |
| plutovg | https://github.com/sammycage/plutovg.git | v1.3.2 |
| cairo_prebuilt (Windows) | preshing/cairo-windows zip release | 1.17.2 |
| asmjit | https://github.com/asmjit/asmjit.git | **master** |
| blend2d | https://github.com/blend2d/blend2d.git | **master** |
| skia_binaries | aseprite/skia prebuilt zips (URL chosen per-platform) | pinned to release `m124-08a5439a6b` in URLs |
| freetype | https://gitlab.freedesktop.org/freetype/freetype.git | VER-2-13-2 |
| agg | https://github.com/ghaerr/agg-2.6.git | **master** |
| thorvg | https://github.com/thorvg/thorvg.git | v0.15.16 |
| amanithvg_sdk | https://github.com/Mazatech/amanithvg-sdk.git | **master** |
| Corrosion | https://github.com/corrosion-rs/corrosion.git | v0.5.0 |

- [BUILD-INTAKE-06] Rust bridge workspace exists at `/rust_bridge` with members `raqote_ffi` and `vello_ffi`.
  - `rust_bridge/rust-toolchain.toml` pins **nightly** (components include rustfmt/clippy).

### CI/CD & release automation
- [BUILD-INTAKE-07] GitHub Actions present:
  - CI workflow builds Release on `ubuntu-latest`, `macos-latest`, `windows-latest`, runs `ctest`, and runs a small CLI smoke test (`--help`, `list`, minimal `run` on `null` backend).
  - Release workflow triggers on tags `v*` and uploads per-OS packaged artifacts + SHA-256 checksums.

### Codebase structure (current)
- [ARCH-INTAKE-01] Source is organized under `/src` with these visible modules:
  - `common/` (Status/Result, capability sets)
  - `pal/` (platform abstraction: timers, environment)
  - `ir/` (IR format + loader, prepared scenes)
  - `assets/` (scene registry + manifest loading)
  - `adapters/` (backend adapters + registry)
  - `harness/` (benchmark harness + statistics)
  - `reporting/` (CSV/JSON writers + summary)
  - `cli/` (CLI parser + entrypoint)
- [ARCH-INTAKE-02] Assets ship in-repo:
  - `/assets/scenes/manifest.json` + 6 `*.irbin` scenes (fills, strokes, validation).

### Testing footprint
- [TEST-INTAKE-01] No standalone `/tests` tree detected.
- [TEST-INTAKE-02] CMake does not currently declare CTest test cases (`enable_testing()`/`add_test()` not found). CI’s `ctest` step likely executes **0 tests** today (CI still adds a CLI smoke test outside CTest).

## 2) Inferred Intent (explicitly inferred; validate)

- [ARCH-INTAKE-10] Provide a **cross-backend, CPU-only** benchmarking harness that ensures identical vector workloads across libraries by running the same canonical IR scenes through per-backend adapters.
- [REQ-INTAKE-10] Provide **repeatable performance reporting** (CSV/JSON/summary) and basic environment metadata to support comparison across machines/OSes.
- [SCOPE-INTAKE-10] Treat Rust-only backends (Raqote/Vello) as optional features, integrated via a CMake↔Rust bridge (Corrosion).

## 3) Gaps / risks / modernization blockers

- [RISK-INTAKE-01] **Repo convention conflict:** existing `/blueprint/` directory will collide with the required generated `/blueprint/` layout (blueprint_vX.Y_ch*.md, decision_log.md, walkthrough.md, implementation_checklist.yaml).
- [RISK-INTAKE-02] **Reproducibility risk:** several key deps track `master` (asmjit, blend2d, agg, amanithvg_sdk). This breaks “pinned dependency versions” guarantees in practice.
- [RISK-INTAKE-03] **Rust toolchain policy mismatch:** build docs mention “stable” is sufficient, but `rust-toolchain.toml` pins **nightly**. This affects CI portability and contributor setup.
- [RISK-INTAKE-04] **Testing gap:** CI runs `ctest` but there are no registered tests; functional correctness and adapter parity likely rely on ad-hoc runs only.
- [RISK-INTAKE-05] **Public/private boundary unclear:** headers live under `/src` and are added as PUBLIC include dirs for some targets. This can accidentally become a de-facto public API/ABI.
- [RISK-INTAKE-06] **Benchmark determinism controls:** no explicit policy observed for CPU affinity, turbo/frequency scaling, warmup strategy standardization, or background noise mitigation (important for p90/p99 stability).

## 4) Modernization delta (what the new blueprint will drive)

References: `blueprint_schema.md`, `repo_layout_and_targets.md`, `cmake_playbook.md`, `dependency_policy.md`, `testing_strategy.md`, `ci_reference.md`, `performance_benchmark_harness.md`, `temp_dbg_policy.md`, `logging_observability_policy_new.md`, `code_style_and_tooling.md`, `cr_template.md`, `decision_log_template.md`.

- [DEC-PRE-INTAKE-01] Establish the required canonical blueprint repo layout by **migrating the existing `/blueprint/` docs** to a non-conflicting location (e.g., `/docs/legacy_blueprint/`) and generating the new `/blueprint/` as source-of-truth.
- [BUILD-DELTA-01] Add CMake presets + toolchain options per `cmake_playbook.md` (configure/build/test/bench presets; CI consumes presets).
- [TEST-DELTA-01] Introduce a real test suite per `testing_strategy.md`:
  - unit tests for IR loader, manifest parsing, stats math;
  - golden-image or checksum-based adapter validation (CPU-only) behind a deterministic baseline;
  - register tests in CTest and make CI fail on missing tests.
- [BUILD-DELTA-02] Pin “master” dependencies to immutable commits/tags per `dependency_policy.md`.
- [VER-DELTA-01] Introduce SemVer + CR governance + decision log per `cr_template.md` + `decision_log_template.md` (also covers deprecations and migration).
- [TEMP-DBG-DELTA-01] Add the TEMP-DBG marker policy and CI gate per `temp_dbg_policy.md`.
- [REQ-DELTA-02] Add structured logging/metrics per `logging_observability_policy_new.md` (bench metadata, run policy, failures).
- [BUILD-DELTA-03] Add sanitizers + static analysis gates per `ci_reference.md` and `code_style_and_tooling.md` (clang-tidy, format, ASan/UBSan; optionally TSan where applicable).
- [PERF-DELTA-01] Add a benchmark regression harness + baselining per `performance_benchmark_harness.md` (p50/p90/p99 budget checks, trend artifacts).

## 5) Sharp architecture questions (need answers before Chapter 0)

1) [META-INTAKE-Q01] **Product boundary:** Is this primarily a *CLI application* or do you want to ship a reusable *library SDK* (stable public API/ABI) for embedding benchmarks in other tools?
2) [META-INTAKE-Q02] **Error model:** Keep the current `Status/Result<T>` (no exceptions) everywhere, or allow exceptions in leaf adapters / third-party integration layers?
3) [META-INTAKE-Q03] **Determinism target:** Do you need cross-run determinism strong enough for CI regression gates (same machine) at p90/p99, or is this primarily for manual benchmarking?
4) [META-INTAKE-Q04] **Backend policy:** Which backends are “tier-1” (must build + run in CI on all OSes) vs “best-effort/optional”?
5) [META-INTAKE-Q05] **Rust policy:** Are Rust backends mandatory? If optional, should we require **stable Rust** only (and remove nightly pin), or is nightly acceptable?
6) [META-INTAKE-Q06] **Result schema stability:** Should CSV/JSON schemas be versioned and treated as a compatibility surface (i.e., downstream tooling depends on them)?
7) [META-INTAKE-Q07] **Redistribution:** Are you okay redistributing the fetched **Skia** prebuilts and Windows Cairo prebuilts inside release artifacts, or should releases build everything from source?
