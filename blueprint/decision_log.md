# Decision Log — VGCPU-Benchmark (Append-only)

Date created: 2025-12-23 (Europe/Rome)
Blueprint version: v1.0.0
Product target milestone: v0.2.0 (see [DEC-META-01])

> Rule: This file is **append-only**. Do not edit prior entries; add new decisions or superseding entries.

---

## [DEC-PRE-INTAKE-01] Canonical /blueprint directory; move legacy docs
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** The repo already contains `/blueprint/` legacy documents. This modernization requires `/blueprint/` to be the canonical source-of-truth with specific filenames.
- **Decision:** Move existing `/blueprint/` to `/docs/legacy_blueprint/` and generate the canonical `/blueprint/` structure for this blueprint set.
- **Rationale:** Avoid naming collisions and ensure deterministic governance artifacts.
- **Alternatives:** (A) Rename the canonical blueprint directory (rejected; violates repo convention). (B) Delete legacy docs (rejected; lose context).
- **Consequences:** Links/docs must be updated; contributors follow the new canonical `/blueprint/`.
- **Affected IDs:** [DEC-ARCH-05], [DEC-TOOL-01], [REQ-115], [REQ-147]
- **Verification:** CI/doc lint ensures canonical blueprint files exist; migration doc updated ([REQ-147]).

---

## [DEC-META-01] Product version target for modernization milestone
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Observed product version is 0.1.0; modernization introduces significant governance/build/test additions.
- **Decision:** Target product release version **v0.2.0** for the first implementation pass of blueprint v1.0.
- **Rationale:** Signals meaningful improvements without implying long-term API stability of a 1.0.
- **Alternatives:** (A) 0.1.x patch/minor only. (B) Jump to 1.0.0.
- **Consequences:** Release notes must distinguish blueprint vs product versions.
- **Affected IDs:** [VER-01], [REQ-139]
- **Verification:** [TEST-45] version macros match CMake; tag naming in release workflow.

---

## [DEC-SCOPE-01] C++ language standard baseline
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Repo is already configured for C++20.
- **Decision:** Keep **C++20** as the baseline; upgrade to C++23 only via CR.
- **Rationale:** Minimizes toolchain churn across OSes.
- **Alternatives:** C++17; immediate C++23.
- **Consequences:** Avoid C++23-only features; keep portability constraints.
- **Affected IDs:** [REQ-89]
- **Verification:** Configure presets compile with C++20 across CI matrix.

---

## [DEC-SCOPE-02] Tier-1 backend list for CI stability
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Many optional backends add heavy/fragile dependencies. CI needs a stable minimum set.
- **Decision:** Tier-1 backends: `null`, `plutovg`, `blend2d`. Others optional by default.
- **Rationale:** Cross-platform reliability while still exercising meaningful CPU renderers.
- **Alternatives:** Only `null+plutovg`; include Skia/Cairo/Qt as Tier-1.
- **Consequences:** CI enforces Tier-1 only via preset/option ([VGCPU_TIER1_ONLY]).
- **Affected IDs:** [DEC-BUILD-03], [REQ-110], [TEST-10]
- **Verification:** `registry_contains_tier1` in CI on all OSes.

---

## [DEC-SCOPE-03] Reports are a versioned compatibility surface
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Downstream tooling often parses CSV/JSON. Unversioned changes break consumers.
- **Decision:** Treat CSV/JSON schemas as versioned; include `schema_version` and apply SemVer rules to schema-impacting changes.
- **Rationale:** Predictable evolution and traceability.
- **Alternatives:** Best-effort, undocumented schema changes.
- **Consequences:** Schema validators + tests required; schema changes require CR.
- **Affected IDs:** [REQ-48..50], [REQ-133], [TEST-24], [TEST-42], [TEST-43]
- **Verification:** Schema validators run in CI on smoke artifacts ([REQ-125]).

---

## [DEC-SCOPE-04] Coarse determinism target for CI perf gates
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Shared CI runners are noisy; strict perf gates are flaky.
- **Decision:** CI perf regression gates detect only large regressions (≈10–15%+).
- **Rationale:** Reduce flakiness while still catching meaningful regressions.
- **Alternatives:** No perf gating; strict 3–5% gating.
- **Consequences:** Perf policy requires rerun confirmation on failure.
- **Affected IDs:** [DEC-VER-04], [REQ-143..146]
- **Verification:** Perf diff tool + policy tests ([TEST-48]).

---

## [DEC-ARCH-01] Internal libraries are not a stable external SDK ABI
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Headers and targets are currently structured as internal implementation pieces.
- **Decision:** Treat `vgcpu_*` libs as internal; no stable external ABI promise.
- **Rationale:** Enables iteration without ABI constraints.
- **Alternatives:** Publish an SDK; provide stable C ABI.
- **Consequences:** Enforce include/visibility boundaries to avoid accidental public API.
- **Affected IDs:** [DEC-API-01], [REQ-120], [REQ-52]
- **Verification:** Include linter and visibility flags in CI.

---

## [DEC-ARCH-02] Single-process, single-executable architecture
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Plugin architectures add ABI/security/distribution complexity.
- **Decision:** Keep compiled-in backends; no runtime plugin DLL/SO in v0.2.0.
- **Rationale:** Simpler cross-platform distribution.
- **Alternatives:** Runtime plugins per backend.
- **Consequences:** Adding backends requires rebuild; feature flags remain build-time.
- **Affected IDs:** [REQ-07], [BUILD-03]
- **Verification:** Release artifacts run without external plugin installs.

---

## [DEC-ARCH-03] Core must not depend on backend libraries
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Core (PAL/IR/assets) must be testable without heavyweight deps.
- **Decision:** `vgcpu_core` has zero backend dependencies.
- **Rationale:** Portability and testability.
- **Alternatives:** Allow backends to leak into core.
- **Consequences:** Adapter boundary must be expressive and isolated.
- **Affected IDs:** [REQ-120-01], [DEC-TOOL-04]
- **Verification:** Include linter blocks forbidden include patterns.

---

## [DEC-ARCH-04] Rust adapters optional by default
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Rust adds toolchain complexity and increases CI variability.
- **Decision:** Rust-backed adapters remain optional (not Tier-1) unless promoted via CR.
- **Rationale:** Keep minimum build stable.
- **Alternatives:** Make Rust adapters Tier-1.
- **Consequences:** Separate CI job for Rust; stable Rust pinning required.
- **Affected IDs:** [DEC-BUILD-06], [REQ-102]
- **Verification:** Optional CI job; build succeeds with Rust disabled.

---

## [DEC-ARCH-05] Canonical repo layout adopted; legacy blueprint moved
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context/Decision/Rationale:** See [DEC-PRE-INTAKE-01].
- **Affected IDs:** [REQ-115]
- **Verification:** Repo structure checks in CI and docs.

---

## [DEC-ARCH-06] Distribution policy for optional runtime deps
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Some optional backends rely on runtime DLLs/prebuilts.
- **Decision:** Tier-1 backends must not require extra runtime installs; optional backends bundled only if legally/reliably possible or disabled in Tier-1 releases.
- **Rationale:** Predictable out-of-box execution.
- **Alternatives:** Bundle everything always; require users to install runtime deps.
- **Consequences:** Provide tiered release presets/artifacts.
- **Affected IDs:** [REQ-17], [REQ-140]
- **Verification:** Tier-1 release smoke run passes on clean machines/CI runners.

---

## [DEC-ARCH-07] IR immutability after load
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Cross-backend comparability and caching safety.
- **Decision:** `SceneIR` and `PreparedScene` are immutable; adapters may prepare backend-specific resources without mutating shared IR.
- **Rationale:** Prevent cross-backend contamination.
- **Alternatives:** Let adapters mutate IR.
- **Consequences:** Adapter API includes a `Prepare()` stage; ownership rules tightened.
- **Affected IDs:** [REQ-66], [REQ-83]
- **Verification:** Concurrency tests + code review; no mutable shared state.

---

## [DEC-ARCH-08] Default fail-on-missing-scene; optional continue
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Benchmark comparability expects a complete scene set.
- **Decision:** Default is fail run if any requested scene cannot load/prepare; provide `--continue-on-error` for partial runs.
- **Rationale:** Avoid silent partial benchmarks.
- **Alternatives:** Skip missing scenes by default.
- **Consequences:** CLI option and report error records required.
- **Affected IDs:** [REQ-24], [REQ-81-02]
- **Verification:** CLI tests for both modes; report contains errors.

---

## [DEC-ARCH-09] Status/Result as primary error transport
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Need stable error codes and predictable control flow.
- **Decision:** Use `Status/Result<T>` across VGCPU boundaries; catch/convert any exceptions at boundaries.
- **Rationale:** Deterministic error handling and reporting.
- **Alternatives:** Exceptions end-to-end.
- **Consequences:** Interfaces are noexcept where practical; error codes are append-only.
- **Affected IDs:** [DEC-API-03], [REQ-53], [REQ-54]
- **Verification:** Unit tests for stable error codes; no-throw boundaries.

---

## [DEC-ARCH-10] Single-thread default; multi-thread opt-in
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Benchmark noise and comparability.
- **Decision:** Default `--threads=1`; multi-thread is opt-in and treated as separate benchmark dimension.
- **Rationale:** Reduce noise and complexity.
- **Alternatives:** Auto-parallelize by core count.
- **Consequences:** Reports/logs must record thread count prominently.
- **Affected IDs:** [DEC-API-04], [REQ-75], [REQ-145]
- **Verification:** Concurrency tests and CLI contract tests.

---

## [DEC-API-01] Internal-only headers; avoid accidental public API
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Current include layout risks becoming a de-facto public SDK.
- **Decision:** Use `include/vgcpu/internal/**` for internal module interfaces; enforce boundary with tooling.
- **Rationale:** Prevent unintended API commitments.
- **Alternatives:** Publish SDK headers.
- **Consequences:** Include linter + visibility flags + no install step by default.
- **Affected IDs:** [DEC-API-05], [REQ-120]
- **Verification:** `check_includes` gate and CI compile.

---

## [DEC-API-02] ABI strategy = Best-effort (recompile consumers)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Static linking into CLI; no external SDK promise.
- **Decision:** No ABI stability guarantee across minor; still use visibility/export macros for hygiene.
- **Rationale:** Flexibility.
- **Alternatives:** Stable ABI; C ABI.
- **Consequences:** Maintain exports as future-proofing only.
- **Affected IDs:** [REQ-52]
- **Verification:** Builds across OSes; no ABI check required.

---

## [DEC-API-03] No exceptions across VGCPU boundaries
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Exceptions crossing module/FFI boundaries are unsafe and hard to report.
- **Decision:** No exceptions as a contract across VGCPU boundaries; convert at boundaries.
- **Rationale:** Safety and determinism.
- **Alternatives:** Exceptions end-to-end.
- **Consequences:** Explicit error codes and Status/Result usage everywhere.
- **Affected IDs:** [REQ-53], [REQ-54], [REQ-58]
- **Verification:** Tests assert stable error codes; adapter boundary catches exceptions.

---

## [DEC-API-04] Default thread-safety contract
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Many backends are not thread-safe; harness must remain safe by default.
- **Decision:** Adapters are not thread-safe unless declared; harness uses per-thread adapter instances.
- **Rationale:** Prevent races and undefined behavior.
- **Alternatives:** Share adapter across threads.
- **Consequences:** Registry exposes capability flags; harness refuses unsupported parallel runs.
- **Affected IDs:** [DEC-CONC-05], [REQ-82]
- **Verification:** [TEST-34] backend refuses parallel if unsupported.

---

## [DEC-API-05] Header structure: internal include tree
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Need deterministic internal module boundaries.
- **Decision:** Centralize internal headers under `include/vgcpu/internal/` and keep `src/` private headers private.
- **Rationale:** Clear layering and tooling enforcement.
- **Alternatives:** Keep headers in `src/` and export include dirs publicly.
- **Consequences:** CMake include dirs become target-scoped and private by default.
- **Affected IDs:** [REQ-18], [REQ-120]
- **Verification:** Include linter + build succeeds.

---

## [DEC-API-06] Uniform Rust FFI shape for Raqote/Vello
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Multiple Rust backends; avoid bespoke glue.
- **Decision:** Use a consistent `create/destroy/render_ir` C ABI shape for Rust FFI.
- **Rationale:** Simplifies integration and error handling.
- **Alternatives:** Custom FFI per backend.
- **Consequences:** Rust crates must implement the uniform API.
- **Affected IDs:** [REQ-51], [REQ-58]
- **Verification:** Build optional Rust adapters and run smoke tests when enabled.

---

## [DEC-MEM-01] Fixed-size string for non-allocating error/metadata
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Use `FixedString<N>` for error messages and short metadata to avoid heap allocation in error/hot paths.
- **Affected IDs:** [REQ-53-03]
- **Verification:** Unit tests; allocation instrumentation shows no alloc in measured loop.

---

## [DEC-MEM-02] Preallocated per-scene samples vector
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Allocate `std::vector<uint64_t>` sized to `reps` before measurement; fill by index.
- **Affected IDs:** [REQ-62], [REQ-77]
- **Verification:** [TEST-28] and [TEST-32].

---

## [DEC-MEM-03] In-place sort for percentile computation
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Sort the preallocated samples vector in-place after measurement for deterministic percentiles.
- **Affected IDs:** [REQ-63], [REQ-38]
- **Verification:** [TEST-15] stats known-samples test.

---

## [DEC-MEM-04] Default stride and alignment policy
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Default stride `width*4`, align buffer base pointer to 64 bytes best-effort.
- **Affected IDs:** [REQ-64], [REQ-65]
- **Verification:** [TEST-30] best-effort alignment test.

---

## [DEC-MEM-05] Harness owns SceneBundle; avoid shared_ptr overhead
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Use a harness-owned container of `{meta, ir, prepared}` and pass `const&` to adapters.
- **Affected IDs:** [REQ-61], [REQ-66]
- **Verification:** Lifetime tests ([TEST-29]) and TSan smoke.

---

## [DEC-MEM-06] Compact prepared command stream representation
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Normalize to a compact `Cmd{u16,u16,u32}` stream plus payload arrays (SoA).
- **Affected IDs:** [REQ-67]
- **Verification:** IR decode/prepare unit tests ([TEST-08]) and adapter prepare tests.

---

## [DEC-MEM-07] Monotonic arena for preparation allocations
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Use an internal bump allocator `Arena` for scene preparation allocations; none in measured loop.
- **Affected IDs:** [REQ-68], [TEST-25], [TEST-26]
- **Verification:** Arena unit tests + measured-loop alloc test.

---

## [DEC-MEM-08] Allocation instrumentation in non-Release builds
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Optional debug allocation counters assert zero allocations during measured loop.
- **Affected IDs:** [REQ-71-03], [TEST-27]
- **Verification:** Null-backend no-alloc test on supported platforms.

---

## [DEC-MEM-09] Default benchmark surface size
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Default surface size is 1024×1024 (CLI override allowed).
- **Affected IDs:** [REQ-72]
- **Verification:** CLI run reports include dimensions; smoke test covers defaults.

---

## [DEC-CONC-01] Multi-thread mode parallelizes reps within a scene
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** For `threads>1`, parallelize repetition slices for a single scene; keep scenes serial and ordered.
- **Affected IDs:** [REQ-75], [REQ-76]
- **Verification:** [TEST-33] stable report order.

---

## [DEC-CONC-02] Do not start empty-slice workers when reps < threads
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Skip workers with empty slices.
- **Affected IDs:** [REQ-77]
- **Verification:** Partition test ([TEST-31]) covers edge cases.

---

## [DEC-CONC-03] Use a start-alignment barrier per scene
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Align worker start with a barrier outside measured loop.
- **Affected IDs:** [REQ-79], [REQ-78]
- **Verification:** Concurrency stress tests; no locks in measured loop.

---

## [DEC-CONC-04] Use std::jthread-based worker pool
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Implement worker pool with `std::jthread` and bounded task queue.
- **Affected IDs:** [REQ-80], [REQ-81]
- **Verification:** Clean shutdown tests; no thread leaks in CI.

---

## [DEC-CONC-05] Add capability flag supports_parallel_render
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Registry exposes `supports_parallel_render`; harness refuses `threads>1` otherwise.
- **Affected IDs:** [REQ-82], [TEST-34]
- **Verification:** Backend refusal test passes.

---

## [DEC-CONC-06] No shared caches in v0.2.0
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Avoid global caches in VGCPU layers for v0.2.0.
- **Affected IDs:** [REQ-83]
- **Verification:** Code review + TSan job; no shared mutable singletons.

---

## [DEC-CONC-07] Best-effort pinning implementation policy
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Implement pinning best-effort using platform APIs; macOS limited support.
- **Affected IDs:** [REQ-86]
- **Verification:** Logging reports pinning success/failure; no crash if unsupported.

---

## [DEC-BUILD-01] Preserve existing ENABLE_* backend option names
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Keep existing CMake option names to avoid breaking workflows/docs.
- **Affected IDs:** [REQ-96]
- **Verification:** Old options still configure; presets toggle tier-1 only.

---

## [DEC-BUILD-02] Preset naming and preset-only CI contract
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Standardize on `dev/release/ci/asan/ubsan/tsan` presets; CI uses presets only.
- **Affected IDs:** [REQ-92], [REQ-106..109], [TEST-39]
- **Verification:** Workflow preset-linter passes; CI builds from presets only.

---

## [DEC-BUILD-03] Backend defaults + Tier-1-only CI mode
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** In `ci`, force optional backends OFF; local defaults may be broader for dev convenience.
- **Affected IDs:** [REQ-95-02], [REQ-110]
- **Verification:** CI matrix builds reliably with Tier-1-only config.

---

## [DEC-BUILD-04] Do not combine sanitizers in presets
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Separate ASan/UBSan/TSan presets/jobs.
- **Affected IDs:** [REQ-97], [REQ-110-04..06]
- **Verification:** Sanitizer jobs stable and platform-appropriate.

---

## [DEC-BUILD-05] Keep FetchContent; enforce deterministic pins
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Continue using FetchContent but reject floating branches in shipped presets.
- **Affected IDs:** [REQ-99], [REQ-100], [TEST-37]
- **Verification:** Configure-time check fails on `master/main`.

---

## [DEC-BUILD-06] Pin Rust toolchain to stable (no floating nightly)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Use a pinned stable Rust toolchain version and `--locked` builds.
- **Affected IDs:** [REQ-102]
- **Verification:** Optional Rust CI job uses pinned toolchain and `Cargo.lock`.

---

## [DEC-BUILD-07] Cache policy for FetchContent + Rust builds
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Cache `_deps` and Rust `target` with keys including dep/version files.
- **Affected IDs:** [REQ-110], [TEST-39]
- **Verification:** CI cache hit rates improve; builds remain correct.

---

## [DEC-BUILD-08] Keep packaging script; expose via CMake target
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Keep existing release packaging script and wrap it in a preset-driven `package` target.
- **Affected IDs:** [REQ-111]
- **Verification:** `cmake --build --preset release --target package` works across OSes.

---

## [DEC-TOOL-01] Tooling/layout decision for legacy blueprint docs
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context/Decision/Rationale:** See [DEC-PRE-INTAKE-01].
- **Affected IDs:** [REQ-115]
- **Verification:** Docs links and quickstart updated.

---

## [DEC-TOOL-02] clang-format style baseline
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Adopt LLVM-based `.clang-format` with explicit overrides (e.g., column limit 100).
- **Affected IDs:** [REQ-116]
- **Verification:** `format_check` target enforced in CI.

---

## [DEC-TOOL-03] Pragmatic clang-tidy checks set
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Enable a low-churn subset of checks; exclude third-party sources from analysis.
- **Affected IDs:** [REQ-118], [REQ-119]
- **Verification:** `lint` job runs on Linux; fails on new warnings in CI.

---

## [DEC-TOOL-04] Include-boundary linter via tools/check_includes.py
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Enforce layering via a custom include linter integrated as a CMake gate target.
- **Affected IDs:** [REQ-120], [TEST-41]
- **Verification:** Include violation fixtures fail deterministically.

---

## [DEC-TOOL-05] Lightweight internal logging framework
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Implement minimal internal logging with console + JSONL sinks; keep schema stable.
- **Affected IDs:** [REQ-02], [REQ-123]
- **Verification:** JSONL validator tool; smoke run produces valid logs.

---

## [DEC-TOOL-06] Optional local git hooks (not mandatory)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Provide hook installer scripts; CI remains source-of-truth.
- **Affected IDs:** [REQ-112], [REQ-126]
- **Verification:** Hooks are best-effort; CI gates enforce quality regardless.

---

## [DEC-VER-01] Product v0.2.0 milestone alignment
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context/Decision/Rationale:** See [DEC-META-01].
- **Affected IDs:** [VER-01]
- **Verification:** Release tagging and notes.

---

## [DEC-VER-02] Schema version is independent and monotonic
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** `schema_version` is a monotonic integer, independent from product SemVer.
- **Affected IDs:** [REQ-133]
- **Verification:** Schema validators and tests enforce presence and bump rules.

---

## [DEC-VER-03] CR threshold for “small changes”
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Skip CR only for changes that do not affect compatibility surfaces or [REQ]/[TEST] contracts.
- **Affected IDs:** [REQ-137], [REQ-138]
- **Verification:** Review policy; CI still gates all changes.

---

## [DEC-VER-04] Perf regression thresholds (coarse)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Gate on ≥15% p50/p90 regressions with confirmation rerun; warn at 10–15%.
- **Affected IDs:** [REQ-143..146]
- **Verification:** Perf diff tooling + policy tests ([TEST-48]).

---

## [DEC-VER-05] Store perf baselines in-repo for v0.2.0
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Keep baselines under `/perf/baselines/**` and require CR to update.
- **Affected IDs:** [REQ-144]
- **Verification:** CI compares against committed baseline; baseline update PRs require review.

---

## [DEC-VER-06] Migration expectations for v0.2.0
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Treat legacy blueprint move and additive schema_version fields as non-breaking unless behavior changes; review any CLI/CSV order changes as potentially breaking.
- **Affected IDs:** [REQ-147]
- **Verification:** Migration doc produced; CLI/schema tests enforce stability.

---

