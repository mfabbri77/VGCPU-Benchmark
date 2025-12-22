<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Chapter 1 — Introduction and Scope

## Purpose

This chapter defines the purpose, boundaries, and intended outcomes of the CPU-only 2D vector graphics rendering benchmark suite. It establishes the scope of vector features, target platforms/backends, and deliverables at a level sufficient to guide the architecture and specification work in subsequent chapters.

## 1.1 System Purpose

1. The benchmark suite **MUST** provide a reproducible method to measure and compare **CPU-only** 2D vector rendering performance across multiple rendering libraries (“backends”) with non-uniform APIs.
2. The suite **MUST** define a single backend-neutral **Intermediate Representation (IR)** for scene description, enabling deterministic replay on all supported backends.
3. The suite **MUST** provide a measurement harness that yields statistically meaningful performance results and records the execution environment necessary for reproducibility.

## 1.2 Supported Platforms and Execution Model

### 1.2.1 Platform Targets

1. The benchmark suite **MUST** be a **console-only** application.
2. The benchmark suite **MUST** support the following operating systems as first-class targets:

   * Linux
   * Windows
   * macOS
3. The benchmark suite **MUST** support the following CPU architectures as first-class targets:

   * x86_64
   * ARM64

### 1.2.2 Timing and Measurement Model (Scope-Level)

1. The harness **MUST** measure both:

   * **Wall-clock time**, and
   * **CPU time** (process or thread CPU time, as feasible per platform).
2. Platform-specific timing APIs **MAY** be used, but **MUST** be accessed via a platform abstraction layer to preserve cross-platform behavior and comparability.

## 1.3 In-Scope Functionality

### 1.3.1 Vector Rendering Feature Scope

The suite **MUST** support only vector features that can be mapped consistently across all target backends, specifically:

1. **Path geometry commands**: move-to, line-to, quadratic Bézier, cubic Bézier, close-path.
2. **Filling**:

   * Solid color fills.
   * Linear gradient fills.
   * Radial gradient fills.
   * Fill rules: Non-Zero and Even-Odd where supported by the backend.
3. **Stroking**:

   * Stroke width.
   * Cap styles: butt, round, square where supported.
   * Join styles: miter, round, bevel where supported.
   * Miter limit.
   * Dash pattern and dash offset where supported.
4. **Transforms**:

   * 2D affine transforms (matrix concatenation).
   * A graphics state stack (save/restore) sufficient to express hierarchical transforms.
5. **Compositing**:

   * At minimum, Source-Over alpha compositing.

### 1.3.2 Workload and Scene Scope

1. A **Scene** **MUST** be a deterministic sequence of IR commands with fixed output dimensions and render settings.
2. The suite **MUST** include a curated set of reference scenes that exercise fills, strokes, gradients, transforms, and optional clipping (only where supported and where semantic contract can be defined).
3. Adding a scene **MUST** not require modifying backend adapters or harness logic; it **MUST** be possible via IR assets alone.

## 1.4 Out-of-Scope Functionality

The suite **MUST NOT** include:

1. Bitmap/image drawing, including image patterns and raster image compositing.
2. Filter and effect pipelines (e.g., blur, drop shadow, color matrices).
3. Text shaping or font/layout pipelines.

   * If text is included in any scene, it **MUST** be represented as precomputed vector outlines (paths) within the IR.
4. GPU execution for benchmarked runs.

   * Backends that require GPU rendering for their available configuration are excluded from CPU-only results.
5. Scene authoring/import tooling (e.g., SVG importers, GUI editors, converters).

   * Scenes **MUST** be handcrafted directly in the IR (and/or a minimal authoring representation defined solely to generate IR, if later introduced).

## 1.5 Target Backends

### 1.5.1 Backends in Scope

The suite **MUST** provide adapters for the following backends, each configured for CPU-only operation:

1. AmanithVG SRE
2. Blend2D
3. Skia
4. ThorVG
5. Vello CPU
6. Raqote
7. PlutoVG
8. Qt QPainter
9. Anti-Grain Geometry (AGG)
10. Cairo

### 1.5.2 CPU-Only Guarantee Requirements

For every backend adapter, the suite **MUST**:

1. Select a CPU-backed surface/target (e.g., raster/image surfaces in system memory).
2. Disable, avoid, or bypass GPU acceleration paths where a library offers optional GPU execution.
3. Record backend configuration, including compile-time flags and runtime initialization decisions, in benchmark output metadata.

## 1.6 Deliverables and Distribution

### 1.6.1 Required Deliverables

The project **MUST** produce:

1. **Benchmark Harness**

   * A CLI executable to select backends/scenes, configure iterations/warm-up, and emit results.
2. **IR Specification**

   * A formal, versioned specification of the scene IR including semantic contracts for operations.
3. **Backend Adapters**

   * One adapter per backend implementing a common interface and CPU-only operation requirements.
4. **Scene Suite**

   * A curated collection of deterministic scenes/workloads.
5. **Reporting**

   * Machine-readable outputs (JSON/CSV) and human-readable summaries.
6. **Reproducibility Kit**

   * Scripts and documentation for dependency pinning, build instructions, and environment capture.

### 1.6.2 Distribution Requirements

1. The project **MUST** support **source distribution** sufficient to rebuild all deliverables on supported platforms.
2. The project **MUST** support **binary releases** published via GitHub Releases for supported platforms/architectures where feasible.
3. The reproducibility kit **MUST** document the exact versions (or commit hashes) of dependencies used for each release artifact.

## 1.7 Success Criteria (High-Level)

1. The suite **MUST** produce comparable performance metrics for each (scene × backend × configuration) tuple, including median and dispersion statistics.
2. The suite **MUST** enable deterministic replay of the same scene IR across all backends that claim support for the required feature subset.
3. Adding a backend **MUST** be possible by implementing only the adapter interface plus any backend-local initialization/configuration.
4. Adding a scene **MUST** be achievable by adding IR assets without modifying backend adapter source code.

## 1.8 Document Conventions

1. This Blueprint uses RFC-style normative keywords: **MUST**, **SHOULD**, **MAY**, **MUST NOT**, **SHOULD NOT**.
2. The IR semantic contract defined later **MUST** be treated as the source of truth for cross-backend behavior.
3. Where a backend cannot support a feature, the suite **MUST** reflect this via a capability matrix and enforce appropriate feature subsets.
4. “Pinned and recorded dependencies” **MUST** mean that versions (and where applicable, commit hashes) are captured in build metadata and included in benchmark outputs and/or release notes.

## Acceptance Criteria

1. A reader can identify, from this chapter alone:

   * The benchmark’s purpose (CPU-only vector rendering comparison).
   * The exact list of in-scope vector features.
   * The explicit out-of-scope features, including scene authoring tools.
   * The platform targets (Linux/Windows/macOS; x86_64/ARM64) and console-only constraint.
   * The target backend list and CPU-only guarantee requirements.
   * The deliverables, distribution model (source + GitHub Releases), and high-level success criteria.
2. The scope statements are unambiguous and use normative terms (MUST/MUST NOT/SHOULD/MAY).
3. The platform and distribution constraints introduced by the stakeholder are reflected explicitly and consistently.

## Dependencies

None.

---


