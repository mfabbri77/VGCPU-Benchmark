<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Chapter 7 — Component Design (Modules, Responsibilities)

## Purpose

This chapter specifies the internal module decomposition and the responsibilities of each module, including key internal contracts, data ownership, configuration surfaces, and extension procedures. It translates the architectural components (Chapter 4) into implementation-ready module boundaries.

## 7.1 Repository and Module Layout

1. The codebase **MUST** be organized into explicit modules with clear ownership boundaries.
2. The following modules **MUST** exist (names are logical; directory names may vary but MUST be unambiguous):

* **7.1.1 `cli`**
* **7.1.2 `harness`**
* **7.1.3 `pal`** (platform abstraction layer)
* **7.1.4 `ir`** (IR runtime + validation + preparation)
* **7.1.5 `adapters`** (one submodule per backend)
* **7.1.6 `reporting`** (schema + writers + summary)
* **7.1.7 `assets`** (scene manifest + IR assets + schemas)

3. Each module **MUST** have:

   * A public interface boundary (headers/contracts) consumed by other modules,
   * A private implementation area,
   * Unit tests where applicable (test strategy specified later).

## 7.2 Module Specifications

### 7.2.1 Module `cli`

**Responsibilities**

1. Parse CLI arguments into a validated `RunRequest`.
2. Resolve scene and backend selections using registries.
3. Invoke `harness` with a fully specified `ExecutionPlan`.
4. Handle exit codes and user-facing diagnostics.

**Ownership and Constraints**

* `cli` **MUST NOT** perform rendering, timing, IR parsing, or report formatting logic beyond basic argument validation.
* `cli` **MUST** be deterministic in identifier resolution (no unordered iteration affecting selection).

**Key Interfaces**

* Consumes: `harness.Run(ExecutionPlan)`, `assets.SceneCatalog`, `adapters.BackendCatalog`.

### 7.2.2 Module `harness`

**Responsibilities**

1. Construct the benchmark matrix (cases) from `ExecutionPlan`.
2. Perform compatibility evaluation (with reasons).
3. Execute warm-up and timed iterations per case.
4. Collect samples and compute required statistics.
5. Support optional process isolation mode (policy-driven).
6. Produce a `RunResult` suitable for `reporting`.

**Ownership and Constraints**

* `harness` **MUST** define and enforce the timed-section boundary.
* `harness` **MUST** ensure IR validation/preparation and all I/O are outside the timed section.
* `harness` **MUST** ensure that failures/skips are captured as structured outcomes without terminating the entire run unless configured.

**Internal Subcomponents**

1. **CaseBuilder**: expands scene/backend selections into cases with configuration IDs.
2. **CompatibilityEvaluator**: compares `required_features` vs `CapabilitySet`.
3. **Runner**: executes warm-up + measurement loops.
4. **StatisticsEngine**: computes p50 + dispersion + sample counts.

### 7.2.3 Module `pal` (Platform Abstraction Layer)

**Responsibilities**

1. Provide monotonic timing and CPU-time measurement.
2. Provide environment metadata acquisition (OS, arch, CPU model, memory).
3. Provide optional process utilities for isolation mode:

   * process spawn,
   * IPC pipes,
   * exit status and stderr capture.

**Ownership and Constraints**

* `pal` **MUST** be the only module that directly uses OS-specific APIs for timing and environment inspection.
* `pal` **MUST** document the semantics of CPU time (process vs thread) per platform build and expose it to metadata.

### 7.2.4 Module `ir`

**Responsibilities**

1. Parse canonical IR binary payloads.
2. Validate IR structure and invariants.
3. Produce `PreparedScene` optimized for replay.
4. Provide canonical utilities for:

   * color premultiplication checks/conversion helpers (where used),
   * transform composition math (to support shared semantics verification).

**Ownership and Constraints**

* `ir` **MUST** be backend-agnostic and contain no backend-specific code.
* `ir` **MUST** reject unsupported major IR versions.
* `PreparedScene` **MUST** be immutable and safe for repeated iteration.

**Internal Subcomponents**

1. **IrReader**: binary parsing and bounds checks.
2. **IrValidator**: structural validation (nesting, indices, value ranges).
3. **ScenePreparer**: converts raw sections to replay-ready views (e.g., pointers/offset tables).

### 7.2.5 Module `adapters`

**Responsibilities**

1. Provide a common adapter interface (contract from Chapter 6).
2. Provide a catalog/registry of compiled-in adapters and their availability.
3. Implement one adapter per backend library:

   * AmanithVG SRE
   * Blend2D
   * Skia (CPU Raster)
   * ThorVG SW
   * vello_cpu
   * Raqote
   * PlutoVG
   * Qt Raster Engine
   * AGG
   * Cairo (Image Surface)

**Ownership and Constraints**

* Each adapter submodule **MUST**:

  1. Enforce CPU-only surfaces/paths.
  2. Implement IR-to-backend mapping consistent with the IR semantic contract.
  3. Expose capabilities and backend metadata.
* Adapters **MUST NOT** perform statistics aggregation or report writing.
* Adapters **SHOULD** avoid allocations during replay; unavoidable allocations **MUST** be documented and optionally measured.

**Build-Time Enablement**

* Each adapter **MUST** be build-toggleable via CMake option (e.g., `ENABLE_SKIA=ON/OFF`), and runtime metadata **MUST** report which adapters were compiled in.

### 7.2.6 Module `reporting`

**Responsibilities**

1. Define result schema objects and schema versions.
2. Serialize `RunResult` to JSON and/or CSV.
3. Generate human-readable summary output.
4. Emit dependency pin records and build/environment metadata.

**Ownership and Constraints**

* `reporting` **MUST** not influence benchmark execution or timing.
* Writers **MUST** produce stable output ordering (deterministic) to support diffing and regression tracking.

**Internal Subcomponents**

1. **Schema**: typed representation of output fields and version.
2. **JsonWriter / CsvWriter**: machine-readable output writers.
3. **SummaryWriter**: text/markdown summary generator.

### 7.2.7 Module `assets`

**Responsibilities**

1. Provide access to:

   * Scene manifest,
   * IR assets,
   * Schema definition files (if shipped as artifacts).
2. Validate manifest correctness (paths, hashes, versions).

**Ownership and Constraints**

* `assets` **MUST** not parse IR (delegates to `ir`), and **MUST** not perform rendering.

## 7.3 Data Ownership and Lifecycle

1. `assets` owns scene catalog loading and manifest validation results.
2. `ir` owns `PreparedScene` construction; `PreparedScene` is owned by `harness` for the duration of a run.
3. `adapters` own backend-specific objects and surface handles; surfaces are created/destroyed under `harness` control.
4. `harness` owns measurement samples and computed statistics, producing an immutable `RunResult` handed to `reporting`.
5. `reporting` owns output file generation; outputs are written after measurement completes for each case or at end-of-run according to policy.

## 7.4 Configuration Surfaces

1. Runtime configuration **MUST** be injectable via `ExecutionPlan` derived from CLI and optional config files.
2. Build-time configuration **MUST** be managed via CMake options controlling:

   * which adapters are compiled,
   * optional features (e.g., process isolation mode support, allocation/RSS measurement support).
3. All configurations affecting results **MUST** be captured in run metadata.

## 7.5 Extension Procedures (Implementation-Ready)

### 7.5.1 Adding a New Backend Adapter

To add a backend, implementer **MUST**:

1. Create a new adapter submodule under `adapters/<backend_id>/`.
2. Implement the adapter lifecycle and render mapping contract.
3. Define capability flags and CPU-only enforcement metadata.
4. Add CMake options and dependency pinning hooks.
5. Register the adapter in the adapter registry/catalog.
6. Add at least one smoke-test scene execution in CI (Appendix B/E references).

### 7.5.2 Adding a New Scene

To add a scene, implementer **MUST**:

1. Create a canonical IR asset and place it under the `assets` scene directory.
2. Compute and record the SceneHash in the manifest.
3. Declare required features accurately.
4. Add tags and notes as needed (non-normative).
5. Ensure the scene does not require backend-specific code changes.

## 7.6 Component-Level Error Handling

1. Modules **MUST** return structured errors/status codes upward (Chapter 6).
2. `harness` **MUST** translate adapter/IR errors into case outcomes with stable reason codes.
3. `cli` **MUST** map run-level failure states to exit codes without losing structured diagnostics.

## Acceptance Criteria

1. The module list is complete and maps directly to architectural components from Chapter 4.
2. Each module has clear responsibilities, constraints, and interfaces consumed/provided.
3. Data ownership and lifecycle across modules is explicitly defined and avoids circular dependencies.
4. Build-time adapter enablement and runtime reporting of compiled adapters is specified.
5. Extension procedures for adding a backend and adding a scene are stepwise and do not require cross-cutting changes.

## Dependencies

1. Chapter 4 — System Architecture Overview (component decomposition).
2. Chapter 5 — Data Design (manifest, IR assets, PreparedScene).
3. Chapter 6 — Interfaces (CLI, adapters, IR runtime, reporting contracts).

---

