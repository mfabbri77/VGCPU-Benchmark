# Chapter 2 — System Architecture (C4 + Platform/Repo/Packaging)

## [ARCH-01] Architecture overview (inherits v1.0)
- [ARCH-01-01] VGCPU-Benchmark is a layered CLI application with internal static libraries:
  - `vgcpu_core` (PAL + IR + assets + common types)
  - `vgcpu_adapters` (backend adapters + adapter registry)
  - `vgcpu_harness` (benchmark orchestration + statistics)
  - `vgcpu_reporting` (CSV/JSON/summary writers)
  - `vgcpu-benchmark` (CLI entrypoint)
- [ARCH-01-02] External backends are compiled/linked in as build-time optional dependencies (C/C++ libs and optional Rust crates via Corrosion).
- [DEC-ARCH-01] Treat all `vgcpu_*` libraries as **internal implementation** (no stable external ABI promise).
  - Rationale: project is a CLI benchmark tool; stabilizing an SDK ABI would constrain evolution.
  - Alternatives: (A) ship a public SDK with exported headers; (B) provide a C API for adapters.
  - Consequences: enforce “internal-only” via include layout + symbol visibility rules (see Ch4/Ch7).

## [ARCH-02] C4 — System context

```mermaid
flowchart LR
  U[User / CI] -->|invokes CLI| CLI[vgcpu-benchmark]
  CLI -->|reads| ASSETS[(Assets: manifest.json + *.irbin)]
  CLI -->|loads| CFG[(CLI options / env)]
  CLI -->|runs| H[Harness]
  H -->|calls| REG[Adapter registry]
  REG -->|selects| AD[Backend adapter]
  AD -->|renders| BUF[(RGBA8 premul buffer)]

  H -->|writes| REPORT[(CSV/JSON + stdout summary)]
  H -->|optional| PNG[(PNG artifacts)]
  H -->|optional| SSIM[(SSIM compare)]
  SSIM -->|uses| GT[(Ground truth buffer)]
  PNG -->|stores in| OUTDIR[(output_dir/png)]
  REPORT -->|stores in| OUTDIR2[(output_dir)]
````

* [ARCH-02-01] External actors are limited to the user/CI invoking the CLI and reading the produced reports/artifacts.
* [ARCH-02-02] Assets are local files; no network I/O is required or permitted by default ([REQ-14]).

## [ARCH-03] C4 — Containers (process + internal libraries)

```mermaid
flowchart TB
  subgraph P[Process: vgcpu-benchmark]
    CLI[CLI front-end\n(parse args, validate options)]
    H[vgcpu_harness\n(warmup+reps, timing, stats)]
    R[vgcpu_reporting\n(JSON/CSV/summary + artifact index)]
    A[vgcpu_adapters\n(adapter registry + adapters)]
    C[vgcpu_core\n(PAL, IR loader, assets, types)]
    IO[Artifact IO\n(PNG writer, SSIM compare)]
  end

  CLI --> H
  H --> A
  A --> C
  H --> C
  H --> IO
  IO --> R
  H --> R
```

* [ARCH-03-01] v1.1 introduces **Artifact IO** responsibilities (PNG encoding + SSIM comparison) as a small internal module (still inside the same process).
* [ARCH-03-02] All backends remain in-process; no IPC boundary exists.

## [ARCH-04] Execution phases (including v1.1 comparison phase)

* [ARCH-04-01] The harness executes in deterministic *phases* per selected scene:

  1. [ARCH-04-01a] Load/prepare scene IR + surface config.
  2. [ARCH-04-01b] Warmup loop (untimed).
  3. [ARCH-04-01c] Measured loop (timed) and stats aggregation.
  4. [ARCH-04-01d] **Post-benchmark artifact phase (untimed)**: optional re-render for PNG, and optional SSIM compare.
* [DEC-ARCH-11] PNG generation MUST use a **separate post-benchmark render** (clean separation from timing).

  * Rationale: prevents file I/O and encoding from contaminating benchmark measurements.
  * Alternatives: reuse “last measured iteration” buffer.
  * Consequences: adapters must be callable for an extra untimed render; reporting must mark artifacts as “post-benchmark”.

### [ARCH-04-02] SSIM ordering policy (ground truth first)

* [ARCH-04-02a] When SSIM is enabled, each scene MUST be processed in two logical steps:

  * Step 1: run the **ground-truth backend** for the scene, then generate its PNG and retain its raw buffer.
  * Step 2: run each non-ground-truth backend, generate PNG, compute SSIM vs the retained ground-truth buffer.
* [DEC-ARCH-12] If the specified ground-truth backend is not available (not built/enabled), the run MUST **fail** before running comparisons.

  * Rationale: avoids silently producing meaningless “no ground truth” results.
  * Alternatives: skip comparisons with warning.
  * Consequences: CLI validation must check build-enabled backends early (see Ch4).

## [ARCH-05] Platform matrix (runtime)

| Concern                        | Windows                           | macOS                                               | Linux                                                          |
| ------------------------------ | --------------------------------- | --------------------------------------------------- | -------------------------------------------------------------- |
| [ARCH-05-01] Timing            | `QueryPerformanceCounter`         | `mach_absolute_time` or `std::chrono::steady_clock` | `clock_gettime(CLOCK_MONOTONIC)` / `std::chrono::steady_clock` |
| [ARCH-05-02] File paths        | UTF-16 boundary -> UTF-8 internal | UTF-8                                               | UTF-8                                                          |
| [ARCH-05-03] Threads           | Win32 threads / std::thread       | pthreads / std::thread                              | pthreads / std::thread                                         |
| [ARCH-05-04] PNG write         | stdio / fstreams                  | stdio / fstreams                                    | stdio / fstreams                                               |
| [ARCH-05-05] SIMD/CPU features | optional                          | optional                                            | optional                                                       |

* [ARCH-05-06] The PNG/SSIM features must be purely CPU + file I/O; no OS-specific graphics surfaces are introduced.

## [ARCH-06] Repo artifact map (build-time + runtime outputs)

* [ARCH-06-01] **Assets**:

  * `assets/manifest.json`
  * `assets/scenes/*.irbin`
* [ARCH-06-02] **Primary executable**:

  * `vgcpu-benchmark`
* [ARCH-06-03] **Reports (existing)**:

  * `<output_dir>/*.json`
  * `<output_dir>/*.csv`
* [ARCH-06-04] **New in v1.1 — PNG artifacts**:

  * `<output_dir>/png/<scene>_<backend>.png` ([DEC-SCOPE-05])
* [ARCH-06-05] **New in v1.1 — SSIM results**:

  * Persist SSIM metrics into JSON/CSV report rows for each (scene, backend) compared to ground truth (see Ch3/Ch4 schema details).

## [ARCH-07] Boundaries and trust model

* [ARCH-07-01] Trust boundary is the local machine; inputs are CLI args and local asset files.
* [ARCH-07-02] Untrusted inputs:

  * [ARCH-07-02a] scene selection strings
  * [ARCH-07-02b] output directory path
  * [ARCH-07-02c] backend selection strings
* [ARCH-07-03] The tool MUST validate options early and fail fast with actionable messages (see Ch4 error contracts).

## [ARCH-08] Output directory policy (v1.1 additions)

* [ARCH-08-01] Output directory MUST be created if missing; failures are fatal and reported via structured errors.
* [ARCH-08-02] PNG directory `<output_dir>/png/` is created only when PNG output is enabled.
* [ARCH-08-03] File name sanitization:

  * [ARCH-08-03a] Scene names and backend ids MUST be sanitized to a filesystem-safe subset.
  * [ARCH-08-03b] Sanitization MUST be deterministic and reversible enough for reporting (store original names separately in report fields).
* [ARCH-08-04] Collision policy:

  * [ARCH-08-04a] Since naming includes backend, collisions are only expected if duplicate backend ids exist (forbidden by adapter registry contract in Ch4).
  * [ARCH-08-04b] If a collision still occurs, writer MUST overwrite only if a `--overwrite` policy exists; otherwise append a numeric suffix. (Exact policy finalized in Ch4; default is “overwrite within a run only”.)

## [ARCH-09] Packaging & distribution (inherits v1.0; v1.1 adds vendored headers)

* [ARCH-09-01] Distribution is source-first; users build with CMake presets.
* [ARCH-09-02] Optional backends are controlled via CMake options; build produces a single CLI binary.
* [ARCH-09-03] v1.1 adds vendored/header-only dependencies:

  * [ARCH-09-03a] `stb_image_write.h` for PNG writing.
  * [ARCH-09-03b] a lightweight SSIM implementation (header-only or small TU) pinned to a specific revision.
* [ARCH-09-04] All third-party licenses MUST be recorded in `THIRD_PARTY_NOTICES.md` and shipped in source archives (see Ch7/Ch9).

## [ARCH-10] Architecture deltas for v1.1 (summary)

* [ARCH-10-01] Add “artifact pipeline” after measurement to:

  * [ARCH-10-01a] render an untimed frame per backend/scene when PNG enabled,
  * [ARCH-10-01b] compute SSIM against ground truth when SSIM enabled,
  * [ARCH-10-01c] persist artifact paths + SSIM into report outputs.
* [ARCH-10-02] Add CLI validation rules for:

  * [ARCH-10-02a] enabling PNG output,
  * [ARCH-10-02b] enabling SSIM compare requires ground truth backend,
  * [ARCH-10-02c] missing ground truth backend is fatal ([DEC-ARCH-12]).

