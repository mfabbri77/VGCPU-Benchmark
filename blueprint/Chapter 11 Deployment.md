# Chapter 11 — Deployment Architecture (Environments, Infra, Scaling)

## Purpose

This chapter specifies how the benchmark suite is built, packaged, and distributed across environments, including developer builds, CI validation, and GitHub Releases for binaries and source. It defines environment-specific configurations, artifact composition, and policies for handling platform/backend variability.

## 11.1 Deployment Model Overview

1. The benchmark suite **MUST** be deployable as:

   * source code (buildable via CMake), and
   * prebuilt binary artifacts distributed via GitHub Releases.
2. The project **MUST NOT** require any hosted runtime services; all execution is local.
3. “Scaling” in this context **MUST** refer to:

   * breadth of supported platforms/architectures,
   * breadth of included backends per build,
   * breadth of scenes and benchmark matrices.

## 11.2 Environments

### 11.2.1 Development Environment (`dev`)

1. `dev` builds **MUST** be runnable from a local checkout.
2. `dev` builds **MUST** allow enabling/disabling adapters via CMake options.
3. `dev` builds **SHOULD** provide fast iteration:

   * incremental builds,
   * selective adapter builds,
   * optional “minimal” configuration (e.g., only one or two backends) to reduce dependency burden.

### 11.2.2 Continuous Integration Environment (`ci`)

1. CI **MUST** build and run validation steps on:

   * Linux x86_64
   * Windows x86_64
   * macOS (at least one architecture supported by available runners; ARM64 preferred where available)
2. CI **MUST** execute at least:

   * manifest and IR validation (`validate`),
   * a smoke benchmark subset (one or more scenes, one or more backends per platform),
   * schema output generation sanity checks.
3. CI **MUST** produce build artifacts for debugging (logs, minimal outputs) on failure.

### 11.2.3 Release Environment (`release`)

1. Release builds **MUST** be attributable to a git tag and recorded git commit SHA.
2. Release builds **MUST** produce:

   * source release (tag-based source archive or equivalent),
   * binary artifacts per OS/arch where feasible.
3. Release builds **MUST** embed or ship:

   * the scene manifest,
   * the IR assets,
   * schema files (if treated as runtime artifacts),
   * dependency pin record(s).

## 11.3 Artifact Composition

### 11.3.1 Binary Release Artifact Contents

Each binary release artifact **MUST** include:

1. The benchmark executable(s) (main CLI; worker executable if isolation mode uses a separate binary).
2. The `assets` bundle:

   * scene manifest,
   * IR assets referenced by the manifest.
3. Documentation subset required to run:

   * minimal README/run instructions,
   * license notices (or a pointer to included license files).
4. Integrity metadata:

   * checksums file for artifacts,
   * dependency pin record (`deps.lock.*`),
   * build metadata (`build-info.json` or equivalent).

### 11.3.2 Optional Adapter Inclusion Policy

1. Not all backends may be buildable or redistributable on all OS/arch combinations.
2. Therefore, release artifacts **MUST** declare:

   * which adapters are included (compiled-in),
   * which adapters are excluded and why (e.g., build failure, unsupported platform, license restriction).
3. The binary **MUST** report adapter availability at runtime (Chapter 7), consistent with the release declaration.

### 11.3.3 Source Release Contents

1. Source release **MUST** include:

   * full source code,
   * CMake build configuration,
   * scene assets and manifest,
   * reproducibility documentation,
   * dependency pin configuration (lockfiles or equivalent).
2. If the project uses fetched dependencies, the source release **MUST** include:

   * pin records sufficient to fetch exact versions,
   * documentation describing the fetch mechanism.

## 11.4 Build and Packaging Pipeline Architecture (High-Level)

1. The canonical build pipeline **MUST** be driven by CMake and produce deterministic build outputs given:

   * pinned dependencies,
   * specified toolchain,
   * specified configuration.
2. Packaging **MUST** produce per-target archives (e.g., `.zip` on Windows, `.tar.gz` on Unix-like systems), unless a different packaging format is chosen consistently.

## 11.5 Configuration Profiles

1. The project **MUST** define at least two build profiles:

   * `Debug` (developer diagnostics),
   * `Release` (benchmarking and distribution).
2. Release builds **MUST** enable compiler optimizations appropriate for fair benchmarking.
3. The project **SHOULD** support a `RelWithDebInfo` profile to aid profiling while retaining representative performance.

## 11.6 Dependency Management Across Environments

1. The build system **MUST** support:

   * vendored/fetched dependencies (pinned),
   * system-installed dependencies (accepted if pinned and recorded).
2. CI and release pipelines **SHOULD** prefer fetched/pinned dependencies to reduce variability, unless a backend strongly requires system packages.
3. When system-installed dependencies are used in CI/release, the pipeline **MUST** record:

   * package source (e.g., OS package manager),
   * package version,
   * any patches or configuration flags.

## 11.7 Runtime Deployment Considerations

### 11.7.1 Headless Execution

1. The benchmark suite **MUST** be runnable headlessly (no GUI required).
2. Any backend requiring a windowing system in its default mode **MUST** be configured for headless CPU rendering or excluded from that platform’s binary artifact.

### 11.7.2 Resource Usage and Scaling

1. The harness **MUST** allow limiting run scope to reduce runtime and resource usage:

   * selecting subsets of scenes/backends,
   * controlling iterations and repetitions.
2. The reporting outputs **MUST** remain tractable as the benchmark matrix grows:

   * stable ordering,
   * per-case records rather than quadratic cross summaries by default.

## 11.8 Upgrade and Compatibility Considerations

1. Binary releases **MUST** be compatible with the IR assets shipped with them.
2. If the IR major version changes, the release notes **MUST**:

   * declare the IR version,
   * declare compatibility expectations with older assets.
3. Backends may evolve; release metadata **MUST** identify backend versions to support longitudinal comparisons.

## Acceptance Criteria

1. The three environments (dev/ci/release) are defined with required behaviors and minimum checks.
2. Binary artifact contents are specified, including assets, metadata, and integrity records.
3. A policy exists for adapter inclusion variability across OS/arch targets, with required declarations in releases and runtime reporting.
4. Build profiles and optimization expectations for fair benchmarking are specified.
5. Dependency management rules for fetched vs system-installed dependencies are specified with recording requirements.
6. Headless execution and matrix scaling controls are explicitly covered.

## Dependencies

1. Chapter 2 — Requirements (cross-platform, distribution, reproducibility).
2. Chapter 7 — Component Design (adapter build toggles, assets module).
3. Chapter 9 — Security, Privacy, and Compliance (checksums, provenance, safe packaging).

---

