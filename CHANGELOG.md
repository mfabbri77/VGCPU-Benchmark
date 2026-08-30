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
- Floating dependency issues (asmjit, blend2d, agg, amanithvg)
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
