# Chapter 15 — Glossary

## Purpose

This chapter defines the canonical terminology used throughout the Blueprint. Terms in this glossary are normative: other chapters’ requirements and interfaces MUST use these meanings to avoid ambiguity.

## 15.1 Core Terms

1. **Adapter**
   A backend-specific integration module that maps IR operations to a rendering library’s API, enforces CPU-only operation, and exposes backend capabilities and metadata.

2. **Backend**
   A specific 2D vector rendering library and configuration used by the suite (e.g., Skia raster, Cairo image surface). A backend is identified by a stable **BackendId**.

3. **BackendId**
   A stable identifier string used to select and report a backend (e.g., `cairo_image`). BackendId MUST be unique within the suite.

4. **BackendMetadata**
   Structured information emitted by an adapter describing backend version/build information, CPU-only enforcement settings, surface types, and other configuration needed for reproducibility.

5. **Benchmark Case**
   A single measurement unit identified by the tuple (SceneId, BackendId, ConfigurationId), with a compatibility decision and an execution outcome.

6. **Benchmark Harness (Harness Core)**
   The component that plans benchmark cases, performs warm-up and measurement, collects timing samples, computes statistics, and produces results for reporting.

7. **Capability Matrix**
   A structured representation of feature support across backends (e.g., fill rules, stroke caps/joins, gradients, dashes, clipping). Used to determine whether a backend can execute a scene.

8. **CapabilitySet**
   The per-backend structured declaration of supported features, consumed by the compatibility engine.

9. **Compatibility Decision**
   The policy output indicating whether a benchmark case should `EXECUTE`, `SKIP`, `FAIL`, or `FALLBACK`, including stable reason codes.

10. **ConfigurationId**
    A stable identifier representing a benchmark configuration (e.g., resolution override, thread policy). ConfigurationId MUST allow distinguishing cases that differ in relevant parameters.

11. **CPU-only**
    Rendering that executes without GPU usage and targets a CPU-resident render surface/buffer. “CPU-only” includes explicit disablement/avoidance of GPU acceleration paths where applicable.

12. **CPU Time**
    The amount of CPU processing time consumed (process or thread CPU time) as measured by platform APIs. The suite MUST record which semantic is used on each platform build.

13. **Deterministic Replay**
    Replaying the same IR payload in the same command order with the same configuration to produce consistent semantic operations. Determinism does not imply pixel-perfect equality across backends.

14. **Dispersion Metric**
    A statistic describing variability (e.g., p90, MAD, IQR) included alongside median/p50.

15. **Fill Rule**
    The rule for determining the inside of a path for fill operations (Non-Zero or Even-Odd), as defined by the IR semantic contract and mapped by adapters.

16. **Intermediate Representation (IR)**
    A backend-neutral, versioned scene description format used for deterministic replay across backends. The canonical IR is a binary format per Chapter 5.

17. **IR Runtime**
    The component that loads, validates, and prepares IR assets into an immutable PreparedScene for efficient replay.

18. **IR Version**
    A semantic and binary-compatibility identifier for IR payloads, expressed as MAJOR.MINOR.PATCH. Major changes are breaking.

19. **Manifest (Scene Manifest)**
    A machine-readable catalog of scenes, including SceneIds, IR asset paths, scene hashes, default settings, and required features.

20. **Monotonic Clock**
    A clock source that cannot go backwards, used for stable wall-time measurement.

21. **Outcome**
    The final state of a benchmark case: `SUCCESS`, `SKIP`, or `FAIL`, with stable reason codes.

22. **PAL (Platform Abstraction Layer)**
    The module that encapsulates OS-specific APIs for timing, environment metadata, and optional process management.

23. **PreparedScene**
    The immutable, replay-ready in-memory representation produced by the IR runtime from an IR payload.

24. **Premultiplied Alpha**
    A color representation where RGB components are multiplied by alpha. The suite’s canonical pixel format uses premultiplied RGBA8 unless a documented deviation is required.

25. **Reason Code**
    A stable machine-readable identifier explaining why a case was skipped or failed (e.g., `UNSUPPORTED_FEATURE:dashes`, `BACKEND_INIT_FAILED`).

26. **Repetition**
    A repeated execution of the same benchmark case intended to reduce noise and enable statistical aggregation across independent runs.

27. **Result Schema**
    The versioned machine-readable structure (JSON/CSV) describing benchmark results and metadata.

28. **Scene**
    A deterministic workload defined by an IR command stream plus fixed output dimensions and render settings, identified by a stable **SceneId**.

29. **SceneHash**
    A cryptographic hash (SHA-256) computed over the canonical IR payload bytes, used to identify the exact scene content measured.

30. **SceneId**
    A stable identifier string used to select and report a scene (e.g., `fills/linear_gradient_basic`). SceneId MUST be unique within the suite.

31. **Semantic Contract**
    The explicit definition of how IR operations must behave (fill rules, stroke behavior, gradients, transforms, compositing). Adapters MUST conform or declare deviations.

32. **Source-Over**
    The standard alpha compositing operation where the source is drawn over the destination.

33. **Surface (Render Surface / Target Surface)**
    A CPU-resident pixel buffer or backend surface onto which a scene is rendered. Its pixel format and ownership are defined by the adapter contract.

34. **Timed Section**
    The portion of execution measured for benchmark timings, which MUST include only rendering (and minimal measurement overhead), excluding parsing, I/O, and reporting.

35. **Wall Time**
    Elapsed real time measured using a monotonic clock, used as a primary performance metric alongside CPU time.

## 15.2 Acronyms

1. **IR** — Intermediate Representation
2. **PAL** — Platform Abstraction Layer
3. **RSS** — Resident Set Size (process memory usage metric)
4. **CLI** — Command Line Interface

## Acceptance Criteria

1. All terms used as identifiers in prior chapters (IR, PAL, Adapter, Backend, Scene, CapabilitySet, Outcome, Reason Code, Timed Section) are defined here.
2. Definitions are consistent with prior chapters and suitable for use in interface and schema documentation.
3. The glossary can be used to resolve ambiguous terms without referencing external sources.

## Dependencies

None.

---

