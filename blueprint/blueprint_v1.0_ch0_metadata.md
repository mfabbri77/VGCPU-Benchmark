# Chapter 0 — Blueprint Metadata (VGCPU-Benchmark)

## [META-01] Blueprint identity
- [META-01-01] Blueprint name: **VGCPU-Benchmark Modernization Blueprint**
- [META-01-02] Blueprint version: **v1.0** (this document set)
- [META-01-03] Blueprint date: **2025-12-23** (Europe/Rome)
- [META-01-04] Repository name (observed): **VGCPU-Benchmark**
- [META-01-05] Repository license (observed): **MIT** (2025 Michele Fabbri)
- [META-01-06] Snapshot source: uploaded `VGCPU-Benchmark.zip` (no git metadata assumed)
- [DEC-META-01] Product SemVer target for the *next* modernization release is **v0.2.0** (from observed `project(... VERSION 0.1.0)`); blueprint itself is versioned independently as v1.0.
  - Rationale: avoid an artificial “1.0” promise for the product while still establishing a stable blueprint baseline.
  - Alternatives: (A) keep product at 0.1.x; (B) jump product to 1.0.0.
  - Consequences: release notes + migration notes must clearly separate “blueprint v1.0” vs “product v0.2.0”.

## [META-02] Glossary
- [META-02-01] **Backend**: a rendering implementation/library (e.g., Skia, Cairo, Blend2D, ThorVG, etc.).
- [META-02-02] **Adapter**: VGCPU component that maps canonical VGCPU IR to a backend’s API.
- [META-02-03] **IR**: VGCPU intermediate representation describing a scene (loaded from `.irbin`).
- [META-02-04] **Scene**: a benchmark workload (IR + metadata) executed through an adapter.
- [META-02-05] **Harness**: orchestration layer that runs scenes, collects timings, performs warmups/repetitions, and computes statistics.
- [META-02-06] **Report**: machine-readable outputs (CSV/JSON) + human summary generated from runs.
- [META-02-07] **Tier-1 backend**: must build and run in CI on all supported OSes for each release.
- [META-02-08] **Optional backend**: builds/runs on best-effort basis; may be excluded from CI and/or releases.

## [META-03] Assumptions (until answered otherwise)
- [META-03-01] Primary deliverable is a **CLI application** (`vgcpu-benchmark`) with internal libraries; a stable external SDK API/ABI is not required unless explicitly requested. (See [META-04-01].)
- [META-03-02] Benchmark focus is **CPU rendering**; GPU acceleration is out of scope.
- [META-03-03] Cross-platform support is required: **Windows / macOS / Linux**.
- [META-03-04] C++ toolchain baseline remains **C++20** (observed), with no ABI-breaking compiler extensions.
- [META-03-05] Rust-backed adapters (Raqote/Vello) remain optional features unless declared tier-1.
- [META-03-06] Deterministic benchmarking will be “best achievable” on a given machine; cross-machine determinism is not assumed.

## [META-04] Risks & open questions
- [META-04-01] Product boundary: CLI-only vs ship a reusable SDK/public API surface.
- [META-04-02] Error model: `Status/Result<T>` everywhere vs exceptions permitted in integration leaves.
- [META-04-03] Determinism requirements for CI perf regression gates (p50/p90/p99) vs manual-only benchmarking.
- [META-04-04] Backend tiering policy (tier-1 list vs optional list).
- [META-04-05] Rust toolchain policy: stable-only vs nightly acceptable (observed nightly pin in `rust-toolchain.toml`).
- [META-04-06] Output schema stability: versioned CSV/JSON contract vs “best effort”.
- [META-04-07] Dependency reproducibility: several deps track `master` today; must be pinned to immutable revisions.
- [META-04-08] Repo layout conflict: existing `/blueprint/` directory collides with required canonical `/blueprint/` structure.
  - [DEC-PRE-INTAKE-01] Resolve by moving existing `/blueprint/` to a non-conflicting path (e.g., `/docs/legacy_blueprint/`) and generating canonical `/blueprint/` as the sole source-of-truth for modernization.

## [META-05] Required `/blueprint` files for blueprint v1.0
The following files constitute the authoritative blueprint set for v1.0:

- [META-05-01] `/blueprint/blueprint_v1.0_ch0_metadata.md`
- [META-05-02] `/blueprint/blueprint_v1.0_ch1_scope.md`
- [META-05-03] `/blueprint/blueprint_v1.0_ch2_system_architecture.md`
- [META-05-04] `/blueprint/blueprint_v1.0_ch3_component_design.md`
- [META-05-05] `/blueprint/blueprint_v1.0_ch4_interfaces_api_abi.md`
- [META-05-06] `/blueprint/blueprint_v1.0_ch5_data_design_hotpath.md`
- [META-05-07] `/blueprint/blueprint_v1.0_ch6_concurrency_parallelism.md`
- [META-05-08] `/blueprint/blueprint_v1.0_ch7_build_toolchain.md`
- [META-05-09] `/blueprint/blueprint_v1.0_ch8_tooling.md` *(or N/A + required [DEC-*] if explicitly out of scope)*
- [META-05-10] `/blueprint/blueprint_v1.0_ch9_versioning_lifecycle.md`
- [META-05-11] `/blueprint/decision_log.md` (append-only)
- [META-05-12] `/blueprint/walkthrough.md`
- [META-05-13] `/blueprint/implementation_checklist.yaml`
- [META-05-14] `/cr/CR-XXXX.md` files for any approved changes (per governance)

Notes:
- [META-05-15] All major requirements/constraints/decisions must carry stable IDs and never be renumbered.
- [META-05-16] Every [REQ-*] must map to ≥1 [TEST-*] and ≥1 checklist step (in `implementation_checklist.yaml`).
