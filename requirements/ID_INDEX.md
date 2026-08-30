# Legacy blueprint ID index (frozen)

This file resolves the stable `[REQ-*]`, `[ARCH-*]`, `[API-*]`, `[MEM-*]`,
`[CONC-*]`, `[BUILD-*]`, `[TOOL-*]`, `[TEST-*]`, `[VER-*]`, `[DEC-*]`,
`[SCOPE-*]` IDs still cited in 55 source, test, and tooling files across the
repository, after the governing blueprint chapters were archived under
`docs/archive/legacy-governance-antigravity/` (ADR-0003).

It is **frozen**: it lists only IDs already cited in code as of the
governance migration (2026-08-30, ADR-0003), each with a one-line summary
and the archived chapter that defines it in full. Do not add new IDs here.
New work cites its governing ADR (`docs/adr/ADR-XXXX-*.md`) instead of
minting a new ID in this scheme; existing code comments keep citing their
original ID unchanged (never renumbered).

A handful of task-checklist references (`[TASK-03.03]`, `[TASK-04.0x]`,
`[TASK-05.0x]`, `[TASK-07.02]`, `[TASK-08.01]`, `[TASK-09.0x]`) predate the
final `implementation_checklist.yaml` snapshot that was archived (that file
was overwritten in place across blueprint revisions, per its own append-only
task-log convention) and cannot be resolved to a surviving definition; they
are historical markers of already-completed work, informational only. The
archived `git log` for `docs/archive/legacy-governance-antigravity/blueprint/`
has the full history if needed.

## Index

- **API-01-01** — CLI contract (public) _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-01-02** — Report schemas (public) _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-03** — Error handling strategy (mandatory decision) _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-04** — Thread-safety and reentrancy contracts _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-06-01** — Common: errors and results _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-06-02** — PAL: monotonic clock and environment _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-06-03** — Assets/Manifest: scene discovery _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-06-04** — IR: decoding to canonical representation _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-06-05** — Adapters: descriptors, registry, adapter instance _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-06-06** — Harness: run orchestration _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-06-07** — Reporting: CSV/JSON emit _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **API-07** — Rust FFI (internal C ABI contract) _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **ARCH-10-01** — CLI Frontend — `src/cli/*` — `vgcpu-benchmark` — Parse args/commands; dispatch to harness/reporting; user interaction _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-10-02** — Common Types — `src/common/*` — `vgcpu_core` — `Status/Result<T>`, error codes, capability sets, common utilities _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-10-03** — PAL — `src/pal/*` — `vgcpu_core` — Monotonic timing, env/CPU info, filesystem helpers (minimal) _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-10-04** — Assets & Manifest — `src/assets/*`, `/assets/scenes/*` — `vgcpu_core` — Scene enumeration, manifest parsing, asset path resolution _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-10-05** — IR Loader / Decoder — `src/ir/*` — `vgcpu_core` — Load `.irbin` scenes into an in-memory canonical representation _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-10-06** — Adapter Registry — `src/adapters/*` — `vgcpu_adapters` — Backend discovery; feature flags; create adapter instances _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-10-07** — Backend Adapters — `src/adapters/backends/*` (inferred) — `vgcpu_adapters` — Translate canonical IR into backend calls; manage backend resources _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-10-08** — Benchmark Harness — `src/harness/*` — `vgcpu_harness` — Orchestrate warmup/measured loops; compute stats; enforce run policy _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-10-09** — Statistics — `src/harness/stats*` (inferred) — `vgcpu_harness` — Robust aggregation: p50/p90/p99 etc. (per [REQ-04-04]) _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-10-10** — Reporting — `src/reporting/*` — `vgcpu_reporting` — Emit CSV/JSON + summary (per [REQ-05]) with schema version (per [DEC-SCOPE-03]) _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-11** — Allowed dependencies (enforced) _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-12-01c** — `SceneIR`: decoded commands and resources (paths, paints, transforms, text runs if present). _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-12-01d** — `PreparedScene`: backend-independent preprocessed representation suitable for adapters. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-12-02a** — `RunConfig`: backend, scenes, warmup, reps, thread count, affinity flags, build info, schema version. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-12-02c** — `SceneStats`: {min, mean, median, p50, p90, p99, stddev?} + sample_count. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-12-02d** — `RunReport`: per-scene stats + environment metadata + errors encountered. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-13** — Primary execution flow (state machine) _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-13-01** — Run lifecycle state machine _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-13-02a** — Warmup loop _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-13-02b** — Measured loop _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-14-A** — CLI Frontend ([ARCH-10-01]) _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-14-B** — PAL ([ARCH-10-03]) _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **ARCH-14-F** — Backend Adapters ([ARCH-10-07]) _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **CONC-08-01** — The CI MUST run ThreadSanitizer (TSan) on at least one Linux/Clang configuration for Tier-1 backends where feasible. _(archived: `blueprint_v1.1_ch6_concurrency_parallelism.md`)_
- **DEC-API-06** — Use this uniform FFI shape for both Raqote and Vello adapters. _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **DEC-BUILD-01** — Keep existing CMake option names (`ENABLE_*`) for backends to avoid breaking docs/scripts. _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **DEC-BUILD-04** — Do NOT combine sanitizers in v0.2.0 presets. _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **DEC-BUILD-05** — Keep FetchContent for v0.2.0 but make it deterministic _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **DEC-BUILD-06** — Switch Rust toolchain pin from floating `nightly` to a pinned **stable** toolchain version for reproducibility and portability. _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **DEC-BUILD-21** — SSIM library: vendor Chris Lomont’s single-file C++ SSIM implementation (MIT licensed) from the `ChrisLomont/SSIM` project. _(archived: `blueprint_v1.1_ch7_build_toolchain.md`)_
- **DEC-MEM-03** — Use in-place sort of the preallocated vector after measurement to avoid extra allocations; percentiles computed by deterministic index selection. _(archived: `blueprint_v1.0_ch5_data_design_hotpath.md`)_
- **DEC-SCOPE-02** — Define **Tier-1** backends for v0.2.0 as _(archived: `blueprint_v1.0_ch1_scope.md`)_
- **REQ-01** — The product MUST build and run on _(archived: `blueprint_v1.0_ch1_scope.md`)_
- **REQ-06** — The system MUST expose a backend registry _(archived: `blueprint_v1.0_ch1_scope.md`)_
- **REQ-07** — Backends MUST be build-time selectable via CMake options. _(archived: `blueprint_v1.0_ch1_scope.md`)_
- **REQ-21** — The measured loop MUST NOT perform filesystem I/O. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **REQ-23** — The measured loop MUST avoid dynamic allocations on the VGCPU side (best-effort; backend may allocate internally). _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **REQ-26** — Timing MUST use a monotonic clock (no wall-clock adjustments) (supports [REQ-04-03]). _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **REQ-29** — Scene ordering MUST be stable (sorted by `SceneId`) unless user explicitly overrides. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **REQ-35** — Adapters MUST be thread-safe per declared mode _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **REQ-41** — CSV MUST be stable column-ordered and include schema version as a header comment where feasible. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **REQ-42** — Reporting MUST NOT reorder scenes beyond the stable harness ordering. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **REQ-43** — Any log emission MUST occur outside the measured loop (supports [REQ-22] and [REQ-02-03]). _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **REQ-45** — Harness MUST support _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **REQ-46** — The CLI commands/options are a compatibility surface _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-48** — CSV and JSON output formats are compatibility surfaces (per [DEC-SCOPE-03]). _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-49** — Reports MUST carry _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-52** — Symbol visibility and exports _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-53** — Canonical error types _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-53-02** — Define _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-54** — Error conversion rules _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-55** — Component thread-safety matrix (authoritative) _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-56** — Reentrancy rules _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-56-02** — If an adapter uses global state (some libraries do), it must serialize internally and declare itself “single-thread-only”. _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-60** — Any change to _(archived: `blueprint_v1.0_ch4_interfaces_api_abi.md`)_
- **REQ-63** — Stats computation MUST operate on _(archived: `blueprint_v1.0_ch5_data_design_hotpath.md`)_
- **REQ-64** — The harness MUST allocate an RGBA8 premultiplied buffer sized _(archived: `blueprint_v1.0_ch5_data_design_hotpath.md`)_
- **REQ-65** — Output buffer base pointer should be 64-byte aligned in Release builds (best-effort). _(archived: `blueprint_v1.0_ch5_data_design_hotpath.md`)_
- **REQ-71-01** — No VGCPU filesystem access. _(archived: `blueprint_v1.0_ch5_data_design_hotpath.md`)_
- **REQ-89** — C++ standard MUST be **C++20** ([DEC-SCOPE-01]). _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-92** — Required preset set _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-95** — Standard project options (binding) _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-96** — Existing backend options remain supported (observed) _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-97** — Sanitizer flags MUST apply only to VGCPU targets (not third-party) where feasible. _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-98** — CI MUST document runtime options _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-99** — Pinning rules (binding) _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-100-01** — asmjit (currently `master`) _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-100-02** — blend2d (currently `master`) _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-100-03** — agg-2.6 (currently `master`) _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-100-04** — amanithvg-sdk (currently `master`) _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-103** — Standard gate targets _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-103-02** — `lint` (clang-tidy) when `VGCPU_ENABLE_LINT=ON` _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-104** — TEMP-DBG code MUST use the exact markers from `temp_dbg_policy.md` _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-105** — `check_no_temp_dbg` MUST fail the build if any marker exists in tracked sources. _(archived: `blueprint_v1.0_ch7_build_toolchain.md`)_
- **REQ-113** — Tooling MUST be cross-platform (Windows/macOS/Linux) and avoid requiring proprietary IDE features. _(archived: `blueprint_v1.0_ch8_tooling.md`)_
- **REQ-116** — clang-format policy (mandatory) _(archived: `blueprint_v1.0_ch8_tooling.md`)_
- **REQ-121** — TEMP-DBG compliance _(archived: `blueprint_v1.0_ch8_tooling.md`)_
- **REQ-122** — Repository-wide detection _(archived: `blueprint_v1.0_ch8_tooling.md`)_
- **REQ-123** — Structured log schema helpers _(archived: `blueprint_v1.0_ch8_tooling.md`)_
- **REQ-124** — Provide schema validators _(archived: `blueprint_v1.0_ch8_tooling.md`)_
- **REQ-125** — CI MUST run validators on artifacts produced by smoke runs (Tier-1). _(archived: `blueprint_v1.0_ch8_tooling.md`)_
- **REQ-130** — Provide `docs/developer_quickstart.md` with _(archived: `blueprint_v1.0_ch8_tooling.md`)_
- **REQ-131** — SemVer change classification (binding) _(archived: `blueprint_v1.0_ch9_versioning_lifecycle.md`)_
- **REQ-133** — Schema version contract (binding) _(archived: `blueprint_v1.0_ch9_versioning_lifecycle.md`)_
- **REQ-133-01** — JSON MUST include integer `schema_version` ([REQ-40], [REQ-49]). _(archived: `blueprint_v1.0_ch9_versioning_lifecycle.md`)_
- **REQ-133-02** — CSV MUST include schema version in a header comment (or dedicated column/header field) ([REQ-41]). _(archived: `blueprint_v1.0_ch9_versioning_lifecycle.md`)_
- **REQ-155** — The build/CI MUST verify the integrity of vendored single-file dependencies (`stb_image_write`, SSIM) against recorded SHA-256 values. _(archived: `blueprint_v1.1_ch8_tooling.md`)_
- **SCOPE-05** — Backend tiering policy (scope decision) _(archived: `blueprint_v1.0_ch1_scope.md`)_
- **TEST-01** — `cli_help_smoke`: `vgcpu-benchmark --help` exits 0 and prints usage. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **TEST-04** — `pal_monotonic_increases`: successive calls produce non-decreasing timestamps. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **TEST-05** — `pal_env_metadata_present`: required fields exist or are explicitly “unknown” (no empty/null). _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **TEST-10** — `registry_contains_tier1`: Tier-1 backends appear on all OSes in CI. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **TEST-12** — `adapter_null_renders`: null backend renders without error and minimal overhead. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **TEST-13** — `adapter_prepare_supported_scenes`: Tier-1 backends can prepare all “Tier-1 scene set”. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **TEST-14** — `adapter_no_io_in_render` (best-effort): instrumentation asserts no VGCPU-side file operations in render. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **TEST-15** — `stats_percentiles_known`: given a known sample set, p50/p90/p99 match expected values. _(archived: `blueprint_v1.0_ch3_component_design.md`)_
- **TEST-27** — `no_alloc_in_measured_loop_null` _(archived: `blueprint_v1.0_ch5_data_design_hotpath.md`)_
- **TEST-42** — `tool_validate_report_json_minimal` _(archived: `blueprint_v1.0_ch8_tooling.md`)_
- **TEST-43** — `tool_validate_report_csv_header` _(archived: `blueprint_v1.0_ch8_tooling.md`)_
