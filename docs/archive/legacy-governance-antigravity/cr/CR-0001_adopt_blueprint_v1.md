# Change Request: Adopt Blueprint v1.0

| Field | Value |
|:------|:------|
| **CR ID** | CR-0001 |
| **Title** | Adopt Blueprint v1.0 for VGCPU-Benchmark Modernization |
| **Author** | VGCPU Team |
| **Date** | 2025-12-23 |
| **Status** | Approved |

## 1. Summary

Adopt the canonical blueprint v1.0 as the source of truth for the VGCPU-Benchmark project, establishing governance rules, versioning policies, and build system requirements for the v0.2.0 modernization milestone.

## 2. Motivation

The existing project documentation was ad-hoc and lacked formal governance, versioning contracts, and quality gates. The new blueprint provides:
- Stable requirement IDs ([REQ-*], [TEST-*], [DEC-*]) for traceability
- Formal SemVer rules and compatibility surface definitions
- Preset-driven CMake build system with CI gates
- Deterministic dependency pinning requirements

## 3. Proposed Change

### 3.1 Affected Components

- `/blueprint/` — Now contains canonical v1.0 chapters (ch0–ch9)
- `/docs/legacy_blueprint/` — Contains previous documentation
- `/cr/` — New directory for change request governance
- `CMakeLists.txt` — Must conform to blueprint build rules
- CI workflows — Must use presets only

### 3.2 Breaking Changes

- [x] Build system changes (CMake presets required)
- [x] Dependency pinning (no floating branches)
- [ ] CLI flags/commands (no changes)
- [ ] Report schema (additive: `schema_version` field)
- [ ] Backend names (no changes)

## 4. Alternatives Considered

| Alternative | Pros | Cons |
|:------------|:-----|:-----|
| Continue without formal blueprint | Less upfront work | Technical debt, inconsistent CI |
| Minimal governance only | Faster adoption | Missing traceability, no perf gates |

## 5. Consequences

### 5.1 Positive

- Deterministic, reproducible builds
- Traceability from requirements to code to tests
- Formal versioning for CLI and report schemas
- Performance regression detection capability

### 5.2 Negative

- Initial implementation effort for presets and gates
- Mitigation: Incremental rollout per milestones

## 6. Affected Requirements

All [REQ-*], [TEST-*], and [DEC-*] IDs from blueprint v1.0 chapters 0–9 are now binding.

## 7. Implementation Notes

Follow the walkthrough phases in `/blueprint/walkthrough.md`:
1. Phase 0: Repo normalization (move legacy blueprint, create CR scaffolding)
2. Phase 1: Build system modernization (CMakePresets.json, Tier-1 config)
3. Phase 2+: Tests, interfaces, CI, packaging, perf harness

## 8. Reviewers

- [x] Project maintainer (auto-approved for initial adoption)

---

This CR established [DEC-*] entries already recorded in `/blueprint/decision_log.md`.
