# VGCPU-Benchmark Developer Quickstart

> Blueprint Reference: [TASK-09.01], Chapter 7 Build & Toolchain

This guide covers the essential workflow for developing and testing VGCPU-Benchmark.

## Prerequisites

- **CMake** 3.21 or later
- **C++20 compiler**: GCC 11+, Clang 14+, MSVC 2022+, or AppleClang 14+
- **Ninja** (recommended) or Make
- **Python 3** (for quality gate scripts)

Optional (for Rust backends):
- **Rust** stable toolchain (pinned to 1.84.0 in `rust_bridge/rust-toolchain.toml`)

## Quick Start

```bash
# Clone the repository
git clone https://github.com/mfabbri77/VGCPU-Benchmark.git
cd VGCPU-Benchmark

# Configure with development preset
cmake --preset dev

# Build
cmake --build --preset dev

# Run tests
ctest --preset dev

# Run the benchmark
./build/dev/vgcpu-benchmark --help
```

## Build Presets

VGCPU-Benchmark uses CMake presets for consistent, reproducible builds. The following presets are available:

| Preset | Build Type | Purpose |
|:-------|:-----------|:--------|
| `dev` | Debug | Local development, debugging |
| `release` | Release | Optimized builds, performance testing |
| `ci` | Release | CI builds with VGCPU_TIER1_ONLY=ON |
| `asan` | RelWithDebInfo | AddressSanitizer enabled |
| `ubsan` | RelWithDebInfo | UndefinedBehaviorSanitizer enabled |
| `tsan` | RelWithDebInfo | ThreadSanitizer enabled (Linux only) |

### Using Presets

```bash
# Configure
cmake --preset <preset>

# Build
cmake --build --preset <preset>

# Test
ctest --preset <preset>
```

## Backend Tiers

### Tier-1 Backends (Always Available)
These backends are built by default and guaranteed to work on all platforms:

- **null** — No-op backend for testing harness overhead
- **plutovg** — Lightweight CPU software rasterizer
- **blend2d** — High-performance JIT-compiled rasterizer

### Optional Backends
Additional backends require platform-specific dependencies:

| Backend | Dependencies |
|:--------|:-------------|
| Cairo | libcairo2-dev (Linux), cairo (macOS), vcpkg (Windows) |
| Skia | Prebuilt binaries (fetched automatically) |
| ThorVG | Built from source (no external deps) |
| AGG | Built from source + FreeType |
| Qt | Qt6 development libraries |
| AmanithVG | SDK from GitHub (prebuilt) |
| Raqote | Rust toolchain |
| vello_cpu | Rust toolchain |

### Tier-1 Only Mode

For CI or minimal builds, use `VGCPU_TIER1_ONLY`:

```bash
cmake --preset ci  # Automatically sets VGCPU_TIER1_ONLY=ON
```

Or manually:

```bash
cmake -B build -DVGCPU_TIER1_ONLY=ON
```

## Running Benchmarks

```bash
# List available backends and scenes
./build/dev/vgcpu-benchmark list

# Run a single backend on a single scene
./build/dev/vgcpu-benchmark run --backend plutovg --scene fills/solid_basic

# Run all Tier-1 backends on all scenes
./build/dev/vgcpu-benchmark run --all-backends --all-scenes

# Customize iterations
./build/dev/vgcpu-benchmark run --backend blend2d --scene fills/spiral_circles \
    --warmup-iters 5 --iters 20 --repetitions 3

# Output to JSON and CSV
./build/dev/vgcpu-benchmark run --all-backends --scene test/simple_rect \
    --format both --out ./results

# Generate PNG artifacts (Debug/Validation)
./build/dev/vgcpu-benchmark run --backend blend2d --scene fills/solid_basic \
    --png --output-dir ./artifacts

# SSIM Regression Testing
# 1. Generate golden images (store in assets/golden)
# 2. Run with comparison
./build/dev/vgcpu-benchmark run --backend blend2d --scene fills/solid_basic \
    --compare-ssim --golden-dir assets/golden
```

## Quality Gates

Run these before committing:

```bash
# Check for TEMP-DBG markers (must pass before release)
cmake --build --preset dev --target check_no_temp_dbg

# Check code formatting
cmake --build --preset dev --target format_check

# Apply code formatting
cmake --build --preset dev --target format
```

## Testing

```bash
# Run all tests via CTest
ctest --preset dev --output-on-failure

# Run tests with sanitizers
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
```

## Adding New Code

### TEMP-DBG Markers

For temporary debug code, use the TEMP-DBG marker format:

```cpp
// [TEMP-DBG] START reason="testing feature X" owner="@yourname" date="2025-01-15"
std::cout << "Debug output" << std::endl;
// [TEMP-DBG] END
```

**Important**: TEMP-DBG markers must be removed before release. The `check_no_temp_dbg` target will fail if any markers exist.

### Dependency Management

All dependencies are pinned in `cmake/vgcpu_deps.cmake`. To update a dependency:

1. Find the current commit SHA of the dependency
2. Update the corresponding `VGCPU_DEP_*` variable
3. Rebuild and run tests

**Never use floating branches** (`master`, `main`) in dependency declarations.

## Project Structure

```
/
├── blueprint/          # Canonical blueprint v1.0 documentation
├── cmake/              # CMake modules (deps, options, sanitizers)
├── cr/                 # Change Requests for governance
├── docs/               # Additional documentation
├── src/                # Source code
│   ├── adapters/       # Backend adapters
│   ├── cli/            # CLI frontend
│   ├── harness/        # Benchmark harness
│   ├── ir/             # Intermediate representation
│   ├── pal/            # Platform abstraction layer
│   └── reporting/      # JSON/CSV report writers
├── tests/              # Unit tests (doctest)
├── tools/              # Quality gate scripts
├── assets/             # Scene files and manifests
├── CMakeLists.txt      # Main CMake file
└── CMakePresets.json   # CMake presets
```

## Further Reading

- [Blueprint v1.0 Documentation](blueprint/)
- [Implementation Checklist](blueprint/implementation_checklist.yaml)
- [Decision Log](blueprint/decision_log.md)
