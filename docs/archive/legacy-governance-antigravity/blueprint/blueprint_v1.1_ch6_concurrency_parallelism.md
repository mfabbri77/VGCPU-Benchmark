# Chapter 6 — Concurrency, Parallelism, Determinism (v1.1)

This project is primarily single-process and currently executes benchmark cases sequentially by default. v1.1 introduces additional post-benchmark work (PNG + SSIM) that must remain deterministic and must not contaminate measured timings.

## [CONC-01] Concurrency model (baseline)
- [CONC-01-01] Default execution is **single-threaded orchestration**: iterate scenes, then backends, performing warmup + measured + post-benchmark artifact phase.
- [CONC-01-02] Backends may internally create threads (implementation-defined); the harness treats each backend as a black box.

## [CONC-02] Determinism guarantees (what we promise)
- [CONC-02-01] Within a single run on a single machine, the harness MUST:
  - [CONC-02-01a] use a fixed scene ordering,
  - [CONC-02-01b] use a fixed backend ordering (with explicit ground-truth-first ordering when SSIM enabled),
  - [CONC-02-01c] separate timed and untimed phases deterministically ([DEC-ARCH-11]).
- [CONC-02-02] Cross-machine and cross-OS bitwise determinism is **not** promised; correctness regression uses SSIM with configurable threshold ([REQ-154]).
- [CONC-02-03] When SSIM is enabled, the ground truth image MUST be generated first for each scene, then reused for comparisons for that scene ([ARCH-04-02]).

## [CONC-03] Artifact pipeline threading rules (v1.1)
- [CONC-03-01] PNG writing MUST be performed **outside** measured loops and may involve file I/O; it MUST NOT run concurrently with the measured loop for the same case.
- [CONC-03-02] Artifact writes MUST be serialized per output path:
  - [CONC-03-02a] For a given run, each `(scene, backend)` produces exactly one PNG path per [DEC-SCOPE-05].
  - [CONC-03-02b] If future parallel execution is added, the system MUST guarantee no two threads write to the same PNG path concurrently.
- [DEC-CONC-01] v1.1 implementation MUST serialize artifact I/O on a dedicated mutex (or execute artifacts on the same orchestrator thread).
  - Rationale: prevents races and partial files; keeps behavior deterministic.
  - Alternatives: per-scene lock; lock-free unique file strategy.
  - Consequences: artifact pipeline remains a small sequential tail; this is acceptable since artifacts are optional.

## [CONC-04] Future-proofing: optional parallel benchmarking (deferred)
- [DEC-CONC-02] Parallelizing scenes/backends is **deferred**; if added later it requires a dedicated CR including:
  - [DEC-CONC-02a] per-backend instance safety analysis,
  - [DEC-CONC-02b] deterministic scheduling policy (stable ordering, bounded worker pool),
  - [DEC-CONC-02c] artifact output collision guarantees (see [CONC-03-02]).
  - Rationale: many backends are not reentrant; parallel runs can distort cache/CPU frequency and harm benchmark validity.
  - Consequences: current v1.1 stays simple and reproducible.

## [CONC-05] Data race safety requirements
- [CONC-05-01] Artifact module functions MUST be reentrant and thread-safe (no mutable globals) ([API-09-01]).
- [CONC-05-02] The harness owns all pixel buffers and MUST ensure spans passed into artifact functions remain valid for the duration of the call ([MEM-03]).
- [CONC-05-03] When SSIM enabled, `gt_pixels` MUST not be mutated after capture; it is treated as immutable scene-scope state.

## [CONC-06] Cancellation and timeouts (baseline)
- [DEC-CONC-03] v1.1 does not add cancellation/timeouts; long runs are user-controlled via reps/scene selection.
  - Rationale: keep harness simple; avoid OS-specific cancellation semantics.
  - Alternatives: SIGINT handling, per-scene timeout.
  - Consequences: future cancellation support requires a CR; must ensure partial outputs are still well-formed.

## [CONC-07] Synchronization details (implementation-ready)
- [CONC-07-01] Required synchronization primitives:
  - [CONC-07-01a] `std::mutex artifact_io_mutex;` guarding filesystem creates/writes (png dir creation + file writes).
- [CONC-07-02] Scope rules:
  - [CONC-07-02a] Acquire the artifact mutex only during:
    - directory creation (once),
    - writing the PNG file,
    - emitting artifact-related log/report updates that depend on file existence (optional).
  - [CONC-07-02b] Never hold artifact mutex while calling backend `Render()` (avoid accidental serialization of backend internals and risk deadlocks).
- [CONC-07-03] SSIM computation:
  - [CONC-07-03a] SSIM compute is pure CPU and may run without the artifact I/O mutex.
  - [CONC-07-03b] If SSIM library uses static state (must not), wrap with a dedicated compute mutex; otherwise disallowed by [API-09-01].

## [CONC-08] Tooling: sanitizer requirements
- [CONC-08-01] The CI MUST run ThreadSanitizer (TSan) on at least one Linux/Clang configuration for Tier-1 backends where feasible.
  - Note: Some third-party libs may be incompatible with TSan; allow a curated allowlist of disabled backends for the TSan job.
- [CONC-08-02] When SSIM/PNG features are enabled in tests, run them under TSan to validate no artifact races.

## [TEST-56] Concurrency & determinism tests (v1.1)
- [TEST-56-01] Deterministic ordering:
  - enable SSIM and assert (via logged structured events or harness event callbacks in tests) that ground-truth backend runs before any comparisons for each scene.
- [TEST-56-02] Artifact serialization:
  - in a test build that simulates parallel calls (or uses a mock that calls artifact writer concurrently), assert no data races and no corrupted output (file size > 0 and valid header).
- [TEST-56-03] Measured-loop purity:
  - with test-only instrumentation, assert artifact mutex is never acquired during measured iterations and PNG/SSIM counters remain zero until post-benchmark phase ([MEM-09-02]).
- [TEST-56-04] TSan smoke:
  - run a small scene subset with `--compare-ssim` under TSan (Tier-1 backends only) and ensure no reported data races.

## [DEC-CONC-04] Logging in concurrent contexts
- [DEC-CONC-04] Structured logging must be thread-safe; however, v1.1 logs for artifact writes and SSIM results are emitted only from the orchestrator thread by default.
  - Rationale: simplifies traceability; avoids interleaved logs.
  - Alternatives: asynchronous logging.
  - Consequences: if future parallelism is added, logging must include case identifiers (scene/backend) on every event.
