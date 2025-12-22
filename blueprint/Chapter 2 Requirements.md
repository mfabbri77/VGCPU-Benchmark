# Chapter 2 — Requirements (Functional & Non-Functional)

## Purpose

This chapter specifies the functional and non-functional requirements for the benchmark suite, including IR handling, backend adapters, measurement methodology, reporting, reproducibility, and cross-platform operation. Requirements are stated as testable obligations to enable implementation and acceptance verification.

## 2.1 Functional Requirements

### 2.1.1 CLI and Execution Control

1. The system **MUST** provide a CLI executable that can run benchmarks for one or more selected backends and scenes.
2. The CLI **MUST** support selecting:

   * A subset of backends (by stable backend identifiers).
   * A subset of scenes (by stable scene identifiers).
   * One or more output resolutions where applicable (scene-defined default and optional overrides).
3. The CLI **MUST** support configuring benchmark policy parameters, including at minimum:

   * Warm-up duration and/or warm-up iteration count.
   * Measurement iteration count and/or minimum measurement time.
   * Process-level repetitions (e.g., multiple independent runs) where supported.
4. The CLI **MUST** support a “list” mode that enumerates available backends, scenes, and backend capability flags.
5. The CLI **MUST** support a “metadata” mode that prints environment and build metadata without running a benchmark.

### 2.1.2 IR Specification and Scene Handling

1. The system **MUST** define a versioned IR for scenes that is sufficient to represent all in-scope vector features (Chapter 1).
2. The IR **MUST** define a strict semantic contract for:

   * Path command interpretation (coordinate system, winding, closure).
   * Fill rules (Non-Zero, Even-Odd) and fallback behavior when unsupported.
   * Stroke semantics (caps, joins, miter limits, dashes) and fallback behavior when unsupported.
   * Gradient interpretation (stop positions, color space assumptions, extend modes if used).
   * Alpha and premultiplication expectations for colors and output buffers.
   * Transform concatenation order and graphics-state scoping.
3. The system **MUST** support loading scenes from IR assets identified by scene ID.
4. The system **MUST** ensure deterministic replay of a scene given:

   * The same IR payload,
   * The same backend configuration,
   * The same output dimensions and canonical pixel format settings.
5. The system **SHOULD** support an optional human-editable scene representation (authoring format) only if it can be compiled into the canonical IR without altering semantics; however, dedicated authoring tooling is out of scope.

### 2.1.3 Backend Adapter Interface

1. Each backend **MUST** be integrated via an adapter implementing a common interface that:

   * Initializes backend state and CPU-only rendering target(s),
   * Replays the IR onto the backend,
   * Exposes backend capability information,
   * Exposes backend configuration metadata for reporting.
2. Adding a new backend **MUST** require implementing only the adapter interface and backend-specific build configuration, without modifying existing scene assets.
3. Adapters **MUST** provide a deterministic mapping from IR operations to backend API calls, subject to the IR semantic contract and documented capability limitations.
4. Adapters **MUST** avoid per-command dynamic allocation during replay where feasible; any unavoidable allocations **MUST** be measured/declared in adapter documentation and in metadata flags.

### 2.1.4 Capability Matrix and Feature Subsetting

1. The system **MUST** maintain a backend capability matrix that enumerates, at minimum:

   * Fill rule support (Non-Zero, Even-Odd),
   * Stroke caps/joins,
   * Dash support,
   * Gradient types (linear, radial),
   * Clipping support (if included in scenes),
   * Any known constraints affecting semantics (e.g., premultiplication expectations).
2. The harness **MUST** enforce feature subsets such that:

   * A scene requiring unsupported features is either:

     * Skipped for that backend with a machine-readable reason, or
     * Executed using a defined fallback mode explicitly selected by the user.
3. The default behavior **MUST** be to **skip** unsupported (scene, backend) combinations rather than silently degrading semantics.

### 2.1.5 Measurement Harness Requirements

1. The harness **MUST** measure both **wall time** and **CPU time** per (scene × backend × configuration) benchmark case.
2. The harness **MUST** implement a warm-up phase before measurements for each benchmark case.
3. The harness **MUST** support running multiple repetitions and compute summary statistics per case, including at minimum:

   * Median (p50),
   * A dispersion metric (p90 and/or MAD/IQR).
4. The harness **MUST** minimize measurement bias by:

   * Avoiding I/O in the timed section,
   * Avoiding scene parsing in the timed section (IR decode/prep must be outside timing where applicable),
   * Reusing allocated buffers across iterations where feasible.
5. The harness **MUST** record the exact benchmark policy (warm-up, iterations, repetitions) in output metadata.
6. The harness **SHOULD** support optional measurement of:

   * Allocation counts/bytes (where available),
   * Peak RSS (where available),
     provided the measurement method is documented and consistent enough to compare within a platform.

### 2.1.6 Output, Reporting, and Schemas

1. The system **MUST** produce machine-readable results in at least one of:

   * JSON, and/or
   * CSV.
2. Machine-readable outputs **MUST** be versioned with a schema version identifier.
3. Results **MUST** include, per benchmark case:

   * Backend ID and backend version/build identifiers,
   * Scene ID and scene version/hash,
   * Output dimensions and pixel format,
   * Wall-time statistics,
   * CPU-time statistics,
   * Skip/unsupported indicators and reasons (if applicable).
4. The system **MUST** produce a human-readable summary report that:

   * Lists benchmark cases executed and skipped,
   * Highlights key statistics per backend and per scene,
   * Includes environment and configuration metadata.
5. The reporting subsystem **SHOULD** support stable sorting/grouping to enable comparisons across runs (e.g., by backend then scene).

### 2.1.7 Reproducibility Kit

1. The project **MUST** provide reproducibility documentation describing:

   * Supported platforms and architectures,
   * Build prerequisites,
   * Build commands (single CMake entry point),
   * How to run benchmark suites with canonical configurations.
2. The system **MUST** capture and emit environment metadata including at minimum:

   * OS and kernel version (or equivalent),
   * CPU model and core count,
   * Available memory,
   * Compiler name/version and relevant build flags,
   * Backend library versions/commit hashes (where applicable),
   * Git commit of the benchmark suite.
3. Dependency versions **MUST** be pinned and recorded, including for system-installed dependencies where used.
4. The project **MUST** support generating release artifacts for GitHub Releases, including:

   * Source archives (or source tag-based release),
   * Prebuilt binaries for supported OS/architectures where feasible,
   * Checksums for release artifacts.

## 2.2 Non-Functional Requirements

### 2.2.1 Portability and Compatibility

1. The system **MUST** build and run on Linux, Windows, and macOS for x86_64 and ARM64 targets.
2. The build system **MUST** be CMake, serving as the single entry point for configuration, building, and dependency orchestration.
3. The system **MUST** support linking against:

   * Project-managed dependencies (fetched/pinned), and
   * System-installed dependencies, provided their versions are pinned and recorded.

### 2.2.2 Determinism and Repeatability

1. Scene replay **MUST** be deterministic with respect to IR interpretation and command order.
2. The system **MUST** avoid sources of non-determinism in benchmarked code paths where feasible (e.g., randomized tessellation parameters).
3. The system **SHOULD** provide a mode to validate scene determinism for a given backend by repeating runs and checking result stability (timing variance bounds are environment-dependent; this is a diagnostic mode).

### 2.2.3 Performance Overhead and Isolation

1. The measurement harness **MUST** ensure that benchmark overhead is small relative to measured work, including:

   * Minimal per-iteration bookkeeping,
   * No per-iteration logging.
2. The system **SHOULD** provide mechanisms to reduce interference, such as:

   * Affinity/pinning controls where supported,
   * Configurable process priority hints where supported,
   * Clear guidance in documentation for running on an idle system.

### 2.2.4 Maintainability and Extensibility

1. The system **MUST** be structured so that:

   * Adding a backend requires implementing an adapter and updating registration, and
   * Adding a scene requires adding IR assets and updating scene registration/manifest only.
2. The IR **MUST** be versioned and backward-compatible within a major version; breaking changes **MUST** increment the major version.
3. The codebase **SHOULD** be organized into modules with stable internal interfaces (harness core, IR runtime, adapter layer, reporting).

### 2.2.5 Reliability and Failure Handling

1. The harness **MUST** handle unsupported features and backend failures gracefully by:

   * Marking cases as skipped or failed,
   * Emitting a machine-readable reason,
   * Continuing other benchmark cases unless configured to stop on first failure.
2. The harness **MUST** return non-zero exit codes for:

   * Invalid CLI arguments,
   * Unrecoverable initialization failures,
   * A configured threshold of benchmark-case failures.

### 2.2.6 Security and Supply Chain (Baseline)

1. The project **MUST** document the provenance and versions of third-party dependencies.
2. Release artifacts **SHOULD** include checksums and reproducible build guidance to reduce supply-chain risk.
3. The benchmark suite **MUST NOT** require elevated privileges to run.

## Acceptance Criteria

1. A test plan can be derived from this chapter that verifies:

   * CLI selection and configuration of backends/scenes and benchmark policy.
   * Deterministic scene loading and replay via a versioned IR.
   * Adapter conformance to a common interface and CPU-only configuration logging.
   * Capability matrix enforcement (skip vs explicit fallback).
   * Wall-time and CPU-time measurement with warm-up and summary statistics.
   * Machine-readable outputs with schema versioning and required fields.
   * Reproducibility metadata capture and dependency version recording.
   * Cross-platform build via CMake and cross-platform run on the target OS/architectures.
2. For at least one backend and one scene, an automated run produces:

   * JSON and/or CSV output with complete metadata and statistics,
   * A human-readable summary report,
   * A deterministic scene hash/version reference in outputs.
3. For a scene requiring an unsupported feature on a backend, the benchmark run:

   * Skips the case by default,
   * Records a machine-readable skip reason,
   * Continues executing other cases.

## Dependencies

1. Chapter 1 — Introduction and Scope (scope and deliverables constraints).
2. Subsequent chapters:

   * Chapter 5 (IR data design) constrains IR semantics and versioning.
   * Chapter 6 (interfaces) constrains adapter interfaces and result schemas.
   * Appendix B (testing strategy) defines the detailed benchmarking methodology and validation.

---


