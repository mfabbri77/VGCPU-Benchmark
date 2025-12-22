<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Chapter 4 — System Architecture Overview

## Purpose

This chapter defines the high-level architecture of the benchmark suite: major components, their responsibilities, primary data flows, and the boundaries that enable reproducibility, cross-backend fairness, and extensibility. It establishes the system decomposition used by later chapters for detailed design.

## 4.1 Architectural Goals and Principles

1. The architecture **MUST** separate concerns between:

   * Scene representation (IR),
   * Backend integration (adapters),
   * Measurement (harness),
   * Reporting and metadata capture.
2. The architecture **MUST** allow adding a backend by implementing an adapter without changing scene assets.
3. The architecture **MUST** allow adding a scene by adding IR assets without modifying backend adapters.
4. The architecture **MUST** ensure that timed benchmark sections exclude I/O, IR decoding, and report generation.
5. The architecture **MUST** make CPU-only enforcement explicit and auditable per backend.

## 4.2 Component Model

### 4.2.1 CLI Frontend

**Responsibilities**

1. Parse and validate CLI arguments and configuration files (if supported).
2. Resolve selected backends, scenes, and benchmark policies into an execution plan.
3. Initiate runs and select output formats/locations.

**Normative Requirements**

* The CLI frontend **MUST** not perform rendering itself; it delegates to the Harness Core.
* The CLI frontend **MUST** produce deterministic resolution of identifiers (backend IDs, scene IDs).

### 4.2.2 Harness Core (Benchmark Orchestrator)

**Responsibilities**

1. Construct the benchmark matrix of (scene × backend × configuration).
2. Manage warm-up, iteration counts, repetition strategy, and (optional) process isolation mode.
3. Invoke adapters with prepared IR and preallocated target buffers.
4. Collect raw measurements (wall and CPU time) and produce summarized statistics.

**Normative Requirements**

* The Harness Core **MUST** define the “timed section boundary” and enforce exclusion of non-render work.
* The Harness Core **MUST** support a single-process mode and **MAY** support a multi-process isolation mode.
* The Harness Core **MUST** record benchmark policy parameters as part of run metadata.

### 4.2.3 Platform Abstraction Layer (PAL)

**Responsibilities**

1. Provide cross-platform access to:

   * High-resolution wall-clock timers,
   * CPU-time measurement APIs,
   * Process information and environment metadata,
   * Optional process spawning/IPC for isolation mode.

**Normative Requirements**

* OS-specific APIs **MUST** be encapsulated behind stable PAL interfaces.
* The PAL **MUST** provide monotonic wall-clock time.
* The PAL **MUST** provide CPU time measurement per benchmark case, with documented semantics (process vs thread) per platform.

### 4.2.4 IR Runtime

**Responsibilities**

1. Load IR assets and validate IR version, structure, and basic invariants.
2. Prepare an in-memory, replay-ready representation (“Prepared Scene”) optimized for iteration replay.
3. Provide semantic utilities shared across adapters (e.g., canonical color/premultiplication helpers) where applicable.

**Normative Requirements**

* IR parsing/validation **MUST** occur outside the timed section.
* Prepared Scene representation **MUST** be immutable during replay.
* The IR Runtime **MUST** expose the IR version, scene hash, and scene metadata for reporting.

### 4.2.5 Scene Registry and Manifest

**Responsibilities**

1. Maintain a catalog of available scenes, including:

   * Scene ID,
   * IR asset location,
   * Declared feature requirements (capabilities needed),
   * Default output dimensions and settings,
   * Scene version/hash.

**Normative Requirements**

* Scene registration **MUST** not require backend-specific code.
* The manifest **MUST** be used to determine compatibility with backend capabilities prior to execution.

### 4.2.6 Backend Adapter Layer

**Responsibilities**

1. Provide a uniform interface for backend operations:

   * Initialization and teardown,
   * CPU-only surface creation,
   * Replay of Prepared Scene,
   * Capability reporting,
   * Backend metadata reporting.
2. Implement mappings from IR commands to backend API calls.

**Normative Requirements**

* Each adapter **MUST** declare capabilities used by the compatibility checker.
* Each adapter **MUST** implement CPU-only enforcement and report enforcement decisions in metadata.
* Adapters **SHOULD** avoid dynamic allocation during replay, and **MUST** document unavoidable allocations.

### 4.2.7 Compatibility and Policy Engine

**Responsibilities**

1. Determine whether a given scene can run on a backend based on capability requirements.
2. Apply default rule: skip unsupported (scene, backend) pairs.
3. Apply explicit fallback modes only when user-selected.

**Normative Requirements**

* The engine **MUST** produce machine-readable compatibility outcomes and reasons (supported/unsupported/fallback-applied).
* The engine **MUST** be invoked before any timed measurements for a case.

### 4.2.8 Results Store and Reporting

**Responsibilities**

1. Persist raw samples and/or aggregated statistics according to configured output policy.
2. Emit machine-readable results (JSON/CSV) and a human-readable summary.
3. Emit run metadata (environment, dependency versions, build info, benchmark policy, backend config).

**Normative Requirements**

* Output schemas **MUST** be versioned.
* Results **MUST** include both CPU time and wall time statistics when available.
* Reporting **MUST** not run inside the timed section.

## 4.3 Primary Data Flows

### 4.3.1 Benchmark Run Flow (Single Process)

1. CLI resolves selection → Harness Core builds execution plan.
2. Scene Registry provides scene manifests; IR Runtime loads and prepares scenes.
3. Adapters are initialized and expose capabilities/metadata.
4. Compatibility Engine filters/labels benchmark cases (execute/skip/fallback).
5. For each executable case:

   * Warm-up (untimed summary; warm-up itself may be timed separately but not included in primary stats).
   * Timed iterations: adapter replays Prepared Scene onto a CPU-backed target buffer.
   * Harness collects wall and CPU timing samples.
6. Results Store aggregates statistics; Reporting emits outputs and metadata.

### 4.3.2 Optional Isolation Flow (Multi-Process)

1. Harness Core spawns a worker process per backend or per case (policy-defined).
2. Parent process sends:

   * Case descriptor (backend ID, scene ID, configuration),
   * Path to IR asset or pre-serialized Prepared Scene (if supported).
3. Worker executes warm-up and timed iterations, returns:

   * Raw or aggregated timing samples,
   * Backend metadata and any failure diagnostics.
4. Parent aggregates results and writes reports.

**Normative Requirements**

* Multi-process mode **MUST** preserve identical output schemas to single-process mode.
* Worker processes **MUST** be invoked with explicit CPU-only enforcement settings, not inherited implicitly.

## 4.4 Key Boundaries and Contracts

1. **IR Contract Boundary**: IR Runtime defines the canonical interpretation; adapters implement mappings consistent with it.
2. **Adapter Contract Boundary**: Harness Core calls adapter interfaces without backend-specific knowledge.
3. **Timing Boundary**: Only scene replay (and required minimal adapter state changes) may occur within timed iterations.
4. **Metadata Boundary**: Each run must produce sufficient metadata to identify configuration and dependencies.

## 4.5 Extension Points

1. **New Backend**: Add a new adapter module implementing the adapter interface; update adapter registry; provide capability mapping and CPU-only enforcement.
2. **New Scene**: Add an IR asset and manifest entry declaring capability requirements and defaults.
3. **New Metric**: Extend the measurement subsystem and schema (e.g., allocations, peak RSS) behind a capability/availability flag and consistent reporting.

## Acceptance Criteria

1. The architecture identifies distinct components and responsibilities for CLI, harness, PAL, IR runtime, adapters, compatibility policy, and reporting.
2. The document defines at least one end-to-end data flow from CLI to results, with explicit timed vs untimed boundaries.
3. CPU-only enforcement is represented as an explicit responsibility of the adapter layer and is reported through metadata.
4. The architecture provides clear extension points for adding backends and scenes without cross-cutting changes.
5. Single-process and optional multi-process modes are described, with identical schema guarantees.

## Dependencies

1. Chapter 1 — Introduction and Scope (scope, backends, platforms).
2. Chapter 2 — Requirements (CLI, IR semantics, measurement, reporting).
3. Chapter 3 — Constraints, Assumptions, and Stakeholders (CMake, cross-platform, CPU-only, distribution).

---

