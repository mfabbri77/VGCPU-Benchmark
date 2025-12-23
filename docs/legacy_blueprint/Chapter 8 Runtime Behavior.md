<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Chapter 8 — Runtime Behavior (Key Flows, Error Handling)

## Purpose

This chapter specifies the runtime behavior of the benchmark suite: key execution flows, timed vs untimed boundaries, warm-up and measurement sequencing, optional isolation mode behavior, and error handling semantics. It defines required invariants to ensure repeatability and fair measurement across backends.

## 8.1 Execution Lifecycle Overview

1. A benchmark run **MUST** proceed through the following phases in order:

   1. Startup and configuration resolution
   2. Asset discovery and validation
   3. Backend discovery and initialization planning
   4. Case planning (matrix build + compatibility decisions)
   5. Case execution (warm-up + measurement)
   6. Aggregation and reporting
   7. Shutdown and exit code selection
2. The harness **MUST** define a strict boundary between **untimed setup** and **timed measurement** (see §8.3).

## 8.2 Key Runtime Flows

### 8.2.1 Startup and Configuration Resolution Flow

1. The CLI **MUST** parse user inputs into a validated `ExecutionPlan` containing:

   * selected backends and scenes,
   * benchmark policy parameters (warm-up, iterations/min duration, repetitions),
   * output settings (directory, formats),
   * compatibility policy (default skip; optional explicit fallback),
   * isolation mode selection.
2. If validation fails, the process **MUST** exit with a usage/configuration error code and **MUST NOT** create partial outputs unless an explicit “write diagnostics on failure” option is later introduced.

### 8.2.2 Asset Discovery and Validation Flow

1. The harness **MUST** load the scene manifest and enumerate candidate scenes.
2. For each selected scene, the harness **MUST**:

   * verify IR asset presence,
   * compute SceneHash and verify it matches the manifest,
   * validate IR version compatibility.
3. Manifest or hash mismatches **MUST** be treated as fatal by default.

### 8.2.3 Backend Discovery and Capability Collection Flow

1. The adapter registry **MUST** enumerate compiled-in adapters.
2. For each selected backend:

   * The harness **MUST** obtain `CapabilitySet` and `BackendMetadata` (static metadata MAY be available without full initialization).
   * If a backend is not compiled in, the harness **MUST** mark all cases for that backend as `SKIP` with reason `BACKEND_NOT_AVAILABLE` unless the user explicitly requested “fail if missing”, in which case it **MUST** be fatal.
3. Backend discovery **MUST** be deterministic with stable backend identifiers.

### 8.2.4 Case Planning and Compatibility Flow

1. The harness **MUST** build the case matrix from selected scenes × selected backends × selected configurations.
2. For each candidate case, the Compatibility Engine **MUST** compute a `CompatibilityDecision`:

   * `EXECUTE` if supported,
   * `SKIP` by default if unsupported,
   * `FAIL` if configured to fail on unsupported,
   * `FALLBACK` only if user-selected and a defined fallback mode exists.
3. The harness **MUST** record, for each non-executed case, stable reason codes including the set of missing features.

### 8.2.5 Case Execution Flow (Single-Process Mode)

For each case with `EXECUTE` (or `FALLBACK`) decision:

1. **Untimed Setup**

   1. Load and prepare the scene once (or reuse a cached PreparedScene).
   2. Initialize backend (if not already initialized for this run), enforcing CPU-only settings.
   3. Create surface/buffer (or reuse per configured reuse policy).
   4. Apply case configuration (dimensions, clear policy, thread policy).
2. **Warm-up Phase**

   * Execute warm-up iterations and/or warm-up duration as specified.
   * Warm-up **MUST** not contribute to primary reported timing statistics.
   * Warm-up **MAY** be recorded as ancillary metadata (counts/duration observed).
3. **Measurement Phase**

   * Execute timed iterations until the specified stop condition (fixed iterations and/or minimum time) is satisfied.
   * Collect per-iteration (or per-batch) wall time and CPU time samples according to the selected sampling policy.
4. **Untimed Teardown**

   * Optionally clear or reset surfaces if required by subsequent cases.
   * Do not emit reports inside the timed loop.

**Deterministic ordering**

* Case execution order **MUST** be deterministic by default (see Chapter 3, §3.2.1 Technical Assumptions).

### 8.2.6 Case Execution Flow (Multi-Process Isolation Mode)

If isolation mode is `process`:

1. The parent process **MUST** plan cases as in §8.2.4.
2. For each executable case (or per-backend grouping if chosen), the parent **MUST** spawn a worker process with:

   * explicit backend selection,
   * explicit CPU-only enforcement parameters,
   * explicit scene selection and case configuration,
   * a request to return results in the defined IPC schema.
3. The worker **MUST** perform:

   * scene load/validation/preparation,
   * backend initialization,
   * warm-up,
   * measurement,
   * response emission,
   * controlled shutdown.
4. The parent **MUST** treat worker crashes/timeouts as `FAIL` outcomes with reason codes indicating `WORKER_CRASH` or `WORKER_TIMEOUT`.

## 8.3 Timed vs Untimed Boundaries

1. The timed region for measurement **MUST** include only:

   * the backend adapter’s `Render(prepared_scene, surface, render_params)` call, and
   * the minimal timing readout overhead required to measure duration.
2. The timed region **MUST NOT** include:

   * IR parsing/validation/preparation,
   * manifest I/O,
   * backend initialization,
   * surface creation/destruction,
   * **output buffer allocation/resizing** (buffer MUST be pre-sized by harness before measurement loop),
   * report serialization or file I/O,
   * allocation-heavy setup that can be performed once per case.
3. **Buffer clearing semantics**: The `kClear` opcode in the IR stream is the authoritative method for clearing the output buffer. Adapters **MUST NOT** perform additional `memset`/`fill` operations on the buffer before rendering. The harness guarantees the buffer is sized correctly; buffer contents before `kClear` are undefined.
4. Any backend-specific state resets required between iterations **MUST** either:

   * be part of the `Render` contract and therefore included in the timed region for all backends, or
   * be performed outside the timed region consistently and documented as a measurement policy decision.

## 8.4 Warm-up and Iteration Semantics

1. Warm-up **MUST** be executed for each (scene × backend × configuration) case before measurement.
2. Warm-up stop conditions **MUST** be one or both of:

   * warm-up iterations, and/or
   * warm-up duration (ms).
3. Measurement stop conditions **MUST** be one or both of:

   * fixed iteration count, and/or
   * minimum measurement duration.
4. The harness **MUST** record the realized counts/durations (warm-up and measurement) for transparency.

## 8.5 Caching and Reuse Policies

1. PreparedScene instances **SHOULD** be cached per scene for the duration of a run to avoid repeated parsing costs.
2. Surfaces/buffers **SHOULD** be reused across iterations of the same case.
3. Surfaces/buffers **MAY** be reused across cases only if:

   * dimensions and pixel format match, and
   * backend semantics do not carry over unintended state.
4. Any reuse across cases **MUST** be documented and **MUST** be applied uniformly across backends.

## 8.6 Error Handling and Outcome Classification

### 8.6.1 Outcome Types

Each benchmark case **MUST** end in exactly one outcome:

1. `SUCCESS` — executed and produced timing stats.
2. `SKIP` — not executed due to policy/capability/unavailability.
3. `FAIL` — attempted execution but did not complete successfully.

### 8.6.2 Stable Reason Codes

The system **MUST** emit stable machine-readable reason codes. At minimum:

**Skip reasons**

* `BACKEND_NOT_AVAILABLE`
* `UNSUPPORTED_FEATURE:<feature>` (may appear multiple times)
* `POLICY_SKIP_UNSUPPORTED`
* `SCENE_FILTERED_OUT` (if filtering yields cases that are enumerated but not run)

**Fail reasons**

* `IR_VALIDATION_FAILED`
* `IR_VERSION_UNSUPPORTED`
* `BACKEND_INIT_FAILED`
* `SURFACE_CREATE_FAILED`
* `RENDER_FAILED`
* `WORKER_CRASH` (isolation mode)
* `WORKER_TIMEOUT` (isolation mode)
* `OUTPUT_WRITE_FAILED`

### 8.6.3 Failure Containment

1. By default, a single case failure **MUST NOT** abort the entire run.
2. If `--fail-fast` is enabled, the harness **MUST** stop after the first `FAIL` outcome and proceed to reporting with partial results.
3. Fatal failures (run-level) **MUST** include at minimum:

   * inability to load/validate the manifest (unless explicitly overridden),
   * inability to create the output directory (if output is required),
   * inability to load any selected backend adapters when user explicitly required them.

## 8.7 Determinism and Reproducibility at Runtime

1. The harness **MUST** emit run metadata sufficient to reproduce case planning and execution ordering:

   * selected backends/scenes,
   * case ordering rule,
   * benchmark policy parameters,
   * capability matrix snapshots (or capability hash).
2. Any non-deterministic behavior detected or configured (e.g., backend internal threading) **MUST** be recorded in metadata and surfaced in the human-readable summary.

## Acceptance Criteria

1. A run can be traced through well-defined phases with explicit timed vs untimed boundaries.
2. The single-process case execution flow specifies: untimed setup, warm-up, timed measurement, untimed teardown.
3. Isolation mode behavior is specified, including worker crash/timeout handling and schema consistency.
4. Warm-up and measurement stop conditions are defined and recorded as metadata.
5. Case outcomes are strictly classified into SUCCESS/SKIP/FAIL with stable reason codes, and failure containment rules are explicit.

## Dependencies

1. Chapter 4 — System Architecture Overview (component roles and data flows).
2. Chapter 6 — Interfaces (adapter lifecycle, timing APIs, output schema fields).
3. Chapter 7 — Component Design (module responsibilities and boundaries).

---

