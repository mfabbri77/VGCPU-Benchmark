# Changelog

All notable changes to VGCPU-Benchmark will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- Retired the Antigravity blueprint/CR governance apparatus in favor of a
  lightweight, OMP-native model: `AGENTS.md` as the single always-active
  contract, `docs/adr/` for architecture decisions, and a frozen
  `requirements/ID_INDEX.md` resolving legacy IDs still cited in code. See
  ADR-0003. `blueprint/`, `cr/`, `.agent/rules/follow-blueprint.md`, and
  `COMPLIANCE_REPORT.md` moved to `docs/archive/legacy-governance-antigravity/`
  (history preserved).

### Added
- `--pin <cpu>` (REQ-13-03): pins the process (and every backend worker
  thread created afterwards) to one logical CPU; Linux/Windows, hard
  error if the pin cannot be applied. The run records `pinned_cpu` and
  the cpufreq `cpu_governor` in `run_metadata.environment` (additive JSON
  fields), warns when the governor is not `performance`, and the HTML
  report shows a "Discipline" row. QUICKSTART documents the rigorous
  protocol. Pinned high-stats run confirms the unpinned rankings within
  a few percent.
- Qt adapter: cap `QThreadPool::globalInstance()` to the harness thread
  budget. Found via `--pin`: Qt's raster engine parallelizes gradient
  fills internally (+119% on fills/gradients_linear when everything
  contends on one pinned core; wall < cpu when unpinned). The cap removes
  most of the pinned contention; unpinned Qt gradient fills remain
  internally parallel (no public knob reaches them) -- with `--pin` the
  numbers are serialized by construction, which is the recommended
  protocol.
- Stroke support implemented in cairo and plutovg (kSetStroke/kStrokePath
  with width, cap and join; paint set through the shared solid/gradient
  helper). Root cause was worse than a missing feature: unhandled opcodes
  fell into `default:` WITHOUT consuming their operand bytes, so the
  first kSetStroke desynchronized the whole command stream. Both adapters
  also gained kSetMatrix/kConcatMatrix, and `default:` now stops parsing
  instead of desynchronizing. vello's FFI stroke additionally honored
  neither width nor cap/join (underscore-ignored args) -- fixed via
  kurbo::Stroke. Verified: non-background pixel count on
  strokes/strokes_curves lands all 10 backends in the same cluster
  (21.4k-22.4k px; before: cairo/plutovg ~0, vello 9.8k), gallery SSIM vs
  cairo >= 0.9985 everywhere.
- Linear and radial gradient support implemented in the five adapters that
  silently fell back to opaque black (cairo, plutovg, agg, raqote, vello;
  the libraries all support gradients natively -- only the adapters/FFI
  bridges were solid-only stubs). cairo/plutovg via native pattern APIs;
  agg via the span_gradient pipeline (256-entry LUT, device-to-gradient
  affine); raqote and vello via new FFI entry points
  (`rqt_fill_path_gradient`, `vlo_fill_path_gradient`). Stop colors follow
  each backend's established channel convention (R<->B pre-swap for
  ARGB32-native backends, raw RGBA for vello). Verified: directional
  4-pixel probe on `fills/gradients_linear` passes 10/10 backends with
  cross-backend agreement within 2/255; gallery SSIM vs cairo >= 0.9984
  everywhere. Gradient-scene timings now measure real gradient work.
- Self-contained HTML report generator (`tools/html_report.py`, ADR-0005):
  one offline file with performance leaderboards, a rendering gallery with
  cross-backend SSIM against a selectable reference backend, amplified
  difference maps, and the correctness-census matrix. Python 3 stdlib
  only. The correctness oracle test additionally exports its census as
  JSON when `VGCPU_ORACLE_JSON` is set. First generated report exposed
  that several adapters (cairo, agg, plutovg, raqote, vello) render
  `fills/gradients_linear` as solid black — gradients unimplemented at
  the adapter level, so their gradient-scene timings measure drawing
  nothing (recorded as follow-up in ADR-0005).
- Correctness oracle suite (`tests/test_correctness_oracle.cpp`, ADR-0004):
  a self-overlap fill-nonzero coverage census, run against every wired
  adapter, distinguishing exact-union rasterizers from the
  sum-then-saturate architecture documented in the market-analysis project.
  Two control cases with a single correct answer are hard requirements for
  every backend; the two self-overlap cases classify and report, never
  fail the build.
- Correctness oracle census extended to all 11 registered backends
  (previously the test binary only ever registered Tier-1 adapters
  regardless of build configuration; `tests/test_main.cpp` now mirrors
  `src/cli/main.cpp`'s full registration list). Result: 7/10 non-null
  backends (blend2d, plutovg, qt, agg, vello, thorvg) classify
  sum-then-saturate; 3 (cairo, skia, amanithvg) classify exact-union.
  Three open follow-ups recorded in ADR-0004 (amanithvg lattice-snap
  epsilon calibration; raqote and thorvg adapter-level anomalies needing
  further investigation, not yet certified as backend defects).
- Blueprint v1.0 adoption with canonical documentation
- CMake presets (dev, release, ci, asan, ubsan, tsan) per [REQ-92]
- Centralized dependency management in `cmake/vgcpu_deps.cmake`
- Tier-1 only build mode (`VGCPU_TIER1_ONLY`) per [DEC-SCOPE-02]
- Quality gate targets: `check_no_temp_dbg`, `format_check`, `format`, `lint`
- Report validators: `validate_report_json.py`, `validate_report_csv.py`
- Test infrastructure with doctest and CTest integration
- Change Request governance scaffolding (`/cr/`)
- Developer quickstart documentation

### Changed
- All dependencies now pinned to immutable tags/SHAs per [REQ-99]
- Rust toolchain pinned to stable 1.84.0 (was nightly)
- CI workflow updated to use CMake presets only
- Release workflow modernized with preset-based builds

### Fixed
- Harness: `--repetitions` was parsed, stored in the policy and reported
  in run metadata but NEVER executed -- every run measured exactly one
  repetition regardless of the flag. Now runs `repetitions` measured
  blocks of `iters` samples aggregated into one pool
  (`sample_count = iters * repetitions`); warmup runs once.
- ThorVG adapter initialized the engine with 1 async worker thread
  hardcoded, so in the `--threads 1` column ThorVG alone rendered on a
  second thread (visible as wall < cpu in every report, and as unstable
  timings under sustained load). Now honors `AdapterArgs.thread_count`
  (N-1 workers, 0 = fully synchronous). Post-fix, ThorVG's wall == cpu.
- Floating dependency issues (asmjit, blend2d, agg, amanithvg)
- Adapter-contract conformance (owner review of the HTML report gallery):
  R<->B channel swap fixed in blend2d, cairo, plutovg, raqote, thorvg and
  amanithvg (ARGB32-native backends; fixed at zero per-pixel cost via
  swapped input colors, or thorvg's native ABGR8888 target); amanithvg
  Y-flip fixed via an OpenVG base flip matrix (benchmark-neutral by
  construction, verified: p50 unchanged within noise on all six).
  Post-fix probe: all 10 backends return exact scene colors. The adapter
  contract in AGENTS.md now states byte order and origin explicitly. New
  precise gaps recorded: cairo and plutovg do not implement kStrokePath.
- `VGCPU_DEP_AMANITHVG_COMMIT` pointed at a commit no longer reachable
  upstream (history rewritten in `Mazatech/amanithvg-sdk`); bumped to
  current `master`.
- `VGCPU_DEP_RUST_TOOLCHAIN` still said `nightly` after the toolchain
  moved to stable (DEC-BUILD-06), which made Corrosion look for a
  nonexistent rustup toolchain and fail configure outright for any build
  with Raqote or vello_cpu enabled.
- Corrosion `v0.5.0` cannot parse `rustup >= 1.28.0`'s
  `toolchain list --verbose` output (upstream corrosion-rs#590); bumped to
  `v0.5.2`.
- Rust toolchain pin bumped 1.84.0 -> 1.86.0: `vello_cpu` 0.0.4 needs the
  `edition2024` Cargo feature (stable since 1.85.0) and declares an MSRV
  of 1.86.0.

### Security
- Dependency pinning prevents supply chain attacks via floating branches

## [0.1.0] - 2025-12-20

### Added
- Initial release with 10 backend adapters:
  - Null (Debug/Testing)
  - PlutoVG
  - Cairo
  - Blend2D
  - Skia
  - ThorVG
  - AGG
  - Qt Raster
  - AmanithVG SRE
  - Raqote (Rust)
  - vello_cpu (Rust, experimental)
- CLI with run, list, metadata, validate commands
- JSON and CSV output formats
- Scene manifest and IR asset pipeline
- Cross-platform support (Windows, macOS, Linux)

[Unreleased]: https://github.com/mfabbri77/VGCPU-Benchmark/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/mfabbri77/VGCPU-Benchmark/releases/tag/v0.1.0
