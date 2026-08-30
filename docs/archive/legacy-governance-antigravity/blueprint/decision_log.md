# Decision Log — VGCPU-Benchmark (Append-only)

Date created: 2025-12-23
Maintainer: repository owner(s)

> Rule: This file is **append-only**. Do not edit or reorder past entries. If a decision changes, add a new entry that supersedes the old one and links to it.

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

## [DEC-PRE-INTAKE-01] Canonical /blueprint directory; move legacy docs
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** The repo already contains `/blueprint/` legacy docs not matching the canonical structure and filenames required by this blueprint schema.
- **Decision:** Move existing `/blueprint/` to `/docs/legacy_blueprint/` and create the canonical `/blueprint/` structure for this blueprint set.
- **Rationale:** Avoid naming collisions and ensure deterministic governance artifacts.
- **Alternatives:** (A) Rename the canonical blueprint directory (rejected; violates repo convention). (B) Delete legacy docs (rejected; lose context).
- **Consequences:** Links/docs must be updated; contributors follow the new canonical `/blueprint/`.
- **Affected IDs:** [DEC-ARCH-05], [DEC-TOOL-01], [REQ-115], [REQ-147]
- **Verification:** CI/doc lint ensures canonical blueprint files exist; migration doc updated ([REQ-147]).

---

## [DEC-ARCH-05] Repo layout normalized to canonical targets
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** The repo layout includes multiple experimental targets and varying include paths; enforce canonical layout for reproducibility.
- **Decision:** Adopt `/src`, `/include`, `/tests`, `/tools`, `/examples`, `/docs`, `/blueprint` with CMake targets aligned to these directories.
- **Rationale:** Deterministic structure enables automation and simplifies CI gates.
- **Alternatives:** Keep ad-hoc layout (rejected; hard to maintain and automate).
- **Consequences:** New modules must conform; includes are validated by tooling.
- **Affected IDs:** [REQ-115], [REQ-120]
- **Verification:** `check_includes` and build succeed across presets.

---

## [DEC-TOOL-01] Tooling/layout decision for legacy blueprint docs
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context/Decision/Rationale:** See [DEC-PRE-INTAKE-01].
- **Affected IDs:** [REQ-115]
- **Verification:** Docs links and quickstart updated.

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
- **Affected IDs:** [REQ-01-04], [BUILD-05]
- **Verification:** CI builds using the defined compiler matrix without needing C++23 flags.

---

## [DEC-SCOPE-02] Tier-1 backends are minimal (CI stability)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Optional backends have heavy deps and variable platform support; CI must stay reliable.
- **Decision:** Define Tier-1 backends as: `null`, `plutovg`, `blend2d`. Treat others as optional.
- **Rationale:** Keep CI signal strong and prevent flakes due to optional backends.
- **Alternatives:** Make Skia/Cairo Tier-1 (rejected; increases fragility and build times).
- **Consequences:** CI jobs must at minimum test Tier-1; optional backends are best-effort.
- **Affected IDs:** [REQ-121], [REQ-122]
- **Verification:** CI runs Tier-1 build + smoke tests on all OSes.

---

## [DEC-BUILD-03] Use vcpkg + vendoring where needed; avoid system packages
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** System packages vary widely across OSes/CI runners; reproducibility is critical.
- **Decision:** Prefer vcpkg for C/C++ deps; vendor single-header/single-file libs when appropriate; avoid relying on system packages.
- **Rationale:** Deterministic dependency resolution.
- **Alternatives:** System packages (rejected), Conan (deferred).
- **Consequences:** CI sets up vcpkg; dependency updates are explicit.
- **Affected IDs:** [REQ-103], [REQ-104], [REQ-105]
- **Verification:** Clean builds on fresh runners succeed via presets.

---

## [DEC-SCOPE-03] Reports are a compatibility surface; schema version is explicit
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Context:** Downstream tooling consumes CSV/JSON. Silent schema drift breaks users.
- **Decision:** Treat report schemas as versioned compatibility surfaces; include explicit schema version markers.
- **Rationale:** Supports deterministic tooling upgrades.
- **Alternatives:** Best-effort schema changes (rejected).
- **Consequences:** Schema changes require version bumps + migration notes.
- **Affected IDs:** [REQ-86], [REQ-87], [REQ-88]
- **Verification:** Schema tests run in CI on smoke artifacts ([REQ-125]).

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
- **Context:** Static linking into a CLI; no external consumers promised.
- **Decision:** Treat `vgcpu_*` libs as internal implementation, not a stable SDK/ABI.
- **Rationale:** Maintain velocity and avoid ABI lock-in.
- **Alternatives:** Publish an SDK and guarantee ABI (rejected).
- **Consequences:** Headers use `include/vgcpu/internal/**`; no install target by default.
- **Affected IDs:** [DEC-API-02], [REQ-120]
- **Verification:** Include-lint and visibility rules enforce boundary; no accidental exports.

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
- **Decision:** No ABI stability guarantee across minor; still use visibility/export macros consistently.
- **Rationale:** Avoid accidental ABI commitments while keeping code hygiene.
- **Alternatives:** Strict ABI guarantees (rejected), no export macros (rejected).
- **Consequences:** Any future shared library must revisit ABI rules via CR.
- **Affected IDs:** [DEC-ARCH-01], [REQ-120]
- **Verification:** Build artifacts show limited exported symbols; symbol checks in CI.

---

## [DEC-ARCH-02] Asset manifest is the single source of truth for scenes
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Use `assets/manifest.json` to enumerate available scenes and metadata.
- **Rationale:** Deterministic scene selection and stable IDs.
- **Alternatives:** Directory scan at runtime (rejected; nondeterministic ordering).
- **Consequences:** Adding/removing scenes requires manifest update.
- **Affected IDs:** [REQ-62], [REQ-63]
- **Verification:** Tests validate manifest consistency.

---

## [DEC-ARCH-03] Canonical render output format = RGBA8 premultiplied
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Adapter outputs are normalized to RGBA8 premultiplied alpha.
- **Rationale:** Unified pipeline for reporting and future correctness checks.
- **Alternatives:** BGRA; floating-point; separate alpha.
- **Consequences:** Adapters must convert internally if needed.
- **Affected IDs:** [REQ-69], [REQ-70]
- **Verification:** Adapter conformance tests for known patterns.

---

## [DEC-ARCH-04] Statistics computed in harness (not backend-specific)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Collect raw timings and compute summary stats in the harness.
- **Rationale:** Comparable results across backends.
- **Alternatives:** Backend-provided stats (rejected).
- **Consequences:** Harness owns measurement policy; backends focus on rendering.
- **Affected IDs:** [REQ-04], [REQ-05]
- **Verification:** Stats unit tests.

---

## [DEC-ARCH-06] Output directory created on demand; fail fast on errors
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Create output directory if missing; treat failures as fatal.
- **Affected IDs:** [REQ-84], [REQ-85]
- **Verification:** Integration tests simulate unwritable paths.

---

## [DEC-ARCH-07] Structured logging as JSONL option
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Support JSON Lines logging mode for CI and machine parsing.
- **Affected IDs:** [REQ-02-02b], [REQ-02-01]
- **Verification:** Log schema test and sample artifact validation.

---

## [DEC-ARCH-08] Prefer monotonic clocks; abstract platform timing
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Use a platform abstraction for monotonic timing.
- **Affected IDs:** [REQ-04-03]
- **Verification:** Unit tests compare monotonic progression; platform builds.

---

## [DEC-ARCH-09] Adapter registry IDs are stable and lowercase
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Backend IDs are lowercase and stable; CLI uses these IDs.
- **Affected IDs:** [REQ-06-01], [API-06-02]
- **Verification:** Registry uniqueness check and CLI listing test.

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

## [DEC-API-03] Error model = Status/Result, no exceptions across boundaries
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Use `Status` / `Result<T>`; exceptions are not allowed across module boundaries.
- **Affected IDs:** [REQ-53]
- **Verification:** Code review + compile flags; tests assert error codes.

---

## [DEC-API-04] CLI flag naming: kebab-case long flags
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Use `--long-flag` names; avoid short flags except `-h`/`--help`.
- **Affected IDs:** [REQ-06], [REQ-76]
- **Verification:** CLI help snapshot tests.

---

## [DEC-API-05] Include boundary check: prevent internal headers from leaking
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Add an include boundary check gate.
- **Affected IDs:** [REQ-120]
- **Verification:** `check_includes` target in CI.

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
- **Verification:** Unit tests; allocation audits.

---

## [DEC-MEM-02] Pre-allocate render buffers; no measured-loop resizing
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Allocate/resize render buffers before warmup/measurement; reuse across iterations.
- **Affected IDs:** [REQ-12], [REQ-69]
- **Verification:** Hot-path instrumentation tests; code review.

---

## [DEC-MEM-03] Use std::vector for pixel buffers; spans for views
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Store pixels in `std::vector<uint8_t>`; pass as `std::span<const uint8_t>` to helpers.
- **Affected IDs:** [REQ-69], [REQ-70]
- **Verification:** Unit tests validate sizes and lifetimes.

---

## [DEC-MEM-04] Keep IR data immutable after load
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Treat loaded IR and prepared scenes as immutable to simplify concurrency and determinism.
- **Affected IDs:** [REQ-03]
- **Verification:** Const-correctness checks; tests.

---

## [DEC-MEM-05] Avoid per-iteration logging and allocation
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** No per-iteration logs/allocations in measured loops; only aggregate outputs.
- **Affected IDs:** [REQ-02-03], [REQ-12]
- **Verification:** Performance hygiene tests.

---

## [DEC-MEM-06] Normalize paths in reports to avoid leaking user info
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Avoid reporting full user home paths by default.
- **Affected IDs:** [REQ-15-01]
- **Verification:** Report snapshot tests.

---

## [DEC-MEM-07] Store schema_version in JSON; CSV header marker
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** JSON includes `schema_version`; CSV includes a header marker or dedicated field.
- **Affected IDs:** [DEC-SCOPE-03], [REQ-86..88]
- **Verification:** Schema tests on generated artifacts.

---

## [DEC-MEM-08] Sanitize user-provided names before filesystem use
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Sanitize scene/backend identifiers for filesystem-safe output names.
- **Affected IDs:** [REQ-84], [REQ-85]
- **Verification:** Unit tests for sanitization.

---

## [DEC-MEM-09] Limit artifact filename component length; add hash on truncation
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** If sanitized name exceeds max length, truncate and append stable hash suffix.
- **Affected IDs:** [REQ-84], [REQ-85]
- **Verification:** Unit tests.

---

## [DEC-CONC-01] Single orchestrator thread; backends are black boxes
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Harness orchestration is single-threaded by default; backend threading is opaque.
- **Affected IDs:** [REQ-75]
- **Verification:** Determinism tests and log order checks.

---

## [DEC-CONC-02] Parallel benchmarking requires a dedicated CR
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Do not add parallel case execution without explicit CR and determinism analysis.
- **Affected IDs:** [REQ-75], [REQ-145]
- **Verification:** Review policy.

---

## [DEC-CONC-03] No cancellation/timeouts in baseline
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** No run cancellation/timeouts; user controls duration via CLI.
- **Affected IDs:** [REQ-76]
- **Verification:** N/A (policy).

---

## [DEC-CONC-04] Logging is thread-safe; emitted from orchestrator by default
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Logging is thread-safe, but harness emits events from orchestrator thread by default.
- **Affected IDs:** [REQ-02]
- **Verification:** Stress tests.

---

## [DEC-CONC-05] Deterministic ordering for scenes/backends
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Stable ordering for scenes and backends; explicit ordering is logged.
- **Affected IDs:** [REQ-13]
- **Verification:** Ordering tests.

---

## [DEC-CONC-06] Prefer std::chrono::steady_clock behind PAL
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Use steady clock abstraction; no wall-clock time for measurements.
- **Affected IDs:** [REQ-04-03]
- **Verification:** Platform tests.

---

## [DEC-CONC-07] TSan is best-effort; allow curated exclusions
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Run TSan on curated configs; some third-party backends may be excluded.
- **Affected IDs:** [REQ-126]
- **Verification:** CI TSan job.

---

## [DEC-BUILD-08] Keep packaging script; expose via CMake target
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Keep existing release packaging script and wrap it in a preset-driven `package` target.
- **Affected IDs:** [REQ-111]
- **Verification:** `cmake --build --preset release --target package` works across OSes.

---

## [DEC-TOOL-02] clang-format is the formatting source-of-truth
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Enforce clang-format on all C/C++ sources in CI.
- **Affected IDs:** [REQ-112]
- **Verification:** `format`/`format_check` targets in CI.

---

## [DEC-TOOL-03] clang-tidy is optional locally; required in CI on curated configs
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Run clang-tidy in CI on curated presets; keep it optional locally.
- **Affected IDs:** [REQ-113], [REQ-126]
- **Verification:** CI tidy job.

---

## [DEC-TOOL-04] TEMP debug code policy is strict; CI fails with markers
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** `[TEMP-DBG]` markers are the only acceptable TEMP debug mechanism; CI fails if any remain.
- **Affected IDs:** [REQ-114]
- **Verification:** `check_temp_dbg` gate.

---

## [DEC-TOOL-05] Observability uses structured logs and run metadata
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Structured logs include run metadata and outcomes; JSONL option supported.
- **Affected IDs:** [REQ-02]
- **Verification:** Log schema tests.

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
- **Decision:** `schema_version` increments monotonically and is independent of product version.
- **Affected IDs:** [DEC-SCOPE-03]
- **Verification:** Schema tests.

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
- **Decision:** Perf gates in CI detect only large regressions; rerun confirmation required.
- **Affected IDs:** [DEC-SCOPE-04], [REQ-143..146]
- **Verification:** Perf tooling tests.

---

## [DEC-VER-05] Dependency update cadence is explicit via CR
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Dependency bumps require a CR, pinned revisions, and license verification.
- **Affected IDs:** [REQ-104], [REQ-105]
- **Verification:** CI build and notice checks.

---

## [DEC-VER-06] Security fixes can bypass normal cadence but not CI gates
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.0.0
- **Decision:** Security hotfixes may be expedited, but must still pass CI and update notices.
- **Affected IDs:** [REQ-106]
- **Verification:** Release checklist.

---

---

## [DEC-SCOPE-05] PNG artifact naming: <scene>_<backend>.png in a flat folder
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** PNG artifacts are produced per (scene, backend). Per-backend subfolders were explicitly rejected; collisions must be avoided.
- **Decision:** Write artifacts to `<output_dir>/png/` using filename `<scene_sanitized>_<backend_id>.png`.
- **Rationale:** Flat folder is convenient for browsing/globbing while backend suffix prevents collisions.
- **Alternatives:** (A) Per-backend subfolders (rejected by user). (B) `<backend>__<scene>.png` (less consistent with requested naming).
- **Consequences:** Backend ids must be unique and stable; sanitization must be deterministic.
- **Affected IDs:** [REQ-148], [REQ-150], [ARCH-06-04], [API-06-02]
- **Verification:** Integration test asserts expected filenames and that multiple backends do not collide.

---

## [DEC-ARCH-11] Post-benchmark render for artifacts (no timing contamination)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** Encoding PNG and computing SSIM involve I/O and CPU work that would distort benchmark timings if performed in measured loops.
- **Decision:** Perform a separate untimed render pass after timings are computed, and use that buffer for PNG + SSIM.
- **Rationale:** Ensures measured results reflect backend render cost only, not artifact/verification overhead.
- **Alternatives:** (A) Reuse the last measured-iteration buffer (risk: measured-loop side effects and encoding overlap). (B) Disable artifacts whenever benchmarking (does not meet requirements).
- **Consequences:** Backends must support an extra render; harness must clearly separate phases.
- **Affected IDs:** [REQ-148], [REQ-151], [MEM-01-02], [TEST-55-04]
- **Verification:** Tests assert artifact counters remain zero during measured iterations and become non-zero only after measurement.

---

## [DEC-ARCH-12] SSIM requires built/enabled ground truth backend; fail if missing
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** SSIM comparisons require a ground truth image; silently skipping comparisons is misleading.
- **Decision:** If `--compare-ssim` is enabled and the specified `--ground-truth-backend` is not available/enabled, fail before running comparisons.
- **Rationale:** Prevents false confidence and makes configuration problems obvious.
- **Alternatives:** Skip comparisons with warning (rejected; hides failures).
- **Consequences:** CLI must validate backend availability early and provide a list of valid backend ids.
- **Affected IDs:** [REQ-153], [API-03-05b], [ARCH-04-02], [TEST-54-03]
- **Verification:** CLI contract test: missing GT backend → non-zero exit with actionable message.

---

## [DEC-ARCH-13] Lightweight vendored SSIM implementation (no heavy deps)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** SSIM is required, but heavy image/toolkit dependencies would bloat builds and complicate CI.
- **Decision:** Use a lightweight, permissive-licensed, vendorable SSIM implementation (single header or small TU), pinned and recorded in notices.
- **Rationale:** Keeps the dependency graph small and reproducible.
- **Alternatives:** (A) OpenCV (rejected: heavy). (B) Custom SSIM implementation (rejected: maintenance risk).
- **Consequences:** We wrap SSIM to match RGBA premul inputs and provide deterministic aggregation.
- **Affected IDs:** [REQ-152], [BUILD-02], [API-21]
- **Verification:** Unit tests validate SSIM correctness on synthetic patterns; CI verifies vendored hashes.

---

## [DEC-ARCH-14] Default SSIM threshold is 0.99 (configurable)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** Users need a meaningful default threshold that detects visual regressions but remains tunable.
- **Decision:** Default `--ssim-threshold` is **0.99**, validated within `[0,1]`, and comparisons fail when SSIM < threshold.
- **Rationale:** Strict by default while accommodating minor backend/platform differences by user override.
- **Alternatives:** 0.995 (stricter; more flakiness). 0.95 (looser; may miss regressions).
- **Consequences:** CI pipelines may tune threshold per backend/platform; artifacts must be kept for failure triage.
- **Affected IDs:** [REQ-154], [API-03-04], [API-02-04], [VER-14-02]
- **Verification:** Integration test forces a known-different image and asserts exit code 4 when below threshold.

---

## [DEC-ARCH-15] PNG write failures are fatal when artifacts are enabled
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** When users enable artifact output, silent failures defeat the purpose and make SSIM triage impossible.
- **Decision:** Fail fast on PNG write/encode errors when PNG output is enabled.
- **Rationale:** Preserves user expectations and avoids partial/inconsistent artifact sets.
- **Alternatives:** Best-effort continue with warnings (deferred as future option).
- **Consequences:** Error handling must clearly report path + OS error; reports written up to failure are still valid.
- **Affected IDs:** [REQ-148], [API-20-02], [ARCH-17-02]
- **Verification:** Unit test simulates unwritable directory and asserts non-zero exit and error message.

---

## [DEC-MEM-10] Tight stride requirement: stride == width*4 in v1.1
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** PNG writer and SSIM comparator assume a simple contiguous layout; allowing arbitrary stride complicates correctness and increases copy risk.
- **Decision:** Enforce `stride_bytes == width*4` for adapter outputs in v1.1.
- **Rationale:** Simplifies artifact pipeline; matches observed adapter behavior.
- **Alternatives:** Support padded stride and propagate stride throughout (requires broader API/data changes).
- **Consequences:** Adapters requiring padding must internally convert/copy to tight stride before returning.
- **Affected IDs:** [API-04-01b], [MEM-02-01], [REQ-149]
- **Verification:** Adapter contract tests validate buffer sizes and stride for Tier-1 backends.

---

## [DEC-MEM-11] SSIM compares premultiplied RGBA directly (no un-premultiply)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** User requirement: compare premultiplied RGBA directly; avoid additional color/alpha transforms.
- **Decision:** Compute SSIM per channel on premultiplied RGBA8 and aggregate deterministically.
- **Rationale:** Minimal transforms; consistent with buffer contract.
- **Alternatives:** Un-premultiply; ignore alpha (RGB only).
- **Consequences:** Edge/coverage differences may reduce SSIM; threshold must be configurable.
- **Affected IDs:** [REQ-151], [API-21-03], [REQ-154]
- **Verification:** Unit tests cover identical vs perturbed buffers and confirm expected SSIM behavior.

---

## [DEC-API-07] SSIM implies PNG artifacts for diagnosability
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** SSIM failures must be debuggable with concrete image artifacts.
- **Decision:** When `--compare-ssim` is enabled, PNG artifacts are generated regardless of `--png` flag (effectively force-enabled).
- **Rationale:** Ensures every SSIM result can be inspected visually.
- **Alternatives:** Allow SSIM without writing PNGs (rejected; poor triage).
- **Consequences:** `--help` and docs must state this implication clearly.
- **Affected IDs:** [REQ-151], [REQ-148], [API-03-06]
- **Verification:** CLI test asserts `--compare-ssim` produces PNGs even without `--png`.

---

## [DEC-API-08] SSIM library selection: ChrisLomont/SSIM (MIT), vendored
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** Need a lightweight SSIM implementation that can be vendored and built everywhere.
- **Decision:** Vendor the single-file SSIM implementation from Chris Lomont’s SSIM project (MIT).
- **Rationale:** Minimal footprint, permissive license, easy to integrate.
- **Alternatives:** Other small SSIM snippets; heavier dependencies like OpenCV (rejected).
- **Consequences:** Must record upstream provenance and license in `THIRD_PARTY_NOTICES.md` and pin via SHA-256 verification.
- **Affected IDs:** [REQ-152], [BUILD-02-02], [REQ-155]
- **Verification:** SSIM unit tests + third-party hash check in CI.

---

## [DEC-CONC-08] Serialize artifact I/O to avoid races/corruption
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** Artifact writing uses filesystem I/O; future parallelism could create races or partial files.
- **Decision:** Serialize artifact directory creation and PNG writes via a dedicated mutex (or on the orchestrator thread).
- **Rationale:** Deterministic, safe output; avoids write collisions.
- **Alternatives:** Lock-free unique paths; per-scene locks.
- **Consequences:** Optional artifact phase remains sequential; acceptable because it is out-of-band and user-controlled.
- **Affected IDs:** [CONC-03-02], [CONC-07-01], [REQ-148]
- **Verification:** TSan-enabled tests include an artifacts-enabled run path.

---

## [DEC-BUILD-20] Vendor stb_image_write for PNG output
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Decision:** Vendor `stb_image_write.h` under `third_party/stb/` and compile exactly one TU with `STB_IMAGE_WRITE_IMPLEMENTATION`.
- **Rationale:** Stable, dependency-free PNG encoding.
- **Alternatives:** libpng; lodepng.
- **Consequences:** Must add notices and ensure no ODR violations.
- **Affected IDs:** [REQ-149], [BUILD-04-03], [REQ-155]
- **Verification:** Unit test writes a PNG and validates it begins with PNG signature bytes; CI builds all presets.

---

## [DEC-BUILD-21] Vendor SSIM single-file implementation and pin provenance
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Decision:** Vendor selected SSIM implementation under `third_party/ssim_lomont/` and record revision/hash.
- **Rationale:** Reproducible builds without network fetch.
- **Alternatives:** FetchContent at configure time (rejected: network variability).
- **Consequences:** Hash verification gate required; notices updated.
- **Affected IDs:** [DEC-API-08], [REQ-152], [REQ-155]
- **Verification:** Hash verification check passes in CI; mismatch intentionally fails.

---

## [DEC-BUILD-22] Treat third_party includes as SYSTEM to preserve Werror cleanliness
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Decision:** Add `third_party/` include dirs as `SYSTEM` for compilation units that include vendored headers.
- **Rationale:** Prevent third-party warnings from failing CI Werror builds.
- **Alternatives:** Patch upstream headers; disable Werror globally.
- **Consequences:** Keep project code warning-clean; vendor headers remain unchanged.
- **Affected IDs:** [BUILD-04-02]
- **Verification:** Werror CI job remains green with new deps.

---

## [DEC-BUILD-24] No PNG decode dependency; compare raw buffers in-memory
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** Ground truth is regenerated each run and comparisons happen in the same process.
- **Decision:** Perform SSIM on in-memory raw RGBA buffers and write PNGs only as artifacts for inspection.
- **Rationale:** Avoid adding image decode libs; reduce I/O and dependency surface.
- **Alternatives:** Decode PNGs from disk for SSIM (rejected).
- **Consequences:** SSIM results always correspond to the same-run ground truth; artifacts are always present when SSIM enabled.
- **Affected IDs:** [REQ-151], [DEC-API-07], [BUILD-02]
- **Verification:** Integration tests run SSIM with PNGs written and compare metrics.

---

## [DEC-TOOL-08] Verify vendored third-party files via recorded SHA-256 in CI
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** Single-file vendored deps can drift silently; we need deterministic provenance.
- **Decision:** Record SHA-256 for each vendored file in CMake metadata and fail CI if files do not match expected hashes when verification is enabled.
- **Rationale:** Ensures reproducible builds and auditability.
- **Alternatives:** Manual review only; rely on git history alone.
- **Consequences:** Updating vendored deps requires a CR and hash refresh.
- **Affected IDs:** [REQ-155], [TEST-70], [VER-16-01]
- **Verification:** CI job with `VGCPU_VERIFY_THIRD_PARTY_HASHES=ON` passes; mismatch intentionally fails.

---

## [DEC-VER-07] SSIM failures are correctness regressions (blocking when enabled)
- **Date:** 2025-12-23
- **Status:** Active
- **Introduced In:** v1.1
- **Context:** SSIM comparisons are introduced to detect correctness drift across backends.
- **Decision:** When SSIM gating is enabled in a pipeline/run, any SSIM < threshold is treated as a correctness regression and causes non-zero exit (exit code 4).
- **Rationale:** Encourages early detection and prevents silent correctness drift.
- **Alternatives:** Advisory-only metric (non-blocking).
- **Consequences:** Pipelines must retain PNG artifacts and reports for debugging.
- **Affected IDs:** [REQ-154], [API-02-04], [VER-14-02]
- **Verification:** Regression test asserts exit code 4 and artifact retention on failure.

---
