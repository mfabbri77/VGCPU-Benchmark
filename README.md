<p align="center">
  <img src="logo.png" alt="VGCPU-Benchmark Logo" width="128">
</p>

<h1 align="center">VGCPU-Benchmark</h1>

<p align="center">
  <b>High-Precision CPU-Only 2D Vector Graphics Benchmark, Correctness Oracle &amp; Memory Profiler</b><br>
  Evaluating raw algorithmic efficiency, memory footprint, and geometric fidelity across modern 2D rasterization engines.
</p>

---

## Table of Contents
- [Overview](#overview)
- [Core Architecture](#core-architecture)
  - [1. Canonical IR (VGIR)](#1-canonical-ir-vgir)
  - [2. Dual-Mode Benchmarking](#2-dual-mode-benchmarking)
  - [3. Analytical Correctness Oracle](#3-analytical-correctness-oracle)
  - [4. Isolated Memory Profiling](#4-isolated-memory-profiling)
- [Supported 2D Engines](#supported-2d-engines)
- [Scene Corpus](#scene-corpus)
- [Building the Suite](#building-the-suite)
- [CLI Commands Reference](#cli-commands-reference)
  - [1. `run` — Execute Performance Benchmarks](#1-run--execute-performance-benchmarks)
  - [2. `list` — Discover Backends and Scenes](#2-list--discover-backends-and-scenes)
  - [3. `profile-memory` — Heap Memory & Working Set Profiling](#3-profile-memory--heap-memory--working-set-profiling)
  - [4. `metadata` — Inspect Environment & Toolchain](#4-metadata--inspect-environment--toolchain)
  - [5. `validate` — Verify IR Scene Assets & Manifest](#5-validate--verify-ir-scene-assets--manifest)
- [Auxiliary Tools & Quality Gates](#auxiliary-tools--quality-gates)
  - [HTML Report Generator (`tools/html_report.py`)](#html-report-generator-toolshtml_reportpy)
  - [Test Suite & Correctness Oracle (`vgcpu_tests`)](#test-suite--correctness-oracle-vgcpu_tests)
  - [IR Asset Generator (`tools/ir_generator.py`)](#ir-asset-generator-toolsir_generatorpy)
  - [Formatting & Architecture Linting](#formatting--architecture-linting)
- [Governance](#governance)
- [License](#license)

---

## Overview

**VGCPU-Benchmark** is a cross-platform benchmarking and analysis framework designed to evaluate the **CPU-only software rasterization** performance, memory working set, and rendering accuracy of modern 2D vector graphics engines.

Unlike GPU-based benchmarks where performance is dominated by driver overhead, bus transfers, and shader scheduling, CPU rasterization measures the fundamental algorithmic efficiency of path flattening, curve decomposition, scanline/cell generation, gradient span interpolation, and clipping.

---

## Core Architecture

### 1. Canonical IR (VGIR)
To ensure absolute fairness, all engines render from a unified **Vector Graphics Intermediate Representation (VGIR)** binary format (`.irbin`). Every backend receives identical path verbs (MoveTo, LineTo, QuadTo, CubicTo, Close), fill rules (NonZero, EvenOdd), stroke parameters (caps, joins, miter limits, dashes), and paint definitions (solid color, linear gradient, radial gradient).

### 2. Dual-Mode Benchmarking
Rendering is measured across two distinct profiles:
* **Mode A — Pre-baked (Retained / Draw-Only)**: Native path and paint objects are prepared upfront in `Prepare()` outside the measurement loop. The timed section measures pure drawing and rasterization throughput.
* **Mode B — Full-Lifecycle (Immediate)**: Measures the complete frame cost in an immediate single-loop execution (`create path -> draw -> destroy path` per draw command), reflecting immediate-mode rendering architectures and measuring memory-allocation overhead under warm CPU caches.

### 3. Analytical Correctness Oracle
SSIM comparison against a reference engine can detect visual drift, but cannot detect when all engines share a systematic flaw. The suite includes an analytical **Self-Overlap Coverage Oracle** (`ADR-0004`) with mathematically hand-derivable expected coverage values:
* **Exact-Union**: Computes the true coverage of overlapping subpaths (e.g. Cairo, Skia, AmanithVG, Raqote).
* **Sum-Then-Saturate**: Sums coverage across subpaths independently and saturates at 1.0 (e.g. Blend2D, Vello CPU, AGG, PlutoVG, ThorVG, Qt).

### 4. Isolated Memory Profiling
Dynamic heap allocations are tracked per-frame in complete isolation from timing benchmarks using custom memory tracking (`pal::TrackMemory`), preventing timing contamination while measuring:
* `alloc_count`: Number of `malloc`/`new` calls per frame.
* `free_count`: Number of deallocations per frame.
* `peak_heap_bytes`: Maximum live heap working set.
* `total_alloc_bytes`: Total heap churn requested during the frame.

---

## Supported 2D Engines

| Backend ID | Engine | Language / Standard | Rasterization Strategy | Coverage Model |
| :--- | :--- | :--- | :--- | :--- |
| `blend2d` | **Blend2D** | C++ (JIT Compiler) | JIT-compiled pipeline (AsmJit) + SIMD span blitters | Sum-then-saturate |
| `vello` | **Vello CPU** (`vello_cpu 0.0.4`) | Rust (Fearless SIMD) | Coarse/Fine Tiling + SIMD cell rasterization | Sum-then-saturate |
| `agg` | **Anti-Grain Geometry 2.6** | C++ | Analytical scanline rasterizer with subpixel precision | Sum-then-saturate |
| `skia` | **Skia CPU** (`m124`) | C++ | Production edge-table & scanline blitters | **Exact-Union** |
| `thorvg` | **ThorVG** (0.12.x) | C++ | Fast-track AABB + Multi-threaded RLE rasterizer | Sum-then-saturate |
| `plutovg` | **PlutoVG** (0.0.4) | C | Lightweight software rasterizer | Sum-then-saturate |
| `qt` | **Qt QPainter** (Qt 6.8) | C++ | Qt Raster Engine scanline blitter | Sum-then-saturate |
| `cairo` | **Cairo** (1.18.2) | C | Trapezoid / Tor-scanline rasterizer | **Exact-Union** |
| `amanithvg` | **AmanithVG SRE** | C (OpenVG 1.1) | OpenVG Tessellator + 1/16 subpixel lattice | **Exact-Union** |
| `raqote` | **Raqote** (0.8.5) | Rust | Fixed-point scanline rasterizer | **Exact-Union** |
| `null` | **Null Adapter** | C++ | Baseline measurement harness with zero drawing operations | N/A |

---

## Scene Corpus

The suite provides **24 canonical scenes** partitioned into two categories:

### 1. Simple Test Suite
* `fills/solid_basic`: 3x2 grid of uniform RGB squares and CMY circles.
* `fills/gradients_linear`: Multi-angle linear gradients with 2 to 5 color stops.
* `fills/spiral_circles`: Concentric circular paths testing curve resolution.
* `fills/nested_rects`: 32 nested squares testing deep alpha compositing and overlap.
* `strokes/strokes_curves`: Stacked wave curves (lines, quadratics, cubics) and spirals.
* `strokes/degen_cusps`: Cusp and near-cusp cubic curves on subpixel grids.
* `strokes/degen_empty`: Zero-length line segments testing cap synthesis.
* `strokes/degen_reversal`: 180-degree path reversals testing miter limits.
* `strokes/degen_short_wide`: Curves with stroke width >> arc length (up to 40:1).
* `validation/subpixel_morton`: Coverage Subpixel Oracle: Exact-Union vs. Sum-Then-Saturate (coincident stacks, rosette petals, shared-edge mesh in a single path with NonZero fill).
* `validation/noop`: 10,000 no-op commands testing harness overhead.
### 2. Complex Test Suite (Real-World MPVG Corpus)
* `complex/paris30k`, `paris50k`, `paris70k`: Geographic map of Paris with up to 50,000+ paths.
* `complex/paris`: Polygon building outlines (3,056 polygons, 41k vertices, EvenOdd rules).
* `complex/hawaii`: Topographic elevation contours (1,137 dense paths).
* `complex/boston`: Dense street map paths with fine strokes (1,922 paths).
* `complex/contour`: High-density mathematical contour plot (53,000 micro-paths).
* `complex/tiger`: Canonical Ghostscript Tiger (305 SVG paths, heavy cubic beziers).
* `complex/drops`: Water drops scene featuring 79 complex radial gradients.
* `complex/car`: Detailed vehicle illustration with 236 gradients and elliptical arcs.
* `complex/paper1`, `paper2`: Complex document glyph outlines and publication diagrams (5,000+ paths).
* `complex/reschart`: Optical resolution test chart with radial line fans.

---

## Building the Suite

### Prerequisites
* **CMake 3.25+**
* **C++20 Compiler**: GCC 13+, Clang 16+, or MSVC 2022
* **Rust Toolchain**: 1.86.0+ (stable)
* **Python 3.10+** (for reporting and IR tools)

### Build Presets
```bash
# Configure and build Release preset (all backends enabled)
cmake --preset release
cmake --build --preset release -j$(nproc)

# Development build
cmake --preset dev
cmake --build --preset dev -j$(nproc)

# Sanitizer builds
cmake --preset asan && cmake --build --preset asan
cmake --preset ubsan && cmake --build --preset ubsan
cmake --preset tsan && cmake --build --preset tsan
```

---

## CLI Commands Reference

The primary benchmark binary is `build/release/vgcpu-benchmark`.

### 1. `run` — Execute Performance Benchmarks
Executes timed rendering runs for both **Pre-baked** and **Full-Lifecycle** modes.

```bash
vgcpu-benchmark run [options]
```

#### Options:
| Option | Description | Default |
| :--- | :--- | :--- |
| `--all-backends` | Run on all available compiled backends | `false` |
| `--backend <id,...>` | Select specific backends (comma-separated, e.g. `blend2d,skia,agg`) | all |
| `--all-scenes` | Run on all scenes in manifest | `false` |
| `--scene <id,...>` | Select specific scenes (e.g. `complex/tiger,fills/nested_rects`) | all |
| `--warmup-iters <n>` | Number of unmeasured warmup iterations per case | `3` |
| `--iters <n>` | Number of measured iterations per repetition | `10` |
| `--repetitions <n>` | Number of interleaved repetitions scheduled across backends | `1` |
| `--threads <n>` | Thread count for multi-threaded adapters | `1` |
| `--pin <cpuset>` | Pin execution to specific CPU core set (e.g. `2` or `0-3`) | none |
| `--out <path>` | Output destination directory for results and artifacts | `.` |
| `--format <type>` | Output format: `json`, `csv`, or `both` | `json` |
| `--png` | Save rendered RGBA frames as PNG artifacts for inspection | `false` |
| `--compare-ssim` | Run SSIM regression against golden image directory | `false` |
| `--golden-dir <path>`| Path to golden reference image directory | `assets/golden` |
| `--fail-fast` | Immediately terminate execution upon the first encountered error | `false` |

#### Example:
```bash
./build/release/vgcpu-benchmark run \
  --all-backends \
  --all-scenes \
  --warmup-iters 2 \
  --iters 5 \
  --png \
  --out out/
```

---

### 2. `list` — Discover Backends and Scenes
Lists all backend adapters registered in the binary and all scenes discovered from the active manifest.

```bash
./build/release/vgcpu-benchmark list
```

---

### 3. `profile-memory` — Heap Memory & Working Set Profiling
Performs isolated, single-frame memory tracking of dynamic heap allocations and peak working sets for both Pre-baked and Full-Lifecycle profiles without benchmark timing contamination.

```bash
vgcpu-benchmark profile-memory [options]
# Alias:
vgcpu-benchmark memory [options]
```

#### Options:
* `--all-backends`, `--backend <id,...>`: Target backend selection.
* `--all-scenes`, `--scene <id,...>`: Target scene selection.
* `--out <path>`: Directory where `memory.json` will be saved.

#### Example:
```bash
./build/release/vgcpu-benchmark profile-memory \
  --all-backends \
  --all-scenes \
  --out out/
```

---

### 4. `metadata` — Inspect Environment & Toolchain
Dumps system environment data, hardware architecture, CPU core topology, OS build, compiler version, timer resolution, and binary compile definitions.

```bash
./build/release/vgcpu-benchmark metadata
```

---

### 5. `validate` — Verify IR Scene Assets & Manifest
Validates all scene binaries (`.irbin`) against `assets/scenes/manifest.json`, verifying SHA-256 integrity hashes, IR version headers, and opcode stream validity.

```bash
./build/release/vgcpu-benchmark validate
```

---

## Auxiliary Tools & Quality Gates

### HTML Report Generator (`tools/html_report.py`)
Generates a self-contained, interactive HTML report with:
* **Unified Top-of-Page Control Bar**: Filter simultaneously across **Profile** (`Full-Lifecycle` vs `Pre-baked`) and **Test-suite** (`Simple` vs `Complex`).
* **Interactive Bar Charts**: Automatic winner highlighting (bold green) with yellow fallback badges and warning bars for unsupported features.
* **Working Memory Profile**: Green highlighting for best-in-class peak heap, allocations/frame, and churn.
* **SSIM & $L_\infty$ PAE Gallery**: Zoomable visual comparison with amplified ($\times 8$) pixel difference maps vs a reference backend.
* **Correctness Census Matrix**: Full breakdown of Exact-Union vs Sum-then-Saturate classifications.

```bash
python3 tools/html_report.py out/ \
  -o out/report.html \
  --reference skia \
  --oracle-json out/oracle.json \
  --memory-json out/memory.json
```

---

### Test Suite & Correctness Oracle (`vgcpu_tests`)
Executes the full unit and integration test suite via doctest:
* Monotonic platform timers and CPU time precision tests.
* Multi-threaded determinism and concurrency verification.
* **Self-Overlap Correctness Oracle** (ADR-0004).

```bash
# Run tests and export machine-readable census JSON
VGCPU_ORACLE_JSON=out/oracle.json ./build/release/vgcpu_tests
```

---

### IR Asset Generator (`tools/ir_generator.py`)
Procedurally synthesizes standard VGIR scene files and regenerates `assets/scenes/manifest.json`.

```bash
python3 tools/ir_generator.py
```

---

### Formatting & Architecture Linting
```bash
# Format all source files (clang-format)
bash tools/format_all.sh

# Validate formatting gate
bash tools/format_check.sh

# Verify no temporary debug markers remain
python3 tools/check_no_temp_dbg.py

# Verify strict include boundaries across architectural layers
python3 tools/check_includes.py src
```

---

## Governance

* **Agent Rules & Architecture Decisions**: See [`AGENTS.md`](AGENTS.md).
* **Architectural Decision Records**: Documented under [`docs/adr/`](docs/adr/).
  * `ADR-0001`: Metric definitions and statistical policy.
  * `ADR-0002`: SSIM and PAE image regression harness.
  * `ADR-0003`: Dual-mode benchmarking (Pre-baked vs Full-lifecycle).
  * `ADR-0004`: Analytical self-overlap coverage correctness oracle.
  * `ADR-0005`: Self-contained HTML report generator.

---

## License

MIT License (see [LICENSE](./LICENSE)).  
Copyright (c) 2025-2026 Michele Fabbri (fabbri.michele@gmail.com)
