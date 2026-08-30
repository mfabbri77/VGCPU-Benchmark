# Chapter 3 — Component Design (Responsibilities, Flows, Invariants, Failure Modes, Test Boundaries)

## [ARCH-10] Component inventory (authoritative)
This chapter defines the internal components and their responsibilities, mapped onto the observed modules/targets.

| Component | Code location (observed) | Target | Responsibility |
|---|---|---|---|
| [ARCH-10-01] CLI Frontend | `src/cli/*` | `vgcpu-benchmark` | Parse args/commands; dispatch to harness/reporting; user interaction |
| [ARCH-10-02] Common Types | `src/common/*` | `vgcpu_core` | `Status/Result<T>`, error codes, capability sets, common utilities |
| [ARCH-10-03] PAL | `src/pal/*` | `vgcpu_core` | Monotonic timing, env/CPU info, filesystem helpers (minimal) |
| [ARCH-10-04] Assets & Manifest | `src/assets/*`, `/assets/scenes/*` | `vgcpu_core` | Scene enumeration, manifest parsing, asset path resolution |
| [ARCH-10-05] IR Loader / Decoder | `src/ir/*` | `vgcpu_core` | Load `.irbin` scenes into an in-memory canonical representation |
| [ARCH-10-06] Adapter Registry | `src/adapters/*` | `vgcpu_adapters` | Backend discovery; feature flags; create adapter instances |
| [ARCH-10-07] Backend Adapters | `src/adapters/backends/*` (inferred) | `vgcpu_adapters` | Translate canonical IR into backend calls; manage backend resources |
| [ARCH-10-08] Benchmark Harness | `src/harness/*` | `vgcpu_harness` | Orchestrate warmup/measured loops; compute stats; enforce run policy |
| [ARCH-10-09] Statistics | `src/harness/stats*` (inferred) | `vgcpu_harness` | Robust aggregation: p50/p90/p99 etc. (per [REQ-04-04]) |
| [ARCH-10-10] Reporting | `src/reporting/*` | `vgcpu_reporting` | Emit CSV/JSON + summary (per [REQ-05]) with schema version (per [DEC-SCOPE-03]) |

## [ARCH-11] Allowed dependencies (enforced)
- [ARCH-11-01] Dependency rules are as defined in Ch2 ([ARCH-04], [DEC-ARCH-03]) and MUST be enforced by:
  - [REQ-18] a CMake-level include-visibility policy (private/public headers), and
  - [REQ-19] an automated “include-graph” lint in CI (implemented in Ch8/Ch7).

(See Ch7 for the concrete enforcement mechanism.)

## [ARCH-12] Core data model (conceptual)
### [ARCH-12-01] Canonical IR in-memory model
- [ARCH-12-01a] `SceneId`: stable string key (from manifest).
- [ARCH-12-01b] `SceneMeta`: human name, tags, dimensions (if applicable), expected invariants.
- [ARCH-12-01c] `SceneIR`: decoded commands and resources (paths, paints, transforms, text runs if present).
- [ARCH-12-01d] `PreparedScene`: backend-independent preprocessed representation suitable for adapters.
  - Examples: resolved resource tables, normalized path segments, precomputed bounds.

- [DEC-ARCH-07] Canonical `SceneIR` is immutable after load; any backend-specific preprocessing MUST happen in adapter “prepare” stage and must not mutate shared IR.
  - Rationale: prevents cross-backend contamination and makes caching safe.
  - Alternatives: allow adapters to mutate IR.
  - Consequences: adapter API must expose a prepare/build phase; shared IR must be reference-counted/owned centrally (Ch5).

### [ARCH-12-02] Benchmark result model
- [ARCH-12-02a] `RunConfig`: backend, scenes, warmup, reps, thread count, affinity flags, build info, schema version.
- [ARCH-12-02b] `SceneRunSample`: vector of measured durations (ns) or aggregated moments.
- [ARCH-12-02c] `SceneStats`: {min, mean, median, p50, p90, p99, stddev?} + sample_count.
- [ARCH-12-02d] `RunReport`: per-scene stats + environment metadata + errors encountered.

- [REQ-20] Report generation MUST be deterministic given identical inputs and measured samples ordering.
  - Note: timings themselves may vary; this requirement is about computation and output ordering.

## [ARCH-13] Primary execution flow (state machine)
### [ARCH-13-01] Run lifecycle state machine
```mermaid
stateDiagram-v2
  [*] --> ParseArgs
  ParseArgs --> LoadManifest: ok
  ParseArgs --> ExitError: invalid args

  LoadManifest --> SelectScenes: ok
  LoadManifest --> ExitError: missing/malformed manifest

  SelectScenes --> CreateAdapter: backend available
  SelectScenes --> ExitError: backend not found/disabled

  CreateAdapter --> LoadScene: adapter init ok
  CreateAdapter --> ExitError: adapter init fail

  LoadScene --> PrepareScene: decode ok
  LoadScene --> ExitError: IR decode fail

  PrepareScene --> Warmup: prepare ok
  PrepareScene --> ExitError: prepare fail

  Warmup --> Measure
  Measure --> AggregateStats
  AggregateStats --> EmitReports

  EmitReports --> [*]
  ExitError --> [*]
````

### [ARCH-13-02] Per-scene inner loop

* [ARCH-13-02a] Warmup loop:

  * Perform `warmup_iters` executions, discarding samples.

* [ARCH-13-02b] Measured loop:

  * Perform `reps` executions, recording duration per rep (or streaming to stats accumulator).

* [REQ-21] The measured loop MUST NOT perform filesystem I/O.

* [REQ-22] The measured loop MUST NOT emit logs (beyond a single “start/end scene” event outside the loop).

* [REQ-23] The measured loop MUST avoid dynamic allocations on the VGCPU side (best-effort; backend may allocate internally).

  * Enforcement: debug counters + allocator hooks in non-Release builds (Ch5/Ch8).

## [ARCH-14] Component design details

### [ARCH-14-A] CLI Frontend ([ARCH-10-01])

**Responsibilities**

* Parse command and options; validate; create `RunConfig`.
* Implement commands:

  * [REQ-06-01] `list` (enumerate available backends)
  * [REQ-06-02] `run` (execute benchmark)
* Select output locations and format options; pass to reporting.

**Key invariants**

* [REQ-24] CLI MUST return non-zero exit code on any run that fails to execute requested scenes.
* [REQ-25] CLI MUST print `--help` without requiring any backend dependencies.

**Failure modes**

* Invalid args → structured error + help hint.
* Unsupported backend name → list alternatives.
* Output path not writable → fail before benchmarking begins.

**Test boundary**

* [TEST-01] `cli_help_smoke`: `vgcpu-benchmark --help` exits 0 and prints usage.
* [TEST-02] `cli_list_smoke`: `vgcpu-benchmark list` exits 0 and contains Tier-1 backends.
* [TEST-03] `cli_run_null_smoke`: `vgcpu-benchmark run --backend null --scenes <small>` exits 0 and produces reports.

---

### [ARCH-14-B] PAL ([ARCH-10-03])

**Responsibilities**

* Provide monotonic timestamp API and duration conversions.
* Provide environment metadata collection:

  * OS name/version, CPU brand string (best effort), core count, build type, toolchain string (if available).
* Provide limited helpers: path normalization and time formatting (not file walking).

**Key invariants**

* [REQ-26] Timing MUST use a monotonic clock (no wall-clock adjustments) (supports [REQ-04-03]).
* [REQ-27] PAL MUST compile without linking to any backend.

**Failure modes**

* Environment queries may fail → populate “unknown” fields but do not abort runs (unless required fields are missing).
* Timer init failure (rare) → fail fast with a specific error code.

**Test boundary**

* [TEST-04] `pal_monotonic_increases`: successive calls produce non-decreasing timestamps.
* [TEST-05] `pal_env_metadata_present`: required fields exist or are explicitly “unknown” (no empty/null).

---

### [ARCH-14-C] Assets & Manifest ([ARCH-10-04])

**Responsibilities**

* Load and parse `/assets/scenes/manifest.json`.
* Resolve scene entries into file paths for `.irbin` payloads.
* Provide scene filtering by id/tag (CLI `--scenes` / `--tags`).

**Key invariants**

* [REQ-28] Manifest parsing MUST validate schema (required keys, types).
* [REQ-29] Scene ordering MUST be stable (sorted by `SceneId`) unless user explicitly overrides.

  * Supports [REQ-13-02] and [REQ-20].

**Failure modes**

* Manifest missing/corrupt → fail before benchmarking starts.
* Scene file missing → fail that scene, and (policy) fail the run unless user requested `--continue-on-error`.

  * [DEC-ARCH-08] Default policy: **fail the run** if any requested scene cannot be loaded.

    * Rationale: benchmark comparability depends on a complete set.
    * Alternatives: skip missing scenes by default.
    * Consequences: CLI adds `--continue-on-error` to allow partial runs (Ch4).

**Test boundary**

* [TEST-06] `manifest_schema_validation`: invalid manifest is rejected with a specific error.
* [TEST-07] `scene_ordering_stable`: same manifest yields stable ordered list.

---

### [ARCH-14-D] IR Loader / Decoder ([ARCH-10-05])

**Responsibilities**

* Decode `.irbin` into `SceneIR` (canonical, backend-independent).
* Validate IR invariants (bounds, command ranges, resource indices).
* Optionally precompute a `PreparedScene` shared representation.

**Key invariants**

* [REQ-30] IR loader MUST reject malformed input deterministically with stable error codes.
* [REQ-31] IR loader MUST not depend on any backend libraries (supports [DEC-ARCH-03]).

**Failure modes**

* Unsupported IR version → explicit “unsupported version” error.
* Corrupt file / out-of-range indices → decode error.

**Test boundary**

* [TEST-08] `ir_decode_roundtrip_small`: known small IR decodes successfully and matches expected structure.
* [TEST-09] `ir_rejects_corrupt`: corrupt IR returns the correct error code and does not crash.

---

### [ARCH-14-E] Adapter Registry ([ARCH-10-06])

**Responsibilities**

* Maintain a registry of compiled-in backends with:

  * name, version, tier (Tier-1/Optional), build-enabled flag,
  * capability set (features supported).
* Create adapter instances via factory functions.

**Key invariants**

* [REQ-32] Registry MUST list only backends that are compiled-in and linkable in this build (no phantom entries).
* [REQ-33] Backend names MUST be stable identifiers (CLI contract).

**Failure modes**

* Backend compiled but missing runtime deps (e.g., DLL not found) → adapter init failure with clear error message.

**Test boundary**

* [TEST-10] `registry_contains_tier1`: Tier-1 backends appear on all OSes in CI.
* [TEST-11] `registry_factory_errors`: requesting a disabled backend returns “not enabled” error.

---

### [ARCH-14-F] Backend Adapters ([ARCH-10-07])

**Responsibilities**

* Translate `PreparedScene` into backend-specific resources and render calls.
* Expose three phases:

  * `Init` (create backend context/device objects)
  * `Prepare(scene)` (compile/convert IR → backend resources)
  * `Render(scene)` (execute one iteration; hot path)
  * `Shutdown` (release resources)

**Key invariants**

* [REQ-34] Adapters MUST NOT perform filesystem I/O in `Render` (supports [REQ-21]).
* [REQ-35] Adapters MUST be thread-safe per declared mode:

  * [REQ-35-01] either “single-thread only” (default), OR
  * [REQ-35-02] explicitly supports multi-threaded scene execution (rare).
* [REQ-36] Adapter `Render` MUST be callable repeatedly with identical semantics (idempotent w.r.t. external state).

**Failure modes**

* Init failure (missing deps, unsupported CPU features) → fail before any measurement.
* Prepare failure (IR feature not supported) → fail that backend/scene combination.
* Render failure → fail run unless `--continue-on-error` is set; always include error in report.

**Test boundary**

* [TEST-12] `adapter_null_renders`: null backend renders without error and minimal overhead.
* [TEST-13] `adapter_prepare_supported_scenes`: Tier-1 backends can prepare all “Tier-1 scene set”.
* [TEST-14] `adapter_no_io_in_render` (best-effort): instrumentation asserts no VGCPU-side file operations in render.

---

### [ARCH-14-G] Benchmark Harness & Statistics ([ARCH-10-08], [ARCH-10-09])

**Responsibilities**

* Execute scenes per the run policy:

  * stable ordering,
  * warmup + measured reps,
  * optional thread count / affinity.
* Collect timing samples using PAL monotonic clock.
* Produce `SceneStats` per scene and aggregate `RunReport`.

**Key invariants**

* [REQ-37] Harness MUST separate warmup from measurement (supports [REQ-04-01]).
* [REQ-38] Harness MUST compute percentiles correctly and deterministically from samples (supports [REQ-04-04], [REQ-20]).
* [REQ-39] Harness MUST record sufficient metadata to reproduce run configuration (supports [REQ-02-01]).

**Failure modes**

* Timer issues → fail run with explicit error.
* Arithmetic overflow or NaNs (if using floating) → treat as bug; assert in debug; return error in release.

**Test boundary**

* [TEST-15] `stats_percentiles_known`: given a known sample set, p50/p90/p99 match expected values.
* [TEST-16] `harness_warmup_not_counted`: warmup samples do not appear in measured stats.
* [TEST-17] `harness_ordering_stable`: repeated runs produce stable scene order in report.

---

### [ARCH-14-H] Reporting ([ARCH-10-10])

**Responsibilities**

* Serialize `RunReport` to:

  * [REQ-05-01] CSV
  * [REQ-05-02] JSON
  * [REQ-05-03] human summary (stdout)
* Include schema version per [DEC-SCOPE-03].

**Key invariants**

* [REQ-40] JSON MUST include `schema_version` and `tool_version`.
* [REQ-41] CSV MUST be stable column-ordered and include schema version as a header comment where feasible.
* [REQ-42] Reporting MUST NOT reorder scenes beyond the stable harness ordering.

**Failure modes**

* Write failure (permissions/disk full) → fail after benchmarking but before exit; still print summary to stdout if possible.

**Test boundary**

* [TEST-18] `report_json_schema_version`: JSON contains required fields and correct schema version.
* [TEST-19] `report_csv_columns_stable`: CSV header matches expected canonical order.
* [TEST-20] `report_summary_nonempty`: summary includes backend + scene count + key stats.

## [ARCH-15] Cross-cutting policies (component-level)

### [ARCH-15-01] Error propagation (component contract)

* [DEC-ARCH-09] Use `Status/Result<T>` as the primary error transport across components; exceptions MAY be used only inside third-party glue layers if fully caught at the adapter boundary.

  * Rationale: predictable control flow and easier CLI/reporting integration.
  * Alternatives: exceptions end-to-end.
  * Consequences: Ch4 defines error codes + mapping, and tests assert stable codes.

### [ARCH-15-02] Logging policy (benchmark integrity)

* [REQ-43] Any log emission MUST occur outside the measured loop (supports [REQ-22] and [REQ-02-03]).
* [REQ-44] Logs MUST include run-id and scene-id fields for correlation.

### [ARCH-15-03] Benchmark noise control knobs

* [REQ-45] Harness MUST support:

  * fixed seed (even if currently unused),
  * optional CPU affinity/pinning best-effort,
  * optional “single-thread forced” mode regardless of backend internal threading.
* [DEC-ARCH-10] Default harness mode is single-threaded scene execution; multi-threading is opt-in and treated as a separate benchmark dimension.

  * Rationale: comparability and reduced noise.
  * Alternatives: auto-parallelize by core count.
  * Consequences: CLI adds `--threads` but default is 1; reporting includes thread count prominently.

## [ARCH-16] Component-level budgets & invariants (enforcement summary)

| Invariant / budget                       | ID                                 | Enforced by                                            |
| ---------------------------------------- | ---------------------------------- | ------------------------------------------------------ |
| No I/O in measured loop                  | [REQ-21]                           | code review + instrumentation tests ([TEST-14])        |
| No per-iteration logging                 | [REQ-22], [REQ-43]                 | harness structure + unit tests + code review           |
| Avoid VGCPU allocations in measured loop | [REQ-23]                           | optional allocator hooks (Ch5/Ch8)                     |
| Stable scene ordering                    | [REQ-29]                           | manifest loader + harness tests ([TEST-07], [TEST-17]) |
| Deterministic stats computation          | [REQ-38], [REQ-20]                 | stats tests ([TEST-15])                                |
| Schema versioned outputs                 | [DEC-SCOPE-03], [REQ-40], [REQ-41] | report tests ([TEST-18], [TEST-19])                    |

## [ARCH-17] Traceability notes

* [ARCH-17-01] This chapter introduces [REQ-18..45] and [TEST-01..20] as forward-referenced contracts.
* [ARCH-17-02] Ch4 will define concrete internal interfaces for adapters/harness/reporting and encode thread-safety + error contracts.
* [ARCH-17-03] Ch5/Ch6 will formalize memory/concurrency rules to uphold [REQ-21..23] and [REQ-35..39].
