# Appendix B — Testing Strategy (unit/integration/system/acceptance; automation)

## Purpose

This appendix defines the testing strategy for the benchmark suite, covering unit, integration, system, and acceptance tests, along with CI automation. It focuses on ensuring deterministic behavior, correctness of IR/schema handling, adapter reliability, and reproducibility, while avoiding unstable performance assertions in automated environments.

## B.1 Testing Objectives

1. Tests **MUST** verify that IR assets are parsed and validated safely and deterministically.
2. Tests **MUST** verify that output schemas are produced correctly and remain backward-compatible within the defined versioning policy.
3. Tests **MUST** verify adapter conformance to the adapter contract, including CPU-only enforcement metadata.
4. Tests **SHOULD** verify that benchmark timing boundaries exclude I/O and parsing overhead (via diagnostics and structural tests).
5. Tests **MUST** be runnable across Linux, Windows, and macOS in CI with minimal environmental assumptions.

## B.2 Test Levels and Coverage

### B.2.1 Unit Tests

Unit tests **MUST** cover isolated modules with no external library dependencies where feasible.

**Required unit test suites**

1. **IR Reader/Validator**

   * Valid header parsing and rejection of invalid magic/version.
   * Section bounds checks (offset+length within file).
   * Command stream validation:

     * save/restore nesting correctness,
     * invalid opcode rejection,
     * index range checks for geometry/paint references.
   * Numeric validation:

     * negative widths rejected,
     * invalid dash patterns rejected,
     * gradient stop ordering enforced.
2. **Hashing**

   * SHA-256 computed over IR bytes matches expected values for fixtures.
3. **Manifest Loader**

   * Manifest schema validation (required fields present).
   * IR file existence checks.
   * SceneHash validation (manifest hash matches computed hash).
4. **Statistics Engine**

   * Correct computation of p50 and selected dispersion metric (p90 and/or MAD/IQR).
   * Deterministic results for fixed input samples.
5. **Output Schema Serialization**

   * Required fields always present.
   * Deterministic ordering (where applicable).
   * Schema version field correctness.

**Normative requirements**

* Unit tests **MUST** run without requiring any rendering backend libraries.

### B.2.2 Integration Tests

Integration tests validate module interactions, and may include backends when available.

**Required integration test suites**

1. **End-to-end IR → PreparedScene**

   * Load fixture IR → validate → prepare → expose expected counts (paths/paints/commands).
2. **CLI Parsing → ExecutionPlan**

   * Given representative CLI invocations, ensure deterministic resolved plan (backends/scenes/policies).
3. **Reporting Pipeline**

   * Given a synthetic RunResult, emit JSON and CSV; verify schema compliance and determinism.

**Optional integration tests**

* Adapter registry: ensure build toggles correctly reflect runtime “compiled_in” availability.

### B.2.3 Adapter Smoke Tests (Backend-Dependent)

Where a backend is available in CI for a given platform:

1. A smoke test **MUST**:

   * initialize the adapter,
   * create a CPU surface,
   * render a minimal PreparedScene,
   * report success and produce timing stats (even if only a few iterations).
2. Smoke tests **MUST** validate that:

   * adapter reports CPU-only enforcement metadata,
   * capability flags are non-empty and consistent with known backend expectations.

**Normative requirements**

* Smoke tests **MUST NOT** assert absolute performance thresholds.
* Smoke tests **MAY** assert that timing values are positive and non-zero and that execution completes within a generous timeout.

### B.2.4 System Tests

System tests run the compiled CLI as a black box.

**Required system test cases**

1. `validate` command:

   * exits successfully with correct manifest and assets.
   * fails deterministically on hash mismatch fixture (negative test).
2. `list` command:

   * outputs deterministic lists of scenes/backends.
3. `run` command (minimal preset):

   * runs a small subset (e.g., 1 backend × 1 scene),
   * produces outputs in the output directory,
   * outputs include run metadata and schema_version.

### B.2.5 Acceptance Tests (Release Readiness)

Acceptance criteria focus on reproducibility and correctness of outputs rather than speed.

1. Release candidate acceptance **MUST** verify:

   * build succeeds on all supported OS/arch targets (as feasible),
   * output artifacts include correct schema version and required metadata,
   * dependency pin record present and populated,
   * adapter inclusion report matches packaged adapters.
2. Acceptance tests **SHOULD** verify:

   * canonical “standard run” preset executes with at least one backend across platforms,
   * unsupported features produce SKIP outcomes with correct reason codes.

## B.3 Golden Rendering Outputs (Optional Diagnostic)

1. The project **MAY** include a diagnostic mode that writes rendered images to disk for inspection.
2. If image comparison is implemented:

   * it **MUST** be tolerance-based by default,
   * it **MUST NOT** be a strict pass/fail gate unless explicitly enabled (e.g., `--strict-images`).
3. Image diagnostics **MUST** be disabled by default to avoid contaminating benchmark runs.

## B.4 Fuzzing and Robustness Testing (Recommended)

1. The project **SHOULD** include fuzz testing of:

   * IR parser/validator,
   * manifest parser (if custom parsing is used).
2. Fuzzing **SHOULD** be run in dedicated CI jobs where sanitizers are available.

## B.5 CI Automation Requirements

### B.5.1 CI Matrix

1. CI **MUST** include jobs for:

   * Linux x86_64
   * Windows x86_64
   * macOS (x86_64 and/or ARM64 depending on runner availability)
2. Where possible, CI **SHOULD** include ARM64 coverage.

### B.5.2 CI Stages (Logical)

CI **MUST** perform at minimum:

1. **Configure & Build**

   * CMake configure and build (Debug and/or Release).
2. **Unit Tests**

   * Run all backend-independent unit tests.
3. **System Tests**

   * Run CLI black-box tests (`validate`, `list`, minimal `run`).
4. **Adapter Smoke Tests**

   * Run where adapters are available for the platform.
5. **Artifact Checks**

   * Verify output schema files and assets are packaged/installed as expected (especially in release workflows).

### B.5.3 Sanitizer and Analysis Jobs (Recommended)

1. On supported platforms, CI **SHOULD** run:

   * AddressSanitizer (ASan),
   * UndefinedBehaviorSanitizer (UBSan),
     for IR parsing and harness core tests.
2. CI **SHOULD** run static analysis (clang-tidy) in at least one job if maintainable.

## B.6 Test Data Management

1. The repository **MUST** include:

   * minimal IR fixtures for positive/negative parsing tests,
   * manifest fixtures for validation tests,
   * synthetic RunResult fixtures for output schema tests.
2. Test fixtures **MUST** be deterministic and small to keep CI fast.

## B.7 Test Reporting and Diagnostics

1. Test output **MUST** include clear failure diagnostics with:

   * failing scene/backend IDs (where applicable),
   * reason codes and validation errors,
   * pointers to generated artifacts/logs.
2. CI failures **SHOULD** upload logs and minimal outputs as artifacts for debugging.

## Acceptance Criteria

1. The strategy defines unit, integration, system, and acceptance test categories with specific required test cases.
2. Tests validate correctness, determinism, schema compliance, and adapter contract behavior without relying on fragile performance thresholds.
3. CI requirements specify a cross-platform matrix and minimum stages, including manifest/IR validation and minimal benchmark execution.
4. Optional image-based diagnostics are explicitly non-gating by default and tolerance-based when used.
5. Fuzzing and sanitizer usage are recommended with clear scope.

## Dependencies

1. Chapter 5 — Data Design (IR, manifest, hashing, schema versioning).
2. Chapter 6 — Interfaces (adapter contract, output schemas, reason codes).
3. Chapter 8 — Runtime Behavior (timed boundaries, outcomes, isolation mode behavior).

---

