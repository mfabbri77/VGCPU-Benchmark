<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Chapter 13 — Risk Management and Rollback Strategy

## Purpose

This chapter identifies key project risks, defines mitigation actions, and specifies rollback strategies for changes that affect reproducibility and comparability (IR, scenes, adapters, schemas, and releases). It provides operational guidance for handling regressions, misconfigurations, and unstable results.

## 13.1 Risk Register

### 13.1.1 Semantic Mismatch Across Backends

**Description**
Backends implement fills/strokes/gradients/transforms with subtle differences (e.g., join behavior, dash phase rules, gradient interpolation), undermining fairness.

**Impact**
Results may not be comparable; stakeholders may misinterpret performance differences caused by semantics.

**Likelihood**
High.

**Mitigations**

1. The IR semantic contract **MUST** specify edge-case behavior (joins, miter limit, dash semantics, fill rules) and adapter mapping expectations.
2. The suite **MUST** maintain a backend capability matrix and default to skipping unsupported features rather than degrading semantics.
3. The suite **SHOULD** provide optional diagnostic rendering outputs and tolerance-based comparisons (non-gating by default) to identify semantic divergences.
4. Adapters **MUST** document deviations and include them in backend metadata.

**Detection**

* CI smoke tests per backend with known “sentinel scenes” designed to expose semantic differences.

### 13.1.2 CPU-Only Enforcement Ambiguity

**Description**
Some libraries may auto-select GPU paths or use OS-accelerated routes; CPU-only guarantees may be violated.

**Impact**
Invalid results; misleading comparisons.

**Likelihood**
Medium to High (backend-dependent).

**Mitigations**

1. Each adapter **MUST** explicitly select CPU-backed surfaces and disable GPU acceleration when applicable.
2. Adapters **MUST** record enforcement configuration in metadata (surface type, flags, runtime mode).
3. The harness **SHOULD** provide a “CPU-only audit” mode that prints enforcement details per backend before running.
4. Binary releases **MUST** declare backend configurations used.

**Detection**

* Adapter-level assertions where possible (e.g., querying backend runtime for selected device/renderer), labeled best-effort.

### 13.1.3 Measurement Noise and Non-Deterministic Performance

**Description**
OS scheduling, power management, background tasks, and backend internal threading create high variance.

**Impact**
Unstable results; difficulty comparing regressions.

**Likelihood**
High.

**Mitigations**

1. Harness **MUST** use warm-up and statistical summaries (median + dispersion).
2. The suite **SHOULD** support repetitions and optionally process isolation mode.
3. Documentation **MUST** include guidance for stable benchmarking (idle system, performance governor, consistent power settings).
4. Outputs **MUST** include environment metadata and policy parameters.

**Detection**

* CI thresholds for variance are discouraged; instead, CI validates correctness and schema, not performance stability.

### 13.1.4 Build and Packaging Variability Across Platforms

**Description**
Backends differ in build systems, dependencies, and platform support; binary releases may vary in included adapters.

**Impact**
Inconsistent feature coverage; confusion for users; increased maintenance cost.

**Likelihood**
High.

**Mitigations**

1. CMake options **MUST** clearly control adapter enablement, and runtime **MUST** report compiled-in adapters.
2. Release artifacts **MUST** include a manifest of included/excluded adapters with reasons.
3. Dependency pin records **MUST** capture versions/commits and system-installed versions when used.
4. The project **SHOULD** prioritize backends with good portability for early milestones.

**Detection**

* CI builds matrix across OS/arch (as runner availability permits) and verifies adapter presence reporting.

### 13.1.5 Licensing and Redistribution Constraints

**Description**
Some backends may have licensing terms that complicate redistribution in binary releases.

**Impact**
Inability to ship certain adapters; legal exposure.

**Likelihood**
Medium.

**Mitigations**

1. The project **MUST** document license obligations per backend (Appendix F).
2. The release pipeline **MUST** allow excluding adapters from binary distributions when required.
3. Source distribution **MUST** remain usable even if binaries omit certain adapters.

**Detection**

* License review checklist as part of release readiness.

### 13.1.6 IR / Schema Evolution Breaking Comparability

**Description**
Changes to IR semantics, scene assets, or output schema can invalidate longitudinal comparisons.

**Impact**
Historical results become non-comparable; users lose trust.

**Likelihood**
Medium.

**Mitigations**

1. IR and schema **MUST** be versioned with explicit compatibility rules.
2. SceneHash **MUST** be emitted and used to identify content changes.
3. Changes to scenes/IR **MUST** be documented with migration notes and release notes.
4. The project **SHOULD** provide “standard run presets” pinned to specific scene sets/versions.

**Detection**

* CI checks that scene hashes match manifest; schema compatibility tests.

## 13.2 Rollback Strategy

### 13.2.1 Release Artifact Rollback

1. If a binary release is found to be incorrect (e.g., GPU path accidentally enabled), maintainers **MUST**:

   * publish a new patch release with corrected artifacts, and
   * mark the previous release as deprecated in release notes.
2. Artifacts **MUST** be traceable to source revisions to enable rebuilding and verification.
3. The project **SHOULD** retain prior release artifacts for reproducibility, unless removal is required for legal/security reasons.

### 13.2.2 IR Rollback / Compatibility Handling

1. Breaking IR changes **MUST** increment IR MAJOR.
2. If an IR MINOR/PATCH change is found to have changed semantics unexpectedly:

   * the change **MUST** be reverted or corrected with a PATCH increment,
   * release notes **MUST** explain impact.
3. The harness **MUST** refuse to run unsupported IR MAJOR versions to avoid silent misinterpretation.

### 13.2.3 Scene Changes Rollback

1. Scene modifications **MUST** change the SceneHash.
2. If a scene change is found to invalidate intended semantics:

   * the project **MUST** revert the scene asset or introduce a new SceneId/versioned variant,
   * the manifest **MUST** be updated accordingly.
3. The suite **SHOULD** avoid reusing the same SceneId for materially different workloads; instead, introduce a new SceneId when intent changes.

### 13.2.4 Adapter Regression Rollback

1. If an adapter change introduces semantic drift or CPU-only violations:

   * the adapter **MUST** be fixed in a patch release, or
   * temporarily disabled in binary release builds while remaining available in source builds (if feasible).
2. Adapter metadata **MUST** include backend version/build identifiers to correlate regressions with changes.

### 13.2.5 Output Schema Rollback

1. Schema MAJOR changes **MUST** be rare and justified.
2. If a schema change breaks downstream consumers unexpectedly:

   * the project **MUST** provide a compatibility export mode (if practical), or
   * revert to the prior schema and re-issue a patch release.

## 13.3 Operational Playbooks (Local Tool Context)

1. The project **SHOULD** provide a “triage checklist” (expanded in Appendix C) that includes:

   * verify adapter CPU-only metadata,
   * verify scene hash and IR version,
   * rerun with isolation mode (if available),
   * rerun with diagnostics enabled (logging, traces) while acknowledging timing contamination.
2. For suspected measurement noise, users **SHOULD**:

   * increase repetitions,
   * increase minimum measurement duration,
   * ensure stable power/performance settings,
   * rerun on an idle system.

## Acceptance Criteria

1. The risk register identifies at least the major risks relevant to this project: semantics, CPU-only enforcement, measurement noise, build variability, licensing, and IR/schema evolution.
2. Each risk includes mitigations and detection approaches that are implementable.
3. Rollback strategies are defined for releases, IR, scenes, adapters, and schema changes, with explicit versioning implications.
4. The chapter aligns rollback meaning with the local-tool distribution model (GitHub Releases and source tags).

## Dependencies

1. Chapter 5 — Data Design (hashing/versioning, SceneHash, schema versioning).
2. Chapter 9 — Security, Privacy, and Compliance (supply-chain and release integrity).
3. Chapter 11 — Deployment Architecture (release artifacts, adapter inclusion variability).

---

