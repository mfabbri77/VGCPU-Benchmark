# Changelog

All notable changes to VGCPU-Benchmark will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
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
