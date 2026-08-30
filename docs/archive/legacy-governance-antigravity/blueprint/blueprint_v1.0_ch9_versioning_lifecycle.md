# Chapter 9 — Versioning, Lifecycle, Governance, Perf Regression Policy

This chapter establishes the operational rules for evolving VGCPU-Benchmark and the blueprint itself, aligned with:
- `cr_template.md` (change request governance)
- `decision_log_template.md` (append-only decision log)
- `performance_benchmark_harness.md` (perf regression harness & gates)
- `dependency_policy.md` (deps cadence + pinning)
- `ci_reference.md` (CI gates)

## [VER-01] Versioning model (product vs blueprint)
- [VER-01-01] The **product** (VGCPU-Benchmark) uses **SemVer**: `MAJOR.MINOR.PATCH`.
- [VER-01-02] The **blueprint** uses its own SemVer-like versioning, independent of product:
  - Blueprint version for this set: **v1.0** ([META-01-02]).
- [DEC-VER-01] Product target for the modernization milestone described by blueprint v1.0 is **v0.2.0** (see [DEC-META-01]).
  - Consequences: release notes for v0.2.0 MUST reference the adoption of blueprint v1.0 and any breaking changes.

## [VER-02] SemVer rules (product)
### [REQ-131] SemVer change classification (binding)
A change is **MAJOR** if it breaks any compatibility surface:
- [REQ-131-01] CLI flags/commands contract ([REQ-46], [REQ-47])
- [REQ-131-02] Backend stable names ([REQ-33])
- [REQ-131-03] Report schemas (CSV/JSON) when `schema_version` changes incompatibly ([REQ-48..50])

A change is **MINOR** if it is backward compatible and additive:
- new CLI flags (non-breaking defaults),
- new optional report fields (JSON) with stable `schema_version`,
- new optional backends.

A change is **PATCH** if it is a bugfix that does not alter compatibility surfaces.

### [REQ-132] Version stamping (binding)
- [REQ-132-01] Product version MUST be defined in one place (CMake `project(VERSION ...)`) and exported to:
  - `VGCPU_VERSION_*` macros (`include/vgcpu/version.h`) ([API-02-11])
  - report field `tool_version` ([REQ-49])
- [REQ-132-02] If available, build metadata MUST include a `git_sha` string; if unavailable in archive builds, it MUST be empty but present in schema.

## [VER-03] Report schema versioning
### [REQ-133] Schema version contract (binding)
- [REQ-133-01] JSON MUST include integer `schema_version` ([REQ-40], [REQ-49]).
- [REQ-133-02] CSV MUST include schema version in a header comment (or dedicated column/header field) ([REQ-41]).
- [REQ-133-03] `VGCPU_REPORT_SCHEMA_VERSION` MUST be bumped when:
  - any required field is added/removed/renamed,
  - any field meaning changes,
  - column ordering changes in CSV in a way that breaks parsers.
- [REQ-133-04] Schema changes MUST reference a CR id and include validators/tests ([TEST-24], [TEST-42], [TEST-43]).

- [DEC-VER-02] Schema versioning is **independent** of product SemVer:
  - schema version is monotonically increasing integer;
  - product SemVer major bumps may include multiple schema bumps.
  - Rationale: simplifies machine parsing and avoids “schema is 0.x too”.
  - Consequences: release notes must include a “schema changes” section.

## [VER-04] Deprecation policy
### [REQ-134] CLI deprecation (binding)
- [REQ-134-01] Deprecated flags MUST remain functional for at least **one MINOR** release cycle before removal.
- [REQ-134-02] `--help` MUST annotate deprecated flags.
- [REQ-134-03] A structured warning log event MUST be emitted when deprecated flags are used (outside measured loops).

### [REQ-135] Backend name deprecation (binding)
- [REQ-135-01] Backend stable names are part of CLI contract ([REQ-33]); renaming requires:
  - alias support for at least one MINOR version,
  - report both “requested name” and “resolved canonical name” in logs.

### [REQ-136] Schema deprecation (binding)
- [REQ-136-01] Removing a report field requires a schema_version bump and documentation in `/docs/schema_changes.md`.
- [REQ-136-02] Additive JSON fields should NOT bump schema_version unless required by downstream tools.

## [VER-05] Change governance (CR + Decision Log)
### [REQ-137] CR required (binding)
Any change affecting:
- compatibility surfaces (CLI/backends/schema),
- architecture boundaries (Ch2/Ch3 rules),
- error codes and their meaning,
- dependency versions (pins),
- CI gates / perf thresholds,
MUST be proposed and approved via a CR file:
- `/cr/CR-XXXX.md` (template: `cr_template.md`)

### [REQ-138] Decision log append-only (binding)
- [REQ-138-01] Architecture/major decisions MUST be captured in `/blueprint/decision_log.md` using `decision_log_template.md`.
- [REQ-138-02] Decisions are append-only; never rewrite historical decisions.
- [REQ-138-03] Each decision must cite:
  - motivation,
  - alternatives considered,
  - consequences,
  - affected [REQ]/[TEST] IDs.

### [DEC-VER-03] Governance threshold
- Small refactors that do not change compatibility surfaces and do not modify any [REQ]/[TEST] contracts can skip CR, but MUST still:
  - pass CI gates,
  - preserve ID references in code comments where applicable.
- Consequences: reviewers must enforce “CR required” consistently.

## [VER-06] Release process & artifacts
### [REQ-139] Release cadence & tagging
- [REQ-139-01] Releases are cut from `main` and tagged `vMAJOR.MINOR.PATCH`.
- [REQ-139-02] Release notes MUST include:
  - changes summary,
  - backends enabled and tiering,
  - schema changes (if any),
  - perf/regression notes (see [VER-08]).

### [REQ-140] Artifact contract
- [REQ-140-01] Release archives MUST contain:
  - `bin/vgcpu-benchmark[.exe]`
  - `assets/**` (manifest + `.irbin`) ([REQ-17])
  - `THIRD_PARTY_NOTICES.md` (if redistributing prebuilts) ([REQ-101])
  - `README.md` or `docs/` pointer
  - checksums file
- [REQ-140-02] Tier-1 releases MUST include Tier-1 backends only unless a separate “full” artifact is explicitly labeled.

## [VER-07] Dependency updates & security policy
### [REQ-141] Dependency cadence
- [REQ-141-01] Dependency pins must be reviewed at least **quarterly**.
- [REQ-141-02] Any security fix update (CVE or critical bug) can trigger an out-of-band patch release.
- [REQ-141-03] No floating branches in shipped presets (restates [REQ-99]).

### [REQ-142] Supply-chain hygiene
- [REQ-142-01] `cmake/vgcpu_deps.cmake` MUST be the single source-of-truth for dependency versions/SHAs.
- [REQ-142-02] For Rust optional backends:
  - `Cargo.lock` committed and `--locked` builds ([REQ-102-01], [REQ-102-02]).
- [REQ-142-03] CI MUST include a job that verifies “no floating deps” ([TEST-37]).

## [VER-08] Performance regression policy (benchmark harness)
This tool is about performance; we still need a practical, non-flaky regression policy.

### [REQ-143] Perf harness definition (binding)
- [REQ-143-01] Provide a benchmark preset or command mode intended for CI:
  - small scene subset,
  - fixed warmup/reps,
  - Tier-1 backends only,
  - deterministic ordering and seeded behavior ([REQ-76], [REQ-85]).
- [REQ-143-02] Perf runs MUST produce machine-readable artifacts (JSON + CSV) and store them as CI artifacts.
- [REQ-143-03] Perf comparisons MUST be performed on the same OS + runner class (never compare cross-OS).

### [DEC-VER-04] Regression thresholds (coarse gates)
- Thresholds for CI gating use conservative triggers:
  - fail if p50 OR p90 regress by **≥ 15%** on the CI suite, confirmed by a rerun,
  - warn (non-failing) for 10–15% regressions.
- Rationale: shared CI runners are noisy (aligns with [DEC-SCOPE-04]).
- Alternatives: no perf gating; strict 3–5%.
- Consequences: the harness must support rerun confirmation and stable baselines.

### [REQ-144] Baseline storage and comparison
- [REQ-144-01] Baselines are stored as versioned JSON artifacts in-repo under:
  - `/perf/baselines/<os>/<backend>/<scene_set>.json`
  OR stored in CI artifact history with a deterministic reference (choose one via CR).
- [DEC-VER-05] Store baselines **in-repo** for v0.2.0 for transparency and determinism.
  - Alternatives: external storage (GitHub Releases/Artifacts).
  - Consequences: baseline updates require CR + reviewer approval (to avoid masking regressions).

### [REQ-145] Measurement protocol (CI and local)
- [REQ-145-01] CI perf runs MUST:
  - pin threads to 1 (default),
  - disable optional backends,
  - use fixed scene set and fixed seed.
- [REQ-145-02] Local perf runs MAY use:
  - `--pin` best-effort,
  - `--threads N` for scaling benchmarks (separate dimension; not compared to single-thread baselines).

### [REQ-146] Perf regression reporting
- [REQ-146-01] On regression failure, CI must upload:
  - baseline report,
  - current report,
  - a diff summary showing per-scene deltas (p50/p90/p99).
- [REQ-146-02] The diff tool MUST ignore non-deterministic metadata fields (timestamps, run_id).

## [VER-09] Migration notes (v0.1.0 → v0.2.0)
### [REQ-147] Migration must be documented
- [REQ-147-01] Create `/docs/migration_v0.1_to_v0.2.md` including:
  - new preset commands (CMakePresets),
  - Tier-1 backend definition,
  - schema_version introduction and any report format adjustments,
  - new CI gates (format/lint/temp-dbg/no-floating-deps).

### Expected breaking changes (if any)
- [DEC-VER-06] Treat these as **non-breaking** in v0.2.0 unless they actually alter behavior:
  - moving legacy `/blueprint` to `/docs/legacy_blueprint` (internal docs path change)
  - adding schema_version fields (additive)
- Potential breaking changes (require careful review / CR if implemented):
  - renaming any CLI flags,
  - changing CSV column order.

## [VER-10] Versioning & lifecycle tests
- [TEST-45] `version_macros_match_cmake`:
  - verifies `VGCPU_VERSION_*` macros and `tool_version` match `project(VERSION)`.
- [TEST-46] `schema_version_present_and_integer`:
  - JSON output includes integer `schema_version`; CSV includes schema header.
- [TEST-47] `no_floating_deps_gate`:
  - configure fails if any dependency uses floating branch (supports [REQ-99], [TEST-37]).
- [TEST-48] `perf_diff_ignores_nondeterminism`:
  - diff tool ignores run_id/timestamps and flags only statistical changes.

## [VER-11] Traceability notes
- [VER-11-01] This chapter defines [REQ-131..147] and [TEST-45..48].
- [VER-11-02] Next file output is `/blueprint/decision_log.md` (append-only) which will record the initial decisions [DEC-*] introduced so far.
- [VER-11-03] `/blueprint/walkthrough.md` and `/blueprint/implementation_checklist.yaml` will provide the executable plan to implement and verify all [REQ-*] and [TEST-*].

