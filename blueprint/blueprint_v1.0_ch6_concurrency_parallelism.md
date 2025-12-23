# Chapter 6 — Concurrency, Parallelism, Determinism (CPU)

## [CONC-01] Concurrency goals
- [CONC-01-01] Default behavior is **single-threaded benchmarking** (threads=1) to maximize comparability and reduce noise (aligns with [DEC-ARCH-10], [DEC-API-04]).
- [CONC-01-02] Optional multi-thread mode exists to benchmark **scaling characteristics**, but must remain deterministic in:
  - work partitioning,
  - sample placement/ordering,
  - report ordering.
- [CONC-01-03] No data races; ThreadSanitizer (TSan) must be supported on at least one Tier-1 CI platform.

## [CONC-02] Threading model (authoritative)
### [REQ-75] Harness execution modes
- [REQ-75-01] Mode A (default): `threads = 1`
  - One adapter instance, one output buffer, one sample vector per scene.
- [REQ-75-02] Mode B (optional): `threads = N (N>1)` — **per-scene parallel reps**
  - For each scene:
    - create **N adapter instances** (one per worker),
    - create **N output buffers** (one per worker),
    - partition `reps` into N deterministic slices,
    - run workers in parallel, each writing samples into its assigned slice.

- [DEC-CONC-01] Multi-thread mode parallelizes **repetitions of a single scene**, not different scenes.
  - Rationale: preserves scene ordering and keeps caching effects consistent within a scene.
  - Alternatives: (A) run different scenes in parallel; (B) parallelize IR loading/prepare only.
  - Consequences: peak memory rises by `threads * output_buffer_size` and `threads * adapter resources`.

### [REQ-76] Scene ordering and determinism
- [REQ-76-01] Scenes MUST execute in stable order (sorted SceneId) regardless of thread count (supports [REQ-29], [REQ-20]).
- [REQ-76-02] For `threads>1`, repetition partitioning MUST be deterministic:
  - Worker `i` gets `[start_i, end_i)` computed by integer division, stable for a given (reps, threads).
- [REQ-76-03] Sample vector positions MUST be deterministic:
  - Each worker writes only to its own sample slice (no push_back).

## [CONC-03] Work partitioning (binding algorithm)
Let:
- `R = reps`, `T = threads`, `i in [0..T-1]`.

### [REQ-77] Slice computation
- `start_i = floor(R * i / T)`
- `end_i   = floor(R * (i+1) / T)`
- Worker i executes `end_i - start_i` iterations and writes samples to `samples[start_i .. end_i-1]`.

- [DEC-CONC-02] If `R < T`, workers with empty slices do not start adapters (avoid overhead).
  - Rationale: avoid unnecessary adapter init and thread wakes.
  - Alternatives: still start all workers.
  - Consequences: thread pool still exists, but can skip tasks.

## [CONC-04] Synchronization strategy
### [REQ-78] Synchronization rules (hot-path safe)
- [REQ-78-01] No mutex locks inside the measured loop.
- [REQ-78-02] No atomic increments inside the measured loop (except optional debug-only counters).
- [REQ-78-03] Worker threads must have exclusive access to:
  - their adapter instance,
  - their output buffer,
  - their sample slice.

### [DEC-CONC-03] Start alignment barrier
- Use a barrier to align worker start for each scene:
  - workers call `barrier.arrive_and_wait()` once before measurement begins.
- Rationale: reduces start skew in multi-thread mode.
- Alternatives: no barrier.
- Consequences: small overhead outside measured loop only.

**Implementation requirement**
- [REQ-79] Use `std::barrier` (C++20) where available; if platform toolchain lacks full support, provide a small fallback barrier (condition_variable-based) used only outside measured loops.

## [CONC-05] Thread pool and lifecycle
### [REQ-80] Worker lifecycle
- [REQ-80-01] For a run with `threads>1`, the harness MUST create a worker pool once and reuse it across scenes.
- [REQ-80-02] Thread pool teardown occurs after all scenes are complete (or on fatal error).

### [DEC-CONC-04] Threading primitives
- Use `std::jthread` for worker threads (RAII stop tokens) in C++20.
- Use a bounded task queue per pool; tasks are scene-specific “execute my slice” jobs.
- Rationale: clean lifecycle + safe shutdown.
- Alternatives: raw `std::thread` + manual join.
- Consequences: pool code must stay portable across MSVC/AppleClang/Clang.

### [REQ-81] Stop/cancel policy
- [REQ-81-01] On fatal error (adapter init failure, timer failure, manifest/IR failure), all workers must be signaled to stop and join before returning to CLI.
- [REQ-81-02] In `--continue-on-error` mode, per-scene errors do not stop the pool; only that scene’s execution is marked failed.

## [CONC-06] Adapter concurrency rules
### [REQ-82] Adapter instance isolation
- [REQ-82-01] Each worker must have its own `IBackendAdapter` instance (no sharing) unless the adapter explicitly declares itself thread-safe and reentrant (future extension).
- [REQ-82-02] If a backend library uses global state, its adapter MUST serialize internally and declare itself “single-thread-only”; harness will refuse `threads>1` for that backend with a stable error code.

### [DEC-CONC-05] Capability flag for multithreading
- Extend `BackendDescriptor` with a capability bit:
  - `supports_parallel_render` (default false).
- Harness checks it when `threads>1`.
- Rationale: prevents accidental unsafe parallel runs.
- Alternatives: try-and-pray.
- Consequences: registry must expose this flag; CLI reports it in `list`.

## [CONC-07] Data sharing and publication (race avoidance)
### [REQ-83] Immutable shared data
- [REQ-83-01] Manifest, `SceneMeta`, `SceneIR`, and `PreparedScene` must be immutable after creation (restate [REQ-66], [DEC-ARCH-07]).
- [REQ-83-02] Shared objects must be fully constructed before publication to workers:
  - publish via passing `const&` into tasks after construction,
  - no lazy init without synchronization.

### [DEC-CONC-06] No shared caches in v0.2.0
- Do not introduce global caches (glyph caches, path caches) inside VGCPU layers in v0.2.0.
- Rationale: reduces race risk and benchmark skew.
- Alternatives: caching for speed.
- Consequences: performance may be lower for some workloads; caching can be added later behind strict sync rules + CR.

## [CONC-08] Determinism policy (practical, enforceable)
### [REQ-84] Deterministic output ordering
- [REQ-84-01] Report ordering (scenes, fields, columns) MUST not depend on thread scheduling.
- [REQ-84-02] Sample storage ordering is deterministic by [REQ-77]; stats computed deterministically after a single post-measure sort (Ch5 [REQ-63]).

### [REQ-85] Run-id and seeds
- [REQ-85-01] `run_id` must be unique per run and generated outside worker threads.
- [REQ-85-02] If any randomness exists (now or future), it must be seeded from `RunConfig.seed` and deterministically partitioned per worker:
  - `worker_seed = hash(seed, scene_id, worker_index)`.

## [CONC-09] Affinity / pinning (best-effort)
### [REQ-86] Affinity knobs
- [REQ-86-01] CLI exposes `--pin` (best-effort) to request CPU affinity pinning for worker threads.
- [REQ-86-02] If pinning is unsupported or fails, the run must continue but log a structured warning (supports [REQ-02]).
- [DEC-CONC-07] Pinning implementation:
  - Windows: `SetThreadAffinityMask`
  - Linux: `pthread_setaffinity_np`
  - macOS: do not hard-pin by default (limited support); provide “best effort” QoS only if needed.
  - Rationale: keep portability and avoid fragile behavior.
  - Alternatives: omit pinning entirely.
  - Consequences: pinning is not guaranteed; report must record whether pinning succeeded.

## [CONC-10] Stress, sanitizer, and concurrency tests
These tests ensure concurrency contracts are enforced and race-free.

- [TEST-31] `threads_partition_deterministic`:
  - for given (reps, threads) verify slice boundaries exactly match [REQ-77].
- [TEST-32] `multithread_samples_written_once`:
  - run `null` backend with `threads=4`, `reps=1000`; verify every sample index is written exactly once (can initialize samples with sentinel and assert none remain).
- [TEST-33] `multithread_report_stable_order`:
  - run twice with `threads=4`; ensure scene ordering and CSV header order are identical.
- [TEST-34] `backend_refuses_parallel_if_unsupported`:
  - pick a backend flagged `supports_parallel_render=false`; request `threads=2`; verify stable error code.
- [TEST-35] `tsan_smoke` (Linux/Clang only):
  - run unit test suite under TSan for Tier-1 null backend paths; no data races.

## [CONC-11] Tooling requirements (forward references)
- [REQ-87] CI must include at least one concurrency gate:
  - TSan job on Linux/Clang, OR
  - stress test loop job with `threads>1` for null backend.
(Concrete CI wiring is defined in Ch7.)

## [CONC-12] Traceability notes
- [CONC-12-01] This chapter defines [REQ-75..87] and [TEST-31..35].
- [CONC-12-02] Ch7 specifies build presets to enable TSan and to run concurrency tests in CI.
- [CONC-12-03] Ch9 defines how perf regression gates treat multi-thread mode (separate benchmark dimension; not compared to single-thread baselines).
