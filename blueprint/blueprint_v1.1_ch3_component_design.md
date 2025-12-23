# Chapter 3 — Component Design (Responsibilities, APIs, Dataflow, Failure Modes)

## [ARCH-11] Components overview (v1.1 delta)
v1.1 adds an internal “artifact pipeline” that runs **after** timed benchmarking:
- PNG artifact generation (`stb_image_write`).
- SSIM comparison against a per-scene ground truth backend (with fail threshold).

## [ARCH-12] Component responsibilities

### 3.1 CLI Frontend (`src/cli/*`)
- [ARCH-12-01] Parse args into `CliOptions`.
- [ARCH-12-02] Validate option combinations and fail fast with actionable messages:
  - [ARCH-12-02a] If `--compare-ssim` enabled, `--ground-truth-backend` must be set ([REQ-153]).
  - [ARCH-12-02b] If `--compare-ssim` enabled, `--ssim-threshold` must be within [0, 1] ([REQ-154]).
  - [ARCH-12-02c] If `--compare-ssim` enabled, the ground-truth backend must be available/enabled; else fail ([DEC-ARCH-12]).
- [ARCH-12-03] Instantiate Harness and Reporting sinks with validated config.

### 3.2 Adapter Registry + Backends (`src/adapters/*`)
- [ARCH-12-04] Provide:
  - [ARCH-12-04a] stable backend id strings (lowercase, filesystem-safe subset) for naming and CLI.
  - [ARCH-12-04b] an adapter interface capable of rendering a scene into a canonical `RGBA8 premultiplied` buffer.
- [ARCH-12-05] Enforce uniqueness of backend ids at registration time (needed for [DEC-SCOPE-05] naming collisions).

### 3.3 Harness (Benchmark Orchestrator) (`src/harness/*`)
- [ARCH-12-06] Own the outer loops:
  - [ARCH-12-06a] per-scene
  - [ARCH-12-06b] per-backend
  - [ARCH-12-06c] warmup + measured loops for timings ([REQ-04])
- [ARCH-12-07] Perform the **post-benchmark artifact phase** (untimed) per (scene, backend) when enabled ([DEC-ARCH-11]).
- [ARCH-12-08] When SSIM enabled:
  - [ARCH-12-08a] run ground truth backend first for each scene ([ARCH-04-02]).
  - [ARCH-12-08b] retain the ground truth raw buffer in memory for the duration of that scene’s comparisons.
  - [ARCH-12-08c] compute SSIM for each non-ground-truth backend and record metrics; fail if below threshold ([REQ-154]).

### 3.4 Artifact Pipeline (new internal module) (`src/artifacts/*` — v1.1)
- [ARCH-12-09] Convert raw render outputs into persisted artifacts:
  - [ARCH-12-09a] PNG writing to `<output_dir>/png/<scene>_<backend>.png` ([DEC-SCOPE-05], [REQ-148]).
- [ARCH-12-10] SSIM compare operations:
  - [ARCH-12-10a] validate dimension/format compatibility,
  - [ARCH-12-10b] compute SSIM on premultiplied RGBA buffers directly ([SCOPE-27 answer]),
  - [ARCH-12-10c] return per-channel and aggregated SSIM.
- [ARCH-12-11] Provide deterministic filename sanitization and path construction.

### 3.5 Reporting (`src/reporting/*`)
- [ARCH-12-12] Extend report schema to include:
  - [ARCH-12-12a] `png_path` for each (scene, backend) when PNG enabled.
  - [ARCH-12-12b] `ssim_vs_ground_truth` and optional per-channel SSIM when SSIM enabled.
  - [ARCH-12-12c] `ground_truth_backend` for traceability when SSIM enabled.
- [ARCH-12-13] On SSIM failure (below threshold), still write reports + artifacts for postmortem, then exit non-zero.

## [ARCH-13] Dependency DAG (v1.1)
```mermaid
flowchart LR
  CLI[CLI] --> HAR[Harness]
  HAR --> REG[Adapter Registry]
  REG --> ADP[Adapters/Backends]
  HAR --> ART[Artifact Pipeline]
  ART --> PNG[PNG Writer\n(stb_image_write)]
  ART --> SSIM[SSIM Compare\n(lightweight lib)]
  HAR --> REP[Reporting]
  ART --> REP
````

* [DEC-ARCH-13] SSIM implementation must be **lightweight, permissive-licensed, and vendorable** (single header or small TU), and pinned to an immutable revision in the build system.

  * Rationale: avoid heavyweight dependencies (e.g., OpenCV) and keep CI fast.
  * Alternatives: OpenCV; custom SSIM implementation; decode/compare PNGs.
  * Consequences: must add upstream notice text into `THIRD_PARTY_NOTICES.md` (Ch7).

## [ARCH-14] Dataflow and state (per scene)

```mermaid
sequenceDiagram
  participant H as Harness
  participant A as Adapter
  participant AR as Artifacts
  participant R as Reporting

  Note over H: For each scene
  alt SSIM enabled
    H->>A: Run benchmark for GroundTruth backend (warmup + measured)
    H->>A: Post-benchmark render (untimed)
    A-->>H: RGBA8 premul buffer (GT)
    H->>AR: Write PNG (scene_backend.png)
    AR-->>H: png_path
    Note over H: Retain GT buffer in memory

    loop each non-GT backend
      H->>A: Run benchmark (warmup + measured)
      H->>A: Post-benchmark render (untimed)
      A-->>H: RGBA8 premul buffer
      H->>AR: Write PNG
      AR-->>H: png_path
      H->>AR: Compute SSIM(buffer, GT)
      AR-->>H: ssim metrics
      H->>R: Record row (timings + png_path + ssim)
    end
  else SSIM disabled
    loop each backend
      H->>A: Run benchmark (warmup + measured)
      opt PNG enabled
        H->>A: Post-benchmark render (untimed)
        A-->>H: RGBA8 premul buffer
        H->>AR: Write PNG
        AR-->>H: png_path
      end
      H->>R: Record row (timings + optional png_path)
    end
  end
```

## [MEM-10] Data structures and ownership (implementation-ready)

### 3.6 Canonical image buffer

* [MEM-10-01] Canonical render output is:

  * `width` (uint32)
  * `height` (uint32)
  * `stride_bytes` (uint32, default `width * 4` unless adapter requires padding)
  * `format` fixed to `RGBA8_PREMULTIPLIED` (v1.1 contract)
  * `std::vector<std::uint8_t> pixels` sized `height * stride_bytes`
* [MEM-10-02] Ownership:

  * [MEM-10-02a] Adapter fills a caller-owned buffer (vector) for renders.
  * [MEM-10-02b] Harness owns the ground truth buffer copy for the duration of the scene comparisons.
  * [MEM-10-02c] Artifact pipeline uses `std::span<const uint8_t>` views; it does not own pixel storage.

### 3.7 Artifact metadata carried to reporting

* [MEM-10-03] For each (scene, backend) result row:

  * `std::string scene_name_original`
  * `std::string backend_id`
  * `std::string scene_name_sanitized`
  * `std::string png_path` (empty if PNG disabled)
  * `std::optional<double> ssim_agg` (empty if SSIM disabled or backend == ground truth)
  * `std::optional<std::array<double,4>> ssim_rgba` (optional feature; default record only aggregate)
  * `std::string ground_truth_backend` (empty if SSIM disabled)

### 3.8 Filename sanitization rule (deterministic)

* [MEM-10-04] Sanitization function:

  * allow `[a-zA-Z0-9._-]`, replace all other bytes with `_`
  * collapse multiple `_` into single `_`
  * trim leading/trailing `_`
  * limit component length to 120 chars; append stable hash suffix if truncated
* [MEM-10-05] Resulting artifact path:

  * `<output_dir>/png/<scene_sanitized>_<backend_id>.png` ([DEC-SCOPE-05])

## [ARCH-15] Artifact pipeline APIs (internal)

> Header signatures are sketched here; full API/ABI contracts are in Ch4.

* [API-20] `artifacts::WritePngRGBA8Premul(path, width, height, stride_bytes, pixels_span) -> Status`
* [API-21] `artifacts::ComputeSSIM_RGBA8Premul(width, height, stride_bytes, a_span, b_span) -> SsimResult`
* [API-22] `artifacts::MakePngPath(output_dir, scene_name, backend_id) -> std::filesystem::path`
* [API-23] `artifacts::SanitizeName(original) -> std::string`

## [ARCH-16] Invariants and budgets

* [ARCH-16-01] PNG and SSIM operations MUST NOT run inside measured timing loops ([DEC-ARCH-11]).
* [ARCH-16-02] When SSIM is enabled:

  * [ARCH-16-02a] ground truth buffer MUST match dimensions and stride expectations for all compared backends for the same scene.
  * [ARCH-16-02b] dimension mismatch is a fatal error for that comparison case (record as error in report and fail run).
* [ARCH-16-03] SSIM thresholds:

  * [DEC-ARCH-14] Default `--ssim-threshold` is **0.99** (user may override; must be in [0,1]).

    * Rationale: strong similarity expectation while allowing minute backend differences.
    * Alternatives: 0.995 (stricter); 0.95 (looser).
    * Consequences: CI may need to tune threshold per platform/backend; artifacts must be kept to diagnose failures.

## [ARCH-17] Failure modes and handling

* [ARCH-17-01] Output directory creation fails → fatal error before any benchmarking starts.
* [ARCH-17-02] PNG write fails (I/O, permission, encoding) →

  * record error in report row,
  * continue remaining backends/scenes only if user chose a “best-effort” mode; otherwise fail fast.
  * [DEC-ARCH-15] Default policy: **fail fast** on PNG write failure when PNG enabled.

    * Rationale: artifacts are requested; failing silently breaks user expectations.
    * Alternatives: best-effort continue.
    * Consequences: CLI can add `--artifact-best-effort` later via a separate CR.
* [ARCH-17-03] SSIM compare fails because ground truth backend missing → fatal before comparisons ([DEC-ARCH-12]).
* [ARCH-17-04] SSIM result < threshold →

  * record metrics + file paths,
  * complete current scene’s artifact writing,
  * finish report writing,
  * exit with non-zero status ([REQ-154]).

## [TEST-53] Test boundaries per component (v1.1 additions)

* [TEST-53-01] Unit: `artifacts::SanitizeName` deterministic behavior and truncation/hash suffix.
* [TEST-53-02] Unit: `WritePngRGBA8Premul` writes a readable PNG and preserves dimensions.
* [TEST-53-03] Unit: `ComputeSSIM_RGBA8Premul` correctness on known synthetic buffers.
* [TEST-53-04] Integration: harness executes “post-benchmark render” only when PNG/SSIM enabled; never in measured loop (validated by counters/markers).
* [TEST-53-05] Integration: SSIM ordering — ground truth generated first per scene; missing GT backend fails.
* [TEST-53-06] Integration: SSIM threshold failure produces non-zero exit and retains artifacts.

(Concrete test implementations and CI wiring are specified in Ch7/Ch8/Ch9.)

## [ARCH-18] N/A sections (explicit)

* [DEC-ARCH-16] GPU abstraction layers are N/A for this project version (CPU-only rendering focus).

  * Rationale: simplifies scope; aligns with existing backends.
  * Alternatives: add Vulkan/Metal/DX12 GPU backends.
  * Consequences: no GPU sync policy in Ch6 beyond “N/A”.

