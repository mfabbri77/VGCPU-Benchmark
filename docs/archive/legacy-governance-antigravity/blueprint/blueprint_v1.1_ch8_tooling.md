# Chapter 8 — Tooling (Dev UX, Quality Gates, Utilities)

## [DEC-TOOL-01] Tooling baseline (inherit v1.0)
- [DEC-TOOL-01] v1.1 inherits the v1.0 tooling stack and quality gates (formatting/lint/sanitizers/tests) unchanged, except where explicitly extended below.
  - Rationale: v1.1 feature work is localized (artifacts + SSIM) and should not disrupt established workflows.
  - Alternatives: rework tooling (new linters, new build system).
  - Consequences: contributors follow existing preset-based commands; new tooling hooks added only where necessary.

## [TOOL-01] Formatting & style
- [TOOL-01-01] Formatting and style rules are governed by `code_style_and_tooling.md`.
- [TOOL-01-02] CI MUST enforce formatting for modified files (existing mechanism from v1.0).
- [TOOL-01-03] New v1.1 code under `src/artifacts/` and `include/vgcpu/internal/artifacts/` MUST be format-clean and warning-clean under the same rules.

## [TOOL-02] Static analysis (lint)
- [TOOL-02-01] Existing clang-tidy / compiler warning gates remain in effect (v1.0).
- [TOOL-02-02] Third-party headers under `third_party/` are treated as `SYSTEM` includes ([DEC-BUILD-22]); lint should not target vendored files.
- [TOOL-02-03] New artifact code MUST be free of:
  - implicit narrowing conversions in image dimension math
  - unchecked overflow in `width*height*4` computations
  - unchecked filesystem errors

## [TOOL-03] Sanitizers
- [TOOL-03-01] ASan/UBSan jobs (from v1.0) MUST execute at least one run path with:
  - [TOOL-03-01a] `--png` enabled (exercise PNG writing)
  - [TOOL-03-01b] `--compare-ssim --ground-truth-backend <tier1>` enabled (exercise SSIM code path)
- [TOOL-03-02] TSan expectations are specified in Ch6 ([CONC-08-01]); artifact/SSIM code must be race-free.

## [TOOL-04] Temporary debug policy
- [TOOL-04-01] TEMP debug code MUST follow `temp_dbg_policy.md`.
- [TOOL-04-02] CI MUST fail if any `[TEMP-DBG]` markers remain at merge time.

## [TOOL-05] New in v1.1 — third-party integrity verification
- [REQ-155] The build/CI MUST verify the integrity of vendored single-file dependencies (`stb_image_write`, SSIM) against recorded SHA-256 values.
- [TEST-70] The verification mechanism is configured via CMake option `VGCPU_VERIFY_THIRD_PARTY_HASHES=ON` (see Ch7).
- [TOOL-05-01] Developer workflow:
  - [TOOL-05-01a] local builds MAY disable verification for rapid iteration,
  - [TOOL-05-01b] CI MUST enable verification in at least one preset/job.

## [TOOL-06] Utilities for artifact inspection (optional, no new required tools)
- [DEC-TOOL-02] v1.1 does not introduce mandatory new inspection tooling; artifacts are standard PNG files and can be viewed with any OS viewer.
  - Rationale: avoid adding dependencies for viewing/diffing images.
  - Alternatives: ship an image diff tool; integrate a viewer.
  - Consequences: correctness triage relies on the PNG artifacts and SSIM metrics in reports.

## [TOOL-07] Developer commands (preset-based; additive)
- [TOOL-07-01] Build:
  - `cmake --preset <preset>`
  - `cmake --build --preset <preset>`
- [TOOL-07-02] Test:
  - `ctest --preset <preset> --output-on-failure`
- [TOOL-07-03] Example run (artifacts + SSIM):
  - `vgcpu-benchmark --out <dir> --scenes <list> --backends <list> --compare-ssim --ground-truth-backend skia --ssim-threshold 0.99`
  - Note: per [DEC-API-01], `--compare-ssim` implies PNG artifacts even if `--png` is not set.

## [TEST-71] Tooling tests (v1.1)
- [TEST-71-01] CI job enables `VGCPU_VERIFY_THIRD_PARTY_HASHES=ON` and fails on mismatch (maps to [TEST-70]).
- [TEST-71-02] Sanitizer job runs at least one SSIM-enabled invocation and reports no sanitizer findings (maps to [TOOL-03-01]).

## [DEC-TOOL-03] N/A boundaries
- [DEC-TOOL-03] No IDE-specific project files are required (VS solutions, Xcode projects) beyond what CMake generates.
  - Rationale: keep repo build-system single-source-of-truth.
  - Alternatives: commit IDE projects.
  - Consequences: contributors use CMake presets + IDE CMake integration.
