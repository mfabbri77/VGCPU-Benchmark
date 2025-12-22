# Chapter 14 — Documentation & Communication Plan

## Purpose

This chapter specifies the documentation set required to operate, reproduce, extend, and release the benchmark suite, and defines how changes are communicated to users and contributors. It establishes required document artifacts, ownership, update policies, and release communication practices.

## 14.1 Documentation Objectives

1. Documentation **MUST** enable a new user to:

   * build or install the suite,
   * run standard benchmark presets,
   * interpret outputs and metadata.
2. Documentation **MUST** enable a contributor to:

   * add a backend adapter via the defined interface,
   * add a scene via IR assets and manifest updates,
   * validate correctness and reproducibility locally and in CI.
3. Documentation **MUST** enable release operators to:

   * generate source and binary releases,
   * produce checksums and dependency records,
   * validate artifact contents and adapter inclusion declarations.

## 14.2 Required Documentation Artifacts

The repository **MUST** contain the following documents (filenames are normative in intent; exact names may vary but must be discoverable from the root README):

### 14.2.1 Root README (User-Facing Entry Point)

The root README **MUST** include:

1. Project purpose and CPU-only scope statement.
2. Supported platforms/architectures.
3. Quickstart:

   * build from source via CMake,
   * run `list`, `validate`, `run` with an example preset.
4. Output artifacts overview (JSON/CSV/summary) and where to find them.
5. Link/index to the rest of the documentation set.

### 14.2.2 IR Specification Document

1. The project **MUST** provide a formal IR specification including:

   * binary container structure (header, sections, alignment),
   * opcode definitions and layouts,
   * semantic contract (fill rules, stroke behavior, gradients, transforms),
   * versioning rules and compatibility expectations.
2. The IR spec **MUST** define invalid input handling and validation rules.
3. The IR spec **MUST** be versioned and updated when IR changes.

### 14.2.3 Scene Suite Documentation

1. The project **MUST** provide a “Scenes Catalog” document listing:

   * each SceneId,
   * what features it exercises,
   * default resolution,
   * required features (capability requirements),
   * SceneHash (or a link to where hashes are recorded).
2. Changes to scenes **MUST** be reflected by updating the catalog and manifest.

### 14.2.4 Backend Adapters Documentation

1. The project **MUST** provide an “Adapters Guide” including:

   * adapter interface contract summary,
   * CPU-only enforcement expectations,
   * per-backend notes:

     * supported capabilities,
     * known semantic deviations,
     * build prerequisites,
     * runtime configuration.
2. The adapters guide **MUST** include a backend capability matrix (or link to a generated matrix artifact).

### 14.2.5 Benchmarking Methodology Document

1. The project **MUST** provide a “Methodology” document describing:

   * warm-up policy,
   * iteration/repetition policy,
   * timed vs untimed boundaries,
   * CPU time vs wall time measurement semantics per platform,
   * recommended benchmarking practices (idle system, power settings).
2. Methodology updates **MUST** accompany changes to harness measurement behavior.

### 14.2.6 Reproducibility Kit Documentation

1. The project **MUST** provide reproducibility instructions including:

   * dependency pinning approach (fetched and system-installed),
   * how dependency versions are recorded in outputs,
   * how to reproduce a published run from a tagged release.
2. Documentation **MUST** describe the contents of release artifacts (assets, schemas, pin records, checksums).

### 14.2.7 Output Schema Documentation

1. The project **MUST** provide schema documentation for:

   * JSON structure and required fields,
   * CSV columns and meanings,
   * schema versioning policy and compatibility guarantees.
2. Schema docs **MUST** include examples (non-normative) and field definitions.

### 14.2.8 Contributing and Governance

1. The project **MUST** provide CONTRIBUTING guidance specifying:

   * coding standards and module boundaries,
   * how to add backends/scenes,
   * required tests and validations,
   * review expectations for IR/schema/scene changes.
2. The project **MUST** define ownership/approval requirements for:

   * IR semantic changes,
   * schema changes,
   * scene suite modifications,
   * release operations.

## 14.3 Documentation Update Policy

1. Any change that affects runtime behavior, outputs, or reproducibility **MUST** update relevant documentation in the same pull request/changeset, including:

   * IR changes → IR spec + changelog entry,
   * scene changes → manifest + scenes catalog + changelog entry,
   * adapter changes → adapters guide + capability matrix updates (if impacted),
   * output schema changes → schema docs + version bump.
2. Documentation **MUST** be treated as an acceptance gate in CI for:

   * presence of required docs,
   * schema/IR version references matching code (where checkable).

## 14.4 Communication Channels and Change Announcements

### 14.4.1 Changelog Structure

1. The project **MUST** maintain a changelog with dedicated sections for:

   * IR changes,
   * schema changes,
   * scene suite changes,
   * adapter additions/removals and behavior changes,
   * build/release process changes.
2. Breaking changes **MUST** be clearly labeled and must reference:

   * affected versions,
   * migration guidance.

### 14.4.2 Release Notes Requirements

For each GitHub Release:

1. Release notes **MUST** include:

   * benchmark suite version (tag/sha),
   * IR version and schema version,
   * included adapters per platform artifact,
   * dependency pin summary (or link to lock record),
   * any known limitations or deviations.
2. Release notes **SHOULD** include a “Recommended standard run command” for that release.

### 14.4.3 Issue and PR Templates (Recommended)

1. The project **SHOULD** provide templates for:

   * bug reports (include run metadata, scene/backend IDs, logs),
   * backend adapter issues (include CPU-only metadata and capability output),
   * performance variance questions (include policy and environment metadata),
   * change proposals for IR/schema (require rationale and compatibility plan).

## 14.5 Documentation Ownership and Review

1. The project **MUST** designate maintainers/owners for:

   * IR specification,
   * output schema,
   * scene suite,
   * release process documentation.
2. Changes to IR semantic contract and schema **MUST** require review by the designated owners.
3. Scene suite changes **MUST** require review focusing on determinism, capability declarations, and comparability impact.

## Acceptance Criteria

1. A complete documentation set is defined, covering users, contributors, and release operators.
2. Update policies ensure documentation evolves with code/assets and is not optional for impactful changes.
3. Communication requirements include a structured changelog and release notes contents sufficient for reproducibility and clarity.
4. Ownership and review requirements are explicitly stated for IR/schema/scene/release documentation.

## Dependencies

1. Chapter 2 — Requirements (deliverables and reproducibility kit).
2. Chapter 5 — Data Design (IR and schema versioning, scene hashes).
3. Chapter 11 — Deployment Architecture (release artifacts and inclusion policy).

---

