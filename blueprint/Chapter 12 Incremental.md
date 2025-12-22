<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Chapter 12 — Incremental Delivery Plan (Milestones, Slices, Backlog)

## Purpose

This chapter defines an incremental delivery plan that produces a usable benchmark suite early and expands capability over time. It specifies milestones, delivery slices, and a structured backlog with clear acceptance targets aligned to reproducibility, fairness, and cross-platform constraints.

## 12.1 Delivery Strategy

1. The project **MUST** be delivered via incremental milestones that each produce a runnable CLI with measurable outputs.
2. Each milestone **MUST** include:

   * updated documentation for new capabilities,
   * pinned dependency records,
   * schema/IR versioning updates as necessary,
   * CI validation coverage consistent with introduced scope.
3. Each milestone **SHOULD** add either:

   * one new backend adapter, or
   * one meaningful new scene category, or
   * one significant harness capability (e.g., isolation mode), but not all at once unless required.

## 12.2 Milestones

### 12.2.1 Milestone M0 — Repository Skeleton and Build Foundation

**Scope**

1. CMake build scaffolding for core modules (`cli`, `harness`, `pal`, `ir`, `reporting`, `assets`).
2. Minimal CLI with `--help`, `metadata`, and `validate` commands.
3. Scene manifest loading + IR asset presence and hash verification (no rendering yet).
4. Output schema v0.1 draft and metadata emission (no case timings).

**Acceptance Criteria**

* `cmake .. && cmake --build .` succeeds on Linux/Windows/macOS (x86_64 baseline; ARM64 where available).
* `benchmark validate` validates manifest + IR hashes deterministically.
* `benchmark metadata` outputs non-identifying environment + build metadata.

### 12.2.2 Milestone M1 — First End-to-End Benchmark (Single Backend, Single Scene)

**Scope**

1. Implement IR runtime parsing/validation/preparation for the initial IR subset:

   * paths, solid fill, transforms, source-over.
2. Implement a first adapter (“starter backend”) chosen for portability and headless CPU output.
3. Implement harness timed loop with warm-up and measurement; capture wall + CPU time.
4. Emit JSON results with per-case stats (p50 + dispersion) and run metadata.
5. Add 1–2 simple scenes:

   * `fills/solid_basic`
   * `paths/curves_basic`

**Acceptance Criteria**

* `run` executes at least one case successfully and emits JSON results with required fields.
* Timed region excludes IR parsing and I/O (validated by code review + diagnostic timings).
* Backend metadata reports CPU-only surface selection.

### 12.2.3 Milestone M2 — Capability Matrix + Skips + CSV + Summary

**Scope**

1. Implement CapabilitySet and compatibility decision engine.
2. Add skip behavior with structured reason codes and default skip policy.
3. Implement CSV output and human-readable summary report.
4. Expand IR to include stroke basics (width, caps, joins, miter limit) for at least one scene.
5. Add 2–3 additional scenes to exercise:

   * stroke caps/joins,
   * transforms stacking.

**Acceptance Criteria**

* Unsupported scene/backend combinations are skipped by default with recorded reasons.
* Both JSON and CSV outputs are generated and schema-versioned.
* Summary report lists executed/skipped/failed cases with reasons.

### 12.2.4 Milestone M3 — Gradients + More Backends (Breadth Expansion I)

**Scope**

1. Add linear and radial gradient paint objects to IR runtime and adapter mappings.
2. Integrate 2–3 additional backends (prioritized for build ease).
3. Add scenes explicitly exercising gradients:

   * `fills/linear_gradient_basic`
   * `fills/radial_gradient_basic`
4. Document backend capability matrix in repo docs.

**Acceptance Criteria**

* Gradient scenes run on at least one backend; others may skip with explicit reasons.
* Backend inclusion/exclusion is reported at runtime and recorded in results metadata.
* CI smoke tests cover at least one gradient case.

### 12.2.5 Milestone M4 — Dashes + Extensive Stroke Coverage (Breadth Expansion II)

**Scope**

1. Add dash patterns and offsets to IR runtime and adapter mappings.
2. Add scenes for:

   * dashed strokes,
   * miter limit edge cases,
   * mixed fills + strokes.
3. Integrate additional backends (target 6–7 total integrated).

**Acceptance Criteria**

* Dash scenes execute successfully on at least one backend that supports dashes.
* Unsupported backends are skipped with `UNSUPPORTED_FEATURE:dashes`.
* Results include stable stats and updated capability flags.

### 12.2.6 Milestone M5 — Cross-Platform Release Packaging (Source + Binaries)

**Scope**

1. GitHub Actions (or equivalent) to build:

   * source release artifact,
   * binary artifacts per OS/arch where feasible.
2. Include assets bundle, schemas, deps pin records, checksums.
3. Document installation and usage for binary releases.
4. Validate that runtime adapter availability matches packaged adapters.

**Acceptance Criteria**

* A tagged release produces downloadable binaries with checksums.
* Running the binary release on each platform produces valid outputs and metadata.
* Release notes include dependency and backend version identifiers.

### 12.2.7 Milestone M6 — Optional Isolation Mode + Diagnostics

**Scope**

1. Implement optional process isolation mode with versioned IPC schema.
2. Add worker crash/timeout handling and reason codes.
3. Add secondary diagnostics (init time, parse time) behind flags.

**Acceptance Criteria**

* Isolation mode produces output identical in schema to single-process mode.
* Worker failures are contained and reported with structured reason codes.
* Diagnostic mode is opt-in and marked in metadata.

### 12.2.8 Milestone M7 — Full Target Backend Set + Stabilization

**Scope**

1. Integrate remaining backends, targeting the full list where feasible.
2. Stabilize IR semantics and output schema at v1.0.
3. Expand scene suite to cover representative workloads (balanced complexity).
4. Finalize documentation, reproducibility kit, and governance policies.

**Acceptance Criteria**

* All feasible backends compile and run on at least one platform configuration.
* The suite supports a stable “standard run” preset producing comparable outputs.
* IR v1.0 and result schema v1.0 are published with migration notes if needed.

## 12.3 Backlog (Structured)

### 12.3.1 Core Backlog Items (P0)

1. Implement IR header/section parser with robust validation and error reporting.
2. Implement PreparedScene immutable representation and caching.
3. Implement timing abstraction (monotonic + CPU time) for all platforms.
4. Implement baseline harness loops (warm-up + measurement) and statistics engine.
5. Implement JSON schema v0.1 and validator tests.
6. Implement capability matrix evaluation and skip/fail policies.
7. Implement at least one backend adapter end-to-end with CPU-only enforcement.

### 12.3.2 Expansion Backlog Items (P1)

1. Add additional backends incrementally with per-backend build toggles and metadata.
2. Add gradients, then dashes, then optional clipping if semantics can be defined.
3. Add CSV output and summary report improvements (grouping by backend/scene).
4. Add release packaging workflows and checksums.

### 12.3.3 Optional Backlog Items (P2)

1. Isolation mode (multi-process) worker protocol and resilience features.
2. Allocation and RSS metrics (platform-dependent).
3. Trace output (diagnostic only) and CI-friendly minimal modes.

## 12.4 Presets and Standard Runs

1. The suite **SHOULD** define named presets (e.g., `--preset quick`, `--preset standard`) that map to:

   * a defined set of scenes/backends,
   * fixed warm-up/iteration policies,
   * fixed output formats.
2. Preset definitions **MUST** be versioned and included in metadata for reproducibility.

## Acceptance Criteria

1. The plan defines milestones that each produce a runnable, testable deliverable with clear acceptance criteria.
2. Milestones are sequenced to deliver an end-to-end benchmark early and expand scope safely.
3. The backlog is prioritized and aligned with earlier chapters’ requirements (reproducibility, fairness, cross-platform).
4. Release packaging is included as an explicit milestone consistent with GitHub Releases distribution.

## Dependencies

1. Chapter 2 — Requirements (deliverables, reproducibility, reporting).
2. Chapter 7 — Component Design (modules and extension procedures).
3. Chapter 11 — Deployment Architecture (environments and packaging).

---

