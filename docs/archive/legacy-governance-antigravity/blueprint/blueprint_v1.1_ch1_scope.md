# Chapter 1 — Scope, Requirements, Budgets (VGCPU-Benchmark)

## [SCOPE-01] Product definition
- [SCOPE-01-01] **Product**: `vgcpu-benchmark` — a cross-platform CLI to benchmark CPU vector-graphics rendering backends by replaying canonical IR scenes through per-backend adapters and emitting comparable reports.
- [SCOPE-01-02] Primary users:
  - [SCOPE-01-02a] Library/backend developers validating performance regressions.
  - [SCOPE-01-02b] CI maintainers running quick smoke/perf sanity.
  - [SCOPE-01-02c] Power users doing controlled local benchmarking.

## [SCOPE-02] Supported platforms & toolchains
- [REQ-01] The product MUST build and run on:
  - [REQ-01-01] Windows 10/11 (x86_64)
  - [REQ-01-02] macOS 13+ (x86_64 and arm64)
  - [REQ-01-03] Linux (glibc-based distros; x86_64)
- [REQ-01-04] Toolchains:
  - [REQ-01-04a] MSVC (Visual Studio 2022) on Windows
  - [REQ-01-04b] AppleClang (Xcode toolchain) on macOS
  - [REQ-01-04c] Clang and/or GCC on Linux
- [DEC-SCOPE-01] Language standard baseline is **C++20** (observed), with a future option to move to C++23 behind an explicit CR.
  - Rationale: matches current repo; avoids broad compiler matrix churn.
  - Alternatives: C++17; C++23 immediately.
  - Consequences: API designs should avoid C++23-only features; keep portability guidelines in Ch7.

## [SCOPE-03] In-scope features
### [REQ-02] Observability (required)
- [REQ-02-01] The CLI MUST provide structured logs with consistent event names and fields for:
  - [REQ-02-01a] run configuration (backend, scenes, warmup/reps, threads)
  - [REQ-02-01b] environment metadata (OS, CPU, build type, commit/build-id if available)
  - [REQ-02-01c] adapter/backend failures with actionable error codes
- [REQ-02-02] Logging MUST support:
  - [REQ-02-02a] human-readable console mode
  - [REQ-02-02b] machine-readable mode (JSON Lines) suitable for CI artifacts
- [REQ-02-03] Logging MUST NOT materially distort benchmark timings (policy: no per-iteration logging on hot path).

### Benchmarking & workload
- [REQ-03] The system MUST execute canonical IR scenes (from the repo’s scene assets) through a selected backend adapter and capture timings per scene.
- [REQ-04] The harness MUST support:
  - [REQ-04-01] configurable warmup iterations
  - [REQ-04-02] configurable measured repetitions
  - [REQ-04-03] a fixed measurement clock abstraction (monotonic) across platforms
  - [REQ-04-04] aggregation into summary statistics (min/mean/median/p50/p90/p99; stddev optional)
- [REQ-05] The system MUST output reports to:
  - [REQ-05-01] CSV
  - [REQ-05-02] JSON
  - [REQ-05-03] a human-readable summary (stdout)
- [REQ-06] The CLI MUST accept:
  - [REQ-06-01] backend selection (one or more)
  - [REQ-06-02] scene selection (one or more)
  - [REQ-06-03] output directory selection
  - [REQ-06-04] warmup/repetition controls

### Correctness artifacts & regression (v1.1)
- [REQ-148] When enabled, the tool MUST produce a PNG artifact per (scene, backend) after benchmarking that (scene, backend).
- [REQ-149] The PNG artifact MUST be generated from the backend’s 32-bit RGBA surface raw buffer using `stb_image_write` (PNG encoding).
- [REQ-150] The CLI MUST expose a parameter to enable/disable PNG production (default: disabled).
- [REQ-151] When enabled, the tool MUST compute SSIM (structural similarity index) comparing each backend’s image against a “ground truth” backend image.
- [REQ-152] SSIM MUST use a lightweight, open-source library suitable for vendoring/pinning; its license MUST be recorded in `THIRD_PARTY_NOTICES.md`.
- [REQ-153] The CLI MUST expose a parameter to select which backend is the ground truth (e.g., `skia`), and the run MUST fail if that backend is not available/enabled.
- [REQ-154] The CLI MUST expose a parameter to set the SSIM failure threshold; when SSIM comparison is enabled, the run MUST fail if any comparison is below the threshold.

- [DEC-SCOPE-05] PNG output naming and location is fixed as:
  - Directory: `<output_dir>/png/`
  - Filename: `<scene>_<backend>.png` (example: `/results/png/test_rect_skia.png`)
  - Rationale: avoids collisions while keeping a flat folder; matches user request.
  - Alternatives: per-backend subfolders; `<backend>__<scene>.png`.
  - Consequences: tools can glob `png/*.png`; uniqueness relies on backend id stability (formalized in Ch4).

## [SCOPE-04] Out of scope
- [REQ-07] GPU rendering backends and GPU benchmarking are out of scope.
- [REQ-07-01] No Vulkan/Metal/DX12 backends are required by this project (CPU-only focus).
- [REQ-07-02] Hardware counters/profilers integration (VTune/Perfetto) is best-effort and not required for baseline.

## [SCOPE-05] Backend tiering policy (scope decision)
- [DEC-SCOPE-02] Define Tier-1 backends as:
  - [DEC-SCOPE-02a] `null` (always available; correctness/control baseline)
  - [DEC-SCOPE-02b] `plutovg` (small, C-only, easiest cross-platform dependency)
  - [DEC-SCOPE-02c] `blend2d` (high value, common CPU renderer)
  - Others are **Optional** by default (Skia/Qt/ThorVG/AGG/Cairo/AmanithVG/Raqote/Vello).
  - Rationale: keep CI reliable while still covering a “real” CPU backend variety.
  - Alternatives: Tier-1 only `null+plutovg`; or include Skia/Cairo as Tier-1.
  - Consequences: CI must at minimum build+run Tier-1; optional backends build-only or best-effort, controlled in Ch7.

## [SCOPE-06] Compatibility surfaces (scope decision)
- [DEC-SCOPE-03] Treat the CLI flags + JSON/CSV output schemas as **versioned compatibility surfaces**.
  - [DEC-SCOPE-03a] Introduce `schema_version` in JSON and a `# schema_version=` header in CSV where feasible.
  - Rationale: downstream tooling commonly depends on report structure.
  - Alternatives: best-effort schema changes without versioning.
  - Consequences: changes to schemas require SemVer rules + migration notes in Ch9.

## [SCOPE-07] Performance and resource budgets
### Budgets table
| Budget | Target | Applies to | Notes |
|---|---:|---|---|
| [REQ-08] CLI cold start | ≤ 1.0s | local | excludes first-time dependency fetch/build |
| [REQ-09] CI smoke run duration | ≤ 60s | CI | Tier-1, small scene subset, limited reps |
| [REQ-10] Full suite run duration | configurable | local | can be minutes; user-controlled |
| [REQ-11] Peak RSS | ≤ 1.0 GiB | all | optional backends may exceed; must be documented |
| [REQ-12] Benchmark overhead | ≤ 2% | local | harness overhead vs “null backend” baseline for measured loop |

### Determinism / measurement controls
- [REQ-13] The harness MUST provide explicit controls to reduce noise:
  - [REQ-13-01] warmup control (see [REQ-04-01])
  - [REQ-13-02] fixed scene ordering and explicit random seed if randomness exists
  - [REQ-13-03] optional CPU affinity / thread pinning best-effort (platform-dependent)
- [DEC-SCOPE-04] Determinism target for CI gates is “coarse”: prioritize catching **large regressions** (e.g., ≥10–15%) rather than small deltas.
  - Rationale: shared CI runners are noisy; small deltas cause flakiness.
  - Alternatives: no perf gating; strict 2–3% gates.
  - Consequences: perf regression harness in Ch9/Ch7 uses conservative thresholds and requires rerun confirmation.

## [SCOPE-08] Security, privacy, and safety
- [REQ-14] The tool MUST avoid collecting or uploading personal data.
- [REQ-15] Reports/logs MUST not include:
  - [REQ-15-01] usernames/home paths by default (redact or normalize)
  - [REQ-15-02] environment variables unless explicitly requested

## [SCOPE-09] Requirements → tests mapping (forward references)
This chapter defines [REQ-*] only. Concrete [TEST-*] IDs and the test implementations are specified in Ch8/Ch9, and each [REQ-*] will map to:
- ≥1 [TEST-*] entry, and
- ≥1 task in `/blueprint/implementation_checklist.yaml`.

(Enforcement rule: CI fails if a [REQ-*] lacks test coverage mapping.)
