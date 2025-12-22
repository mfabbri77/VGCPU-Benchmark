# Chapter 3 — Constraints, Assumptions, and Stakeholders

## Purpose

This chapter enumerates the binding constraints that shape architecture and implementation, records project-level assumptions that guide design choices, and identifies stakeholders and their responsibilities. The intent is to make the benchmark suite’s boundaries and decision drivers explicit and auditable.

## 3.1 Constraints

### 3.1.1 Platform and Delivery Constraints

1. The benchmark suite **MUST** be a cross-platform **console-only** application supporting Linux, Windows, and macOS on x86_64 and ARM64.
2. The project **MUST** use **CMake** as the single canonical build entry point for configuring, building, and orchestrating dependencies.
3. The project **MUST** support both:

   * **Source distribution** (buildable from a tagged revision), and
   * **Binary releases** published via GitHub Releases where feasible for the supported platforms/architectures.
4. Release artifacts **MUST** include sufficient metadata to identify:

   * Benchmark suite git revision/tag,
   * Compiler/toolchain versions,
   * Backend library versions (or commit hashes) used to produce the artifact.

### 3.1.2 Benchmark Scope Constraints

1. The suite **MUST** remain within the in-scope vector feature set defined in Chapter 1.
2. The suite **MUST NOT** include GPU execution in CPU-only benchmark results; GPU-accelerated backends or configurations **MUST** be excluded from CPU-only reporting.
3. The suite **MUST NOT** include bitmap/image drawing, filter effects, or text layout/shaping pipelines.
4. The suite **MUST** treat scene authoring and import tooling (e.g., SVG import) as out of scope; scenes are handcrafted in the IR or a minimal representation used solely to generate IR.

### 3.1.3 Cross-Backend Fairness Constraints

1. The suite **MUST** define and enforce a strict semantic contract for IR operations to reduce ambiguity across backends.
2. The suite **MUST** use a canonical pixel format for outputs (default: premultiplied RGBA8) and **MUST** document any unavoidable deviations per backend.
3. The suite **MUST** implement a capability matrix and **MUST** not silently degrade semantics when a backend lacks a feature; unsupported combinations **MUST** be skipped by default or run only under an explicit fallback mode.

### 3.1.4 Reproducibility and Dependency Constraints

1. Dependency versions **MUST** be pinned and recorded for reproducibility, including when using system-installed dependencies.
2. The suite **MUST** record execution environment metadata sufficient to reproduce results and explain variability (OS, CPU, toolchain, backend versions, benchmark policy).
3. The harness **MUST** measure both CPU time and wall time using OS-specific APIs behind an abstraction layer.

## 3.2 Assumptions

### 3.2.1 Technical Assumptions

1. It is assumed that each target backend can be configured to render to a CPU-backed surface without requiring a windowing system for headless operation (or that a headless configuration is available).
2. It is assumed that each backend provides stable enough APIs to implement an adapter without extensive patching of upstream code.
3. It is assumed that cross-platform timing APIs provide adequate resolution and stability to support meaningful comparisons within a platform when best practices are followed.
4. It is assumed that scenes can be authored deterministically such that their IR payload is stable across time, and that any modifications to scenes are intentional and versioned.

### 3.2.2 Operational Assumptions

1. It is assumed that benchmark users will run the suite on relatively idle systems and will accept documented guidance (e.g., disabling power-saving features) to reduce noise.
2. It is assumed that binary releases may not cover every OS/architecture combination initially if certain backends are unavailable or impractical to package; in such cases source builds remain authoritative.

### 3.2.3 Legal and Licensing Assumptions

1. It is assumed that each backend library’s license permits redistribution in source form and, where applicable, in binary releases that include the backend, subject to compliance obligations documented later.
2. It is assumed that benchmark scenes do not embed copyrighted assets requiring restrictive distribution terms (since raster images and fonts are out of scope).

## 3.3 Stakeholders and Responsibilities

### 3.3.1 Stakeholder Groups

1. **Benchmark Users (Developers, Performance Engineers)**

   * Need: reproducible, transparent CPU-only performance results and clear configuration controls.
2. **Backend Maintainers / Contributors**

   * Need: a clear adapter contract, capability matrix, and diagnostics to validate mappings and CPU-only enforcement.
3. **Project Maintainers (Core Team)**

   * Need: maintainable architecture, consistent IR semantics, release automation, and a clear policy for accepting new scenes/backends.
4. **CI/Release Operators**

   * Need: deterministic builds, pinned dependencies, artifact generation, checksums, and repeatable packaging for GitHub Releases.
5. **Technical Stakeholders / Decision Makers**

   * Need: readable summary reports, comparable statistics, and environment metadata supporting interpretation.

### 3.3.2 Responsibility Requirements

1. The project **MUST** define ownership for:

   * IR specification changes,
   * Scene suite changes,
   * Backend adapter maintenance,
   * Release processes (source and binaries).
2. Changes to IR semantics **MUST** be versioned and reviewed because they affect comparability across time and backends.
3. Changes to scenes **MUST** increment scene version identifiers and update scene hashes recorded in benchmark metadata.

## 3.4 Constraints-to-Design Implications (Non-Exhaustive)

1. Because GPU execution is excluded, adapters **MUST** explicitly select CPU surfaces and record enforcement metadata; architecture must include a “CPU-only enforcement” layer per backend.
2. Because backends have non-uniform APIs and capabilities, architecture must include:

   * A strict IR semantic contract, and
   * A capability matrix and enforcement mechanism.
3. Because both source and binary releases are required, build and packaging design must support:

   * Optional inclusion/exclusion of certain backends per platform,
   * Clear reporting of what is present in each build artifact,
   * Dependency pinning and metadata capture.

## Acceptance Criteria

1. The constraints section includes explicit, testable constraints covering platform targets, build system, delivery channels, benchmark scope, CPU-only requirements, reproducibility, and dependency pinning.
2. Assumptions are clearly separated from constraints and are written such that they can be validated or revised without altering scope.
3. Stakeholders are identified with clear needs, and responsibilities are specified for IR/scene/backend/release ownership.
4. At least three concrete design implications are stated that trace back to constraints (CPU-only, capability differences, reproducibility, release packaging).

## Dependencies

1. Chapter 1 — Introduction and Scope (scope boundaries, backends list).
2. Chapter 2 — Requirements (functional and non-functional obligations informing constraints and stakeholder needs).

---


