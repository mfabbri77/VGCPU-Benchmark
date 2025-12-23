# Chapter 9 — Versioning, Lifecycle, Governance, Regression Policy (v1.1)

## [DEC-VER-00] Inheritance baseline
- [DEC-VER-00] This chapter **inherits** all v1.0 policies and requirements in `/blueprint/blueprint_v1.0_ch9_versioning_lifecycle.md` **unchanged**, except for the explicit v1.1 deltas recorded below.
  - Rationale: v1.1 adds PNG artifacts + SSIM comparison, but does not change the project’s fundamental governance model.
  - Consequences: all existing [VER-01..VER-11] rules remain in force; v1.1 only appends new constraints and gates.

---

## [VER-12] Release versioning for this change set (product vs blueprint)
- [VER-12-01] **Blueprint** version is `v1.1` (this document set).
- [VER-12-02] **Product** (CLI benchmark) version in `CMakeLists.txt` is currently `0.1.0` (observed).
- [DEC-VER-01] The product release containing CR-0002 is **0.2.0** (bump MINOR under 0.x) rather than “v1.1.0”.
  - Rationale: adds new user-facing features (new CLI flags + new report fields) while remaining non-breaking for existing flags; repo currently uses `0.1.0`.
  - Alternatives: 0.1.1 (too small for feature add); 1.0.0 (too strong a stability promise).
  - Consequences: update `project(... VERSION 0.2.0 ...)` in CMake and add changelog entry; docs must clarify blueprint vs product version strings.

## [VER-13] Correctness regression lifecycle (PNG + SSIM)
- [VER-13-01] PNG artifacts and SSIM comparisons are **opt-in** features and default to OFF ([REQ-150]).
- [VER-13-02] If SSIM comparison is enabled:
  - [VER-13-02a] the run outcome is PASS only if all SSIM scores are ≥ threshold ([REQ-154]),
  - [VER-13-02b] SSIM failure MUST yield a non-zero exit code dedicated to regression (see [API-02-04]).
- [VER-13-03] Ground truth behavior:
  - [VER-13-03a] ground truth images MUST be regenerated each run, per scene, before comparisons ([API-03-05c]),
  - [VER-13-03b] missing/unavailable ground truth backend MUST fail the run ([REQ-153], [DEC-ARCH-12]).

## [VER-14] SSIM gating policy and CI usage
- [VER-14-01] CI MAY enable SSIM gating only on a **curated subset** of scenes/backends to avoid flakiness.
- [DEC-VER-02] Default SSIM threshold is **0.99** but MUST be user-configurable via CLI ([REQ-154]).
  - Rationale: “meaningful default” that is strict but tunable.
  - Alternatives: 0.995 (stricter); 0.95 (looser).
  - Consequences: CI jobs can set stricter/looser thresholds per platform; failures must retain artifacts for debugging.
- [VER-14-02] When SSIM gating is enabled in CI, CI MUST retain:
  - [VER-14-02a] the JSON/CSV reports, and
  - [VER-14-02b] the `output_dir/png/*.png` artifacts.
- [DEC-VER-03] SSIM regressions are classified as **correctness regressions** and are blocking for releases when enabled in release-candidate pipelines.
  - Rationale: correctness must not silently drift across backends.
  - Alternatives: advisory-only SSIM reporting.
  - Consequences: release checklist must include at least one SSIM-enabled run on Tier-1 backends.

## [VER-15] Report schema evolution (v1.1 fields)
- [VER-15-01] Adding new report fields (`png_path`, `ssim_vs_ground_truth`, `ground_truth_backend`, `ssim_threshold`) is a **schema extension** and must follow [DEC-SCOPE-03] schema versioning.
- [VER-15-02] Schema changes MUST include:
  - [VER-15-02a] `schema_version` bump for JSON and CSV,
  - [VER-15-02b] changelog entry,
  - [VER-15-02c] migration notes if downstream tooling needs updates.

## [VER-16] Dependency governance for vendored single-file libs (new)
- [VER-16-01] Vendored dependencies (`stb_image_write`, SSIM) MUST be pinned by recorded SHA-256 and verified in CI ([REQ-155], [TEST-70]).
- [VER-16-02] Updating a vendored dependency requires:
  - [VER-16-02a] a CR describing the bump,
  - [VER-16-02b] updated `THIRD_PARTY_NOTICES.md`,
  - [VER-16-02c] updated recorded SHA-256 values,
  - [VER-16-02d] rerun of SSIM-enabled smoke tests to ensure no behavioral change in outputs (unless intended).

## [VER-17] Migration & deprecation policy (CLI flags)
- [VER-17-01] CLI flags are a stable surface:
  - adding new flags is allowed in MINOR releases,
  - renaming/removing flags requires a deprecation period and a MAJOR bump (or explicit “breaking under 0.x” note in release notes).
- [VER-17-02] Deprecation process:
  - [VER-17-02a] mark deprecated flags in `--help`,
  - [VER-17-02b] keep accepting them for at least one MINOR release,
  - [VER-17-02c] emit a structured warning event when used.

## [VER-18] Performance regression interaction with artifacts
- [VER-18-01] Artifact generation and SSIM computation MUST remain outside timed loops and must not affect performance regression baselines ([DEC-ARCH-11]).
- [VER-18-02] CI perf regression gates MUST run with PNG/SSIM disabled unless the gate is explicitly validating correctness artifacts (to keep perf data comparable).

## [VER-19] CR governance for v1.1
- [VER-19-01] CR-0002 is the governing change request for v1.1 correctness artifacts.
- [VER-19-02] Any follow-on changes that:
  - modify image buffer contracts,
  - change naming/layout of artifacts,
  - change SSIM algorithm or aggregation,
  MUST be introduced via a new CR and new [DEC-*] entries in `decision_log.md`.

## [TEST-72] Versioning/lifecycle tests (v1.1)
- [TEST-72-01] Report schema version presence test: JSON and CSV include schema markers when new fields appear.
- [TEST-72-02] Dependency integrity test: configure/build fails if vendored hashes mismatch when `VGCPU_VERIFY_THIRD_PARTY_HASHES=ON` ([TEST-70]).
- [TEST-72-03] SSIM regression exit code test: SSIM below threshold yields exit code `4` and still writes reports and PNG artifacts.

(Implementation steps and CI wiring are detailed in `walkthrough.md` and `implementation_checklist.yaml`.)
