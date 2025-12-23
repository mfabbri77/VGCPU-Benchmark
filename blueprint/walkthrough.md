# Walkthrough — VGCPU-Benchmark Modernization (to product v0.2.0)

This walkthrough is the **operational plan** for implementing blueprint v1.0.
Each step references stable IDs ([REQ-*], [TEST-*], [DEC-*]) and ends with a DoD (definition of done) and runnable commands.

> Conventions:
> - All commands use presets (Ch7 [REQ-106..109]).
> - “Tier-1” = `null`, `plutovg`, `blend2d` (Ch1 [DEC-SCOPE-02]).
> - Any compatibility-surface change requires a CR (Ch9 [REQ-137]).

---

## Phase 0 — Repo normalization & blueprint adoption

### Step 0.1 — Create canonical blueprint structure
- Action:
  - Create `/blueprint/` with the files emitted by blueprint v1.0 ([META-05-01..14]).
  - Move existing repo `/blueprint/` to `/docs/legacy_blueprint/` ([DEC-PRE-INTAKE-01], [DEC-TOOL-01], [REQ-115]).
- DoD:
  - Canonical `/blueprint/blueprint_v1.0_ch*.md` exist.
  - `/docs/legacy_blueprint/` contains previous docs.
- Commands:
  - N/A (file moves + commit).

### Step 0.2 — Create governance scaffolding
- Action:
  - Add `/cr/` directory.
  - Add CR template `cr_template.md` as `/cr/CR-0000_TEMPLATE.md`.
  - Ensure `/blueprint/decision_log.md` is present and append-only ([REQ-138]).
- DoD:
  - A sample CR exists for “Adopt blueprint v1.0” referencing key decisions ([DEC-*]).
- Commands:
  - N/A.

---

## Phase 1 — Build system modernization (presets + gates)

### Step 1.1 — Add CMake presets
- Implements: [REQ-92..94], [DEC-BUILD-02], [REQ-106..109]
- Action:
  - Add `CMakePresets.json` with presets: `dev`, `release`, `ci`, `asan`, `ubsan`, `tsan`.
  - Introduce cache variables: `VGCPU_WERROR`, `VGCPU_TIER1_ONLY`, `VGCPU_ENABLE_*SAN`.
- DoD:
  - `cmake --preset dev` configures on at least one platform locally.
- Commands:
  - `cmake --preset dev`
  - `cmake --build --preset dev`

### Step 1.2 — Add tier-1-only build mode
- Implements: [REQ-95-02], [DEC-BUILD-03], [DEC-SCOPE-02]
- Action:
  - Implement `VGCPU_TIER1_ONLY` that forces optional `ENABLE_*` options OFF.
  - Ensure Tier-1 defaults are ON when not in tier1-only mode ([DEC-BUILD-03]).
- DoD:
  - `cmake --preset ci` results in only Tier-1 backends being compiled/registered.
- Commands:
  - `cmake --preset ci`
  - `cmake --build --preset ci`

### Step 1.3 — Add dependency pinning enforcement
- Implements: [REQ-99..101], [REQ-100], [TEST-37], [DEC-BUILD-05]
- Action:
  - Centralize all FetchContent versions/SHAs in `cmake/vgcpu_deps.cmake`.
  - Replace any `master/main` refs with immutable tags/SHAs.
  - Add configure-time linter: fail if any `GIT_TAG` is `master/main`.
- DoD:
  - Configure fails if an engineer reintroduces `master/main`.
- Commands:
  - `cmake --preset ci` (should succeed)
  - (negative test) set a dep to `master` and verify failure.

### Step 1.4 — Add gate targets: format/lint/temp-dbg/include checks/tests
- Implements: [REQ-103..105], [REQ-116..122], [REQ-120], [DEC-TOOL-04]
- Action:
  - Add `.clang-format`, `.clang-tidy`, `.editorconfig`.
  - Add tools scripts:
    - `tools/check_no_temp_dbg.py` ([REQ-122])
    - `tools/check_includes.py` ([DEC-TOOL-04])
  - Wire CMake targets:
    - `format`, `format_check`, `lint`, `check_no_temp_dbg`, `check_includes`, `tests`.
- DoD:
  - All targets exist and fail correctly on violations.
- Commands:
  - `cmake --preset ci`
  - `cmake --build --preset ci --target format_check`
  - `cmake --build --preset ci --target check_no_temp_dbg`
  - `cmake --build --preset ci --target check_includes`
  - `cmake --build --preset ci --target lint` (where enabled)

---

## Phase 2 — Establish a real test suite (CTest + unit/contract tests)

### Step 2.1 — Add /tests and CTest integration
- Implements: [TEST-01..20], [TEST-25..48], [TEST-INTAKE-02], [TEST-DELTA-01]
- Action:
  - Create `/tests` with a unit test runner (Catch2/Doctest/GoogleTest—pick one via CR if needed).
  - Add `enable_testing()` and `add_test()` registrations.
  - Ensure CI’s `ctest` actually runs >0 tests.
- DoD:
  - `ctest --preset ci` runs and reports tests executed.
- Commands:
  - `cmake --preset ci`
  - `cmake --build --preset ci`
  - `ctest --preset ci --output-on-failure`

### Step 2.2 — Implement correctness tests for PAL/assets/IR/stats/reporting
- Implements: Ch3/Ch5/Ch6/Ch9 tests:
  - PAL: [TEST-04], [TEST-05]
  - Manifest: [TEST-06], [TEST-07]
  - IR: [TEST-08], [TEST-09]
  - Stats/harness: [TEST-15..17], [TEST-31..33]
  - Reporting: [TEST-18..20], [TEST-22..24], [TEST-46]
- Action:
  - Add fixtures for manifest and minimal IR scenes (or use existing assets with a controlled subset).
  - Add golden tests for CSV header order and required JSON keys.
- DoD:
  - Tier-1 tests pass on all three OSes in CI.
- Commands:
  - `ctest --preset ci --output-on-failure`

### Step 2.3 — Hot-path and allocation tests (debug instrumentation)
- Implements: [REQ-71-03], [DEC-MEM-08], [TEST-27], [TEST-28], [TEST-30]
- Action:
  - Add optional allocation instrumentation build flag `VGCPU_ENABLE_ALLOC_INSTRUMENTATION`.
  - Implement `no_alloc_in_measured_loop_null` test for null backend.
- DoD:
  - In a debug/test preset, the no-allocation test passes.
- Commands:
  - `cmake --preset dev -DVGCPU_ENABLE_ALLOC_INSTRUMENTATION=ON` (if supported by preset)
  - `cmake --build --preset dev`
  - `ctest --preset dev --output-on-failure`

### Step 2.4 — Concurrency + TSan
- Implements: [REQ-87], [REQ-110-06], [TEST-35]
- Action:
  - Ensure multi-thread harness partitioning tests exist and pass.
  - Add TSan CI job on Linux/Clang if toolchain supports it.
- DoD:
  - TSan job runs and is green, or an approved fallback stress job exists.
- Commands:
  - `cmake --preset tsan`
  - `cmake --build --preset tsan`
  - `ctest --preset tsan --output-on-failure`

---

## Phase 3 — Internal interfaces + layering cleanup

### Step 3.1 — Introduce internal header tree and export/version macros
- Implements: [DEC-API-05], [REQ-52], [API-02-10..11], [REQ-132]
- Action:
  - Add `include/vgcpu/internal/*.h` per Ch4 interface sketches.
  - Generate `include/vgcpu/version.h` from CMake configure step.
  - Add `-fvisibility=hidden` where applicable ([REQ-52-01]).
- DoD:
  - Build succeeds; internal headers are used by targets; no “public” installs.
- Commands:
  - `cmake --preset ci`
  - `cmake --build --preset ci`
  - `ctest --preset ci --output-on-failure`

### Step 3.2 — Adapter registry contracts + capability flags
- Implements: [REQ-32..36], [DEC-CONC-05], [TEST-10..14], [TEST-34]
- Action:
  - Ensure registry enumerates only compiled backends.
  - Add `supports_parallel_render` capability and enforce harness refusal.
- DoD:
  - `registry_contains_tier1` passes on all OSes.
- Commands:
  - `ctest --preset ci --output-on-failure`

### Step 3.3 — Logging and schema-versioned reporting
- Implements: [REQ-02], [REQ-40..44], [REQ-49], [REQ-133], [TEST-18..19], [TEST-46]
- Action:
  - Implement structured logging with console + JSONL sinks (outside measured loops).
  - Add JSON/CSV schema version fields and validators.
- DoD:
  - Smoke run produces valid JSON/CSV; validators pass in CI.
- Commands:
  - `cmake --preset ci`
  - `cmake --build --preset ci`
  - `./build/ci/vgcpu-benchmark --help`
  - `./build/ci/vgcpu-benchmark list`
  - `./build/ci/vgcpu-benchmark run --backend null --reps 10 --warmup 1 --out build/ci/out`
  - `python tools/validate_report_json.py build/ci/out/report.json`
  - `python tools/validate_report_csv.py build/ci/out/report.csv`

---

## Phase 4 — CI workflow updates & release packaging

### Step 4.1 — CI uses presets only + add gate jobs
- Implements: [REQ-106..109], [REQ-110], [REQ-127], [TEST-39]
- Action:
  - Update `.github/workflows/ci.yml` to use:
    - `cmake --preset ci`
    - `cmake --build --preset ci`
    - `ctest --preset ci`
  - Add jobs for `asan`, `ubsan`, and `tsan` (or fallback stress) on Linux.
  - Add fast lint job: `lint_workflows_presets.py`.
- DoD:
  - CI green with new gates; no raw configure flags in workflows.
- Commands (locally emulate):
  - `python tools/lint_workflows_presets.py`

### Step 4.2 — Packaging target and artifact validation
- Implements: [REQ-111], [REQ-140], [REQ-101]
- Action:
  - Expose packaging script via `package` target.
  - Ensure assets are included and Tier-1 release runs out-of-the-box.
  - Add `THIRD_PARTY_NOTICES.md` and license collection.
- DoD:
  - `cmake --build --preset release --target package` produces an archive matching contract.
- Commands:
  - `cmake --preset release`
  - `cmake --build --preset release --target package`

---

## Phase 5 — Performance regression harness (coarse, non-flaky)

### Step 5.1 — Define CI perf suite and baseline format
- Implements: [REQ-143..146], [DEC-VER-04], [DEC-VER-05]
- Action:
  - Add `perf/`:
    - `perf/scenes_ci.txt` (small scene ids list)
    - `perf/baselines/<os>/<backend>/ci_suite.json` (committed baselines)
  - Implement `tools/perf_diff.py`:
    - compares p50/p90 per scene,
    - ignores nondeterministic metadata,
    - emits a summary diff.
- DoD:
  - Diff tool works on two sample reports and produces stable output.
- Commands:
  - `python tools/perf_diff.py --baseline perf/baselines/linux/null/ci_suite.json --current build/ci/out/report.json`

### Step 5.2 — Add CI perf job (non-blocking initially)
- Implements: [REQ-143], [REQ-146]
- Action:
  - Add a CI job that runs the CI perf suite and uploads artifacts.
  - Start as “warn-only”; switch to gating after baselines settle.
- DoD:
  - Artifacts are uploaded and diff summary is visible.
- Commands:
  - N/A (CI).

---

## Phase 6 — Documentation & migration notes

### Step 6.1 — Developer quickstart and migration doc
- Implements: [REQ-130], [REQ-147]
- Action:
  - Add `docs/developer_quickstart.md` and `docs/migration_v0.1_to_v0.2.md`.
  - Document Tier-1 backends, presets, schema_version, and new gates.
- DoD:
  - A new contributor can build and run Tier-1 with only the quickstart.
- Commands:
  - Follow quickstart commands verbatim and verify.

---

## Global Definition of Done (for v0.2.0 milestone)
All of the following must be true:

- [DoD-01] Tier-1 builds and runs on Windows/macOS/Linux ([REQ-01], [REQ-110]).
- [DoD-02] CI uses presets only and includes sanitizer jobs ([REQ-106..110]).
- [DoD-03] Tests exist and run via CTest; CI `ctest` runs >0 tests ([TEST-01..48]).
- [DoD-04] No floating dependencies in shipped presets ([REQ-99], [TEST-37]).
- [DoD-05] TEMP-DBG markers are forbidden and gated ([REQ-121..122]).
- [DoD-06] Reports are schema-versioned and validators pass ([REQ-48..50], [REQ-133], [REQ-124..125]).
- [DoD-07] Release package is self-contained for Tier-1 ([REQ-17], [REQ-140]).
- [DoD-08] All [REQ-*] are mapped to ≥1 [TEST-*] and ≥1 checklist task (validated by implementation_checklist.yaml generation rules).

