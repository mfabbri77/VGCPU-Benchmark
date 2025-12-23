# Migration Guide: v0.1.0 → v0.2.0

> Blueprint Reference: [TASK-09.02], Chapter 9 Versioning & Lifecycle

This document covers the migration from VGCPU-Benchmark v0.1.0 to v0.2.0.

## Overview

v0.2.0 introduces **Blueprint v1.0** adoption, which modernizes the build system, 
formalizes backend tiers, and adds quality gates. Most changes are additive 
and backwards-compatible.

## Build System Changes

### CMake Presets (New)

v0.2.0 requires using CMake presets for builds:

```bash
# Old (v0.1.0)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# New (v0.2.0)
cmake --preset release
cmake --build --preset release
```

Available presets: `dev`, `release`, `ci`, `asan`, `ubsan`, `tsan`

### Tier-1 Only Mode (New)

For CI builds with minimal dependencies:

```bash
cmake --preset ci  # Sets VGCPU_TIER1_ONLY=ON automatically
```

This builds only: `null`, `plutovg`, `blend2d`

### Dependency Management

Dependencies are now centralized in `cmake/vgcpu_deps.cmake`. All floating 
branches (`master`, `main`) have been pinned to immutable commit SHAs.

**Impact**: More reproducible builds, but dependency updates require explicit 
version bumps in `vgcpu_deps.cmake`.

## CLI Changes

### No Breaking Changes

All CLI flags and commands remain compatible:

```bash
./vgcpu-benchmark run --backend plutovg --scene fills/solid_basic
./vgcpu-benchmark list
./vgcpu-benchmark metadata
./vgcpu-benchmark validate
```

## Report Schema Changes

### JSON Reports

The JSON report schema is unchanged. The `schema_version` field remains `"0.1.0"`.

### CSV Reports

CSV output format is unchanged.

## Backend Changes

### Backend Tiering

Backends are now classified into tiers:

| Tier | Backends | Guarantee |
|:-----|:---------|:----------|
| Tier-1 | null, plutovg, blend2d | Always built, fully tested |
| Optional | All others | Platform-dependent, may require deps |

### No Backend API Changes

The backend adapter interface (`IBackendAdapter`) is unchanged.

## Testing Changes

### CTest Integration (New)

Unit tests are now registered with CTest:

```bash
ctest --preset dev
```

### Doctest Framework

Tests use [doctest](https://github.com/doctest/doctest) v2.4.11.

## CI/CD Changes

### Preset-Only Builds

CI workflows now use presets exclusively. Direct CMake commands are no longer used.

### Quality Gates

New quality gates are enforced:

- `check_no_temp_dbg` — No TEMP-DBG markers allowed
- `format_check` — Code formatting must pass

### Sanitizer Jobs

Linux CI includes ASan and UBSan jobs.

## Migration Checklist

- [ ] Update build scripts to use CMake presets
- [ ] Review pinned dependency versions in `cmake/vgcpu_deps.cmake`
- [ ] Remove any TEMP-DBG markers before release
- [ ] Run `ctest --preset dev` to verify tests pass
- [ ] Update CI scripts to use preset-based commands

## Breaking Changes Summary

**None** — v0.2.0 is backwards-compatible with v0.1.0 for:
- CLI flags and commands
- Report schemas (JSON/CSV)
- Backend names and interfaces

The only changes are in the build system (presets) and internal quality gates.

## Getting Help

If you encounter issues migrating:

1. Check the [Developer Quickstart](docs/QUICKSTART.md)
2. Review the [Blueprint Documentation](blueprint/)
3. Open an issue on GitHub
