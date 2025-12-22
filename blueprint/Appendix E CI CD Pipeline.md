# Appendix E — CI/CD Pipeline Specification (stages, gates, approvals, artifacts)

## Purpose

This appendix specifies the CI/CD pipeline required to build, validate, test, package, and release the benchmark suite across Linux/Windows/macOS and x86_64/ARM64 where feasible. It defines pipeline stages, quality gates, artifact outputs, and release workflows aligned with reproducibility and integrity requirements.

## E.1 Pipeline Overview

1. The project **MUST** use an automated CI pipeline triggered on:

   * pull requests,
   * pushes to main,
   * version tags (release workflow).
2. The pipeline **MUST** enforce quality gates for:

   * build correctness,
   * IR/manifest validation,
   * schema compliance,
   * unit/integration/system tests,
   * packaging integrity for releases.
3. The pipeline **MUST** produce artifacts suitable for debugging failures and for distributing releases.

## E.2 CI Workflows

### E.2.1 Pull Request Workflow (`ci-pr`)

**Triggers**

1. On pull request creation/update.

**Stages**

1. **Checkout & Metadata**

   * Record git sha, branch, PR id.
2. **Configure**

   * CMake configure in at least one configuration (Debug recommended).
3. **Build**

   * Build core modules; adapters may be minimal depending on platform.
4. **Unit Tests**

   * Run backend-independent unit tests.
5. **System Tests**

   * Run CLI black-box tests:

     * `validate`
     * `list`
     * minimal `run` (small preset).
6. **Schema & Manifest Gates**

   * Validate manifest hashes match IR assets.
   * Validate schema version fields exist and required fields present in generated output.
7. **Static Analysis (Recommended)**

   * clang-tidy/format check where feasible.

**Gates**

* PR workflow **MUST** fail if any required stage fails.
* Performance regressions **MUST NOT** be gating by default.

### E.2.2 Main Branch Workflow (`ci-main`)

**Triggers**

1. On pushes/merges to main.

**Stages**

1. All stages from `ci-pr`.
2. **Release-mode Build**

   * Build in Release configuration on at least one platform to detect optimization-related issues.
3. **Adapter Smoke Tests (Platform-Dependent)**

   * Run smoke tests for adapters that are available on that runner.

**Gates**

* Main workflow **MUST** be green before release tags are considered valid.

### E.2.3 Release Workflow (`ci-release`)

**Triggers**

1. On creation of a version tag (e.g., `vX.Y.Z`).

**Stages**

1. **Checkout Tag**
2. **Configure & Build (Release)**

   * For each target OS/arch combination supported by runners:

     * Linux x86_64 (required)
     * Windows x86_64 (required)
     * macOS (required; arch depends on runner availability)
     * ARM64 jobs **SHOULD** be included where available (macOS ARM64 preferred).
3. **Test Gates**

   * Run unit tests and system tests.
   * Run minimal smoke benchmark subset.
4. **Package Artifacts**

   * Assemble binaries + assets + docs subset + metadata.
5. **Generate Integrity Files**

   * Checksums for each artifact.
6. **Publish GitHub Release**

   * Upload artifacts and integrity files.
   * Attach release notes (at least minimal template).

**Gates**

* Release workflow **MUST** not publish artifacts unless:

  * all required OS jobs succeed,
  * manifest/hash validation succeeds,
  * schema compliance checks succeed.

## E.3 Build Matrix Specification

### E.3.1 Required Matrix (Minimum)

1. Linux x86_64
2. Windows x86_64
3. macOS (x86_64 or ARM64, whichever is available and stable)

### E.3.2 Preferred Extended Matrix

1. Linux ARM64
2. Windows ARM64 (if feasible)
3. macOS ARM64 and macOS x86_64

**Normative requirement**

* If an extended target is not available, the pipeline **MUST** document the limitation in release notes and/or repository docs.

## E.4 Stages in Detail

### E.4.1 Configure Stage

1. CMake configure **MUST** use pinned dependency configuration.
2. Build options **MUST** be explicit, including adapter enablement flags.
3. Configure logs **MUST** be captured as CI artifacts on failure.

### E.4.2 Build Stage

1. Build **MUST** produce:

   * CLI executable,
   * worker executable if isolation mode uses a separate binary,
   * adapter modules compiled in for that build variant.
2. Build outputs **MUST** embed build metadata (version identifiers) used by runtime metadata reporting.

### E.4.3 Test Stage

1. Unit tests **MUST** run for all platforms.
2. System tests **MUST** run for all platforms and include at least:

   * `validate`, `list`, minimal `run`.
3. Adapter smoke tests **SHOULD** run where adapters are available.

### E.4.4 Packaging Stage

1. Packaging **MUST** include:

   * executables,
   * assets bundle (manifest + IR assets),
   * schema files (if shipped),
   * dependency pin record,
   * build metadata file,
   * license notices.
2. Packaging **MUST** produce deterministic directory layout inside archives.

### E.4.5 Integrity Stage

1. Checksums **MUST** be generated for each artifact using a standard algorithm (SHA-256 recommended).
2. Checksums file **MUST** be published alongside artifacts.

## E.5 Quality Gates and Approvals

### E.5.1 Required Gates

1. Build success on required matrix targets.
2. Manifest validation gate:

   * all scene hashes match IR payloads.
3. Schema gate:

   * schema_version present and valid,
   * required fields present in generated outputs.
4. Unit/system test success.

### E.5.2 Review/Approval Policy (Repository Governance)

1. Changes touching the IR spec, IR runtime, scenes manifest/assets, or output schema **SHOULD** require maintainer approval.
2. Release tagging **SHOULD** be restricted to maintainers (or a protected workflow) to prevent unreviewed releases.

## E.6 CI Artifacts

### E.6.1 PR/CI Artifacts (Debugging)

CI **SHOULD** upload on failure:

1. CMake configure logs
2. Build logs
3. Test logs
4. Minimal benchmark output directory (for the minimal `run`)

### E.6.2 Release Artifacts (Distribution)

Release workflow **MUST** publish:

1. Per-target binary archives
2. Source archive (tag-based)
3. Checksums file
4. Dependency pin record(s)
5. Build metadata file(s)
6. Adapter inclusion/exclusion manifest per artifact

## E.7 Versioning and Release Notes Automation

1. The pipeline **SHOULD** validate tag format (e.g., `vMAJOR.MINOR.PATCH`).
2. Release notes **MUST** include:

   * IR version and schema version,
   * included adapters per artifact,
   * dependency pin summary or pointer to lock record.

## Acceptance Criteria

1. CI workflows are defined for PRs, main branch, and releases with clear triggers and required stages.
2. A minimum cross-platform build/test matrix is specified, with an extended matrix as a SHOULD target.
3. Quality gates include build success, manifest/hash validation, schema validation, and tests.
4. Release workflows specify packaging, checksums generation, and publishing to GitHub Releases.
5. CI artifacts are defined for both debugging and distribution.

## Dependencies

1. Chapter 11 — Deployment Architecture (environments, packaging expectations).
2. Appendix B — Testing Strategy (test stages and scope).
3. Chapter 9 — Security, Privacy, and Compliance (integrity/provenance requirements).

---

