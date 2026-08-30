# Chapter 7 — Build, Toolchain, Dependencies, CI Gates (CMake)

This chapter codifies an implementation-ready build system aligned with the normative guidance in:
- `cmake_playbook.md` (presets/targets/sanitizers/quality gates)
- `ci_reference.md` (CI gates + preset-driven commands)
- `dependency_policy.md` (pinning/reproducibility)

## [BUILD-01] Toolchain baseline (binding)
- [REQ-88] Minimum CMake version MUST remain **3.21** (matches observed repo) unless raised by CR.
- [REQ-89] C++ standard MUST be **C++20** ([DEC-SCOPE-01]).
- [REQ-90] Generator policy:
  - [REQ-90-01] Prefer **Ninja** for single-config builds on all OSes.
  - [REQ-90-02] Multi-config generators are allowed only if also represented by presets.
- [REQ-91] Windows CRT policy:
  - [REQ-91-01] Default to `/MD` (dynamic CRT) for all configs.

- [DEC-BUILD-01] Keep existing CMake option names (`ENABLE_*`) for backends to avoid breaking docs/scripts.
  - Alternatives: rename to `VGCPU_ENABLE_*`.
  - Consequences: we add a thin alias layer later only via CR if needed.

## [BUILD-02] Canonical CMake Presets (mandatory)
Per `cmake_playbook.md`, we define the minimum preset set: `dev`, `release`, `asan`, `ubsan`, `tsan`, `ci`.
Per `ci_reference.md`, CI MUST use presets only.

### [REQ-92] Required preset set
- [REQ-92-01] Configure presets:
  - `dev` (Debug-like)
  - `release` (Release)
  - `asan` (AddressSanitizer)
  - `ubsan` (UndefinedBehaviorSanitizer)
  - `tsan` (ThreadSanitizer; where supported)
  - `ci` (deterministic CI build; typically Release + Werror)
- [REQ-92-02] Build presets MUST exist for each configure preset (same names).
- [REQ-92-03] Test presets MUST exist for each configure preset (same names).

### [REQ-93] Preset invariants
- [REQ-93-01] `CMAKE_EXPORT_COMPILE_COMMANDS=ON` in `dev` and `ci` (tooling support).
- [REQ-93-02] `ci` MUST enable “warnings as errors” for VGCPU code (not for third-party).
- [REQ-93-03] Sanitizers must not be combined unless explicitly validated (see [DEC-BUILD-04]).

### [DEC-BUILD-02] Preset naming and command contract is binding
- Rationale: matches the normative preset convention and simplifies CI docs.
- Consequences: all docs and workflows call `cmake --preset <name>` / `cmake --build --preset <name>` / `ctest --preset <name>` only.

### Minimal `CMakePresets.json` skeleton (authoritative)
- [REQ-94] Repository MUST contain a `CMakePresets.json` that includes, at minimum, the following shape (values may be adjusted by platform):
```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 21, "patch": 0 },
  "configurePresets": [
    {
      "name": "dev",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/dev",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "VGCPU_WERROR": "OFF"
      }
    },
    {
      "name": "release",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "VGCPU_WERROR": "OFF"
      }
    },
    {
      "name": "ci",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/ci",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "VGCPU_WERROR": "ON",
        "VGCPU_TIER1_ONLY": "ON"
      }
    },
    {
      "name": "asan",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/asan",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "VGCPU_ENABLE_ASAN": "ON"
      }
    },
    {
      "name": "ubsan",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/ubsan",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "VGCPU_ENABLE_UBSAN": "ON"
      }
    },
    {
      "name": "tsan",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/tsan",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "VGCPU_ENABLE_TSAN": "ON"
      }
    }
  ],
  "buildPresets": [
    { "name": "dev", "configurePreset": "dev" },
    { "name": "release", "configurePreset": "release" },
    { "name": "ci", "configurePreset": "ci" },
    { "name": "asan", "configurePreset": "asan" },
    { "name": "ubsan", "configurePreset": "ubsan" },
    { "name": "tsan", "configurePreset": "tsan" }
  ],
  "testPresets": [
    { "name": "dev", "configurePreset": "dev", "output": { "outputOnFailure": true } },
    { "name": "ci", "configurePreset": "ci", "output": { "outputOnFailure": true } },
    { "name": "asan", "configurePreset": "asan", "output": { "outputOnFailure": true } },
    { "name": "ubsan", "configurePreset": "ubsan", "output": { "outputOnFailure": true } },
    { "name": "tsan", "configurePreset": "tsan", "output": { "outputOnFailure": true } }
  ]
}
````

## [BUILD-03] Standard build options (project-level)

### [REQ-95] Standard project options (binding)

* [REQ-95-01] `VGCPU_WERROR` (ON in `ci`, OFF otherwise)
* [REQ-95-02] `VGCPU_TIER1_ONLY` (ON in `ci`; forces optional backends OFF)
* [REQ-95-03] `VGCPU_ENABLE_ASAN`, `VGCPU_ENABLE_UBSAN`, `VGCPU_ENABLE_TSAN` (preset-driven)
* [REQ-95-04] `VGCPU_ENABLE_LINT` (default OFF; ON in dedicated CI lint job)
* [REQ-95-05] `VGCPU_ENABLE_ALLOC_INSTRUMENTATION` (default OFF; ON in debug/test builds where supported)

### Backend toggles (existing, preserved)

* [REQ-96] Existing backend options remain supported (observed):

  * `ENABLE_NULL_BACKEND`, `ENABLE_PLUTOVG`, `ENABLE_CAIRO`, `ENABLE_BLEND2D`, `ENABLE_SKIA`,
    `ENABLE_THORVG`, `ENABLE_AGG`, `ENABLE_QT`, `ENABLE_AMANITHVG`, `ENABLE_RAQOTE`, `ENABLE_VELLO_CPU`.
* [DEC-BUILD-03] Defaults:

  * Tier-1: `ENABLE_NULL_BACKEND=ON`, `ENABLE_PLUTOVG=ON`, `ENABLE_BLEND2D=ON`
  * Optional: default OFF when `VGCPU_TIER1_ONLY=ON`; otherwise keep current repo defaults (ON) for developer convenience.
  * Consequences: CI is stable (Tier-1 only), developers can opt into “full build” locally.

## [BUILD-04] Sanitizers (mandatory presets)

Per `cmake_playbook.md`, sanitizers must be separated and runtime-configured.

* [REQ-97] Sanitizer flags MUST apply only to VGCPU targets (not third-party) where feasible.

* [REQ-98] CI MUST document runtime options:

  * `ASAN_OPTIONS`, `UBSAN_OPTIONS`, `TSAN_OPTIONS` as needed.

* [DEC-BUILD-04] Do NOT combine sanitizers in v0.2.0 presets.

  * Rationale: portability and reduced flakiness across OS/toolchains.
  * Alternatives: ASan+UBSan combined on Linux only.
  * Consequences: separate CI jobs for asan/ubsan; tsan only where supported (Linux/Clang).

## [BUILD-05] Dependency policy (pinning + reproducibility)

Normative policy: `dependency_policy.md` discourages floating branches and requires pinning.

### [REQ-99] Pinning rules (binding)

* [REQ-99-01] No dependency may track a floating branch (`master`, `main`) in shipped presets (`ci`, `release`).
* [REQ-99-02] Every FetchContent dependency MUST be pinned to:

  * an immutable tag AND/OR
  * an exact commit SHA (preferred when upstream tags move or are absent).
* [REQ-99-03] CI MUST fail if any dependency uses `GIT_TAG master/main` (enforced by a configure-time check).

### [DEC-BUILD-05] Keep FetchContent for v0.2.0 but make it deterministic

* Rationale: minimal disruption to the current repo; still satisfies pinning by prohibiting floating refs.
* Alternatives: migrate to vcpkg manifest mode; migrate to conan.
* Consequences: we add:

  * a centralized `cmake/vgcpu_deps.cmake` with all versions/SHAs,
  * a configure-time linter that rejects floating refs.

### [REQ-100] Concrete dep fixes required (from intake)

The following observed floating deps MUST be pinned before v0.2.0:

* [REQ-100-01] asmjit (currently `master`)
* [REQ-100-02] blend2d (currently `master`)
* [REQ-100-03] agg-2.6 (currently `master`)
* [REQ-100-04] amanithvg-sdk (currently `master`)

### [REQ-101] License and attribution

* [REQ-101-01] A `/third_party/` directory MUST contain:

  * `LICENSES/` with collected license texts (at least for redistributed prebuilts),
  * `THIRD_PARTY_NOTICES.md` generated/maintained.
* [REQ-101-02] Release packaging MUST include applicable notices if it redistributes prebuilts (see Ch2 [DEC-ARCH-06]).

## [BUILD-06] Rust toolchain and Corrosion integration (optional backends)

Rust adapters are optional by default (Ch2 [DEC-ARCH-04]).

* [DEC-BUILD-06] Switch Rust toolchain pin from floating `nightly` to a pinned **stable** toolchain version for reproducibility and portability.

  * Implementation: `rust_bridge/rust-toolchain.toml` uses `channel = "1.84.0"` (example pinned stable) and includes targets for Windows/macOS/Linux.
  * Rationale: current crates (edition 2021, `raqote`, `vello_cpu`) do not require nightly features in typical usage.
  * Alternatives: pin `nightly-YYYY-MM-DD`; keep floating nightly (not allowed by [REQ-99]).
  * Consequences: if any crate actually requires nightly, it must be surfaced as a CR with justification.

* [REQ-102] When Rust backends are enabled:

  * [REQ-102-01] `Cargo.lock` MUST be committed for deterministic builds.
  * [REQ-102-02] CMake builds MUST use `cargo build --locked` (via Corrosion configuration where possible).
  * [REQ-102-03] CI MUST treat Rust backends as a separate (optional) job until promoted to Tier-1.

## [BUILD-07] Quality gate targets (mandatory)

Per `cmake_playbook.md` and `ci_reference.md`, quality gates must be build targets.

### [REQ-103] Standard gate targets

CMake MUST define these targets:

* [REQ-103-01] `format` and `format_check` (clang-format apply vs verify)
* [REQ-103-02] `lint` (clang-tidy) when `VGCPU_ENABLE_LINT=ON`
* [REQ-103-03] `check_no_temp_dbg` (fails on TEMP-DBG markers)
* [REQ-103-04] `tests` (builds and runs the unit test binary/binaries via CTest or direct invocation)

### TEMP-DBG enforcement

* [REQ-104] TEMP-DBG code MUST use the exact markers from `temp_dbg_policy.md`:

```cpp
// [TEMP-DBG] START <reason> <owner> <date>
...temporary debug code...
// [TEMP-DBG] END
```

* [REQ-105] `check_no_temp_dbg` MUST fail the build if any marker exists in tracked sources.

## [BUILD-08] CI command contract (binding)

Per `ci_reference.md`, CI uses presets and the following commands only:

* [REQ-106] Configure:

  * `cmake --preset <preset>`
* [REQ-107] Build:

  * `cmake --build --preset <preset>`
* [REQ-108] Test:

  * `ctest --preset <preset> --output-on-failure`
* [REQ-109] Quality gates:

  * `cmake --build --preset <preset> --target check_no_temp_dbg`
  * `cmake --build --preset <preset> --target format_check`
  * `cmake --build --preset <preset> --target lint` (when enabled)

## [BUILD-09] CI minimum matrix (Tier-1)

* [REQ-110] Minimum CI jobs for PRs:

  * [REQ-110-01] Windows (MSVC) `ci` preset + smoke tests
  * [REQ-110-02] macOS (AppleClang) `ci` preset + smoke tests
  * [REQ-110-03] Linux (Clang) `ci` preset + smoke tests
  * [REQ-110-04] Linux ASan job (`asan` preset)
  * [REQ-110-05] Linux UBSan job (`ubsan` preset)
  * [REQ-110-06] Linux TSan job (`tsan` preset) if supported; otherwise run concurrency stress tests ([REQ-87]).

* [DEC-BUILD-07] Cache policy for this repo (FetchContent-based):

  * cache `build/**/_deps` and `~/.cache` as applicable,
  * cache `rust_bridge/target` for Rust optional jobs,
  * cache key includes OS + compiler + a hash of `cmake/vgcpu_deps.cmake` + `Cargo.lock`.
  * Rationale: aligns with `ci_reference.md` caching intent even without vcpkg/conan.
  * Consequences: ensure cache is read-only for untrusted forks if required by repo security.

## [BUILD-10] Packaging presets and artifact contract

* [REQ-111] Provide a `package` build target (or script) that produces the existing archive layout:

  * `package/bin/vgcpu-benchmark[.exe]`
  * `package/assets/**`
  * `package/README.md`
  * checksums
* [DEC-BUILD-08] Keep the existing `scripts/package_release.sh` but also expose it via a preset-driven CMake custom target:

  * `cmake --build --preset release --target package`
  * Rationale: unify local and CI behavior.
  * Alternatives: migrate to CPack.
  * Consequences: CI release workflow becomes simpler and more deterministic.

## [BUILD-11] Build & CI tests (build-system focused)

These tests ensure the build contract is actually enforced.

* [TEST-36] `cmake_presets_exist`:

  * verify required presets exist in `CMakePresets.json` and configure successfully.
* [TEST-37] `no_floating_deps`:

  * configure-time or script-based check fails if any dependency uses `master/main`.
* [TEST-38] `quality_targets_present`:

  * build-system test ensures targets `format_check`, `check_no_temp_dbg`, `tests` exist.
* [TEST-39] `ci_uses_presets_only`:

  * workflow lint (script) ensures no raw `cmake -D...` calls appear outside presets.

## [BUILD-12] Traceability notes

* [BUILD-12-01] This chapter defines [REQ-88..111] and [TEST-36..39].
* [BUILD-12-02] Ch8 defines the concrete tooling implementation (clang-format/clang-tidy config files, scripts).
* [BUILD-12-03] Ch9 defines release governance, SemVer, and perf regression harness integration into CI.
