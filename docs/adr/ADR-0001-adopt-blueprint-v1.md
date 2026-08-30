# ADR-0001 — Adopt Antigravity blueprint v1.0 (historical)

Status: Superseded by ADR-0003
Date: 2025-12-23
Tracker: n/a (original record: `docs/archive/legacy-governance-antigravity/cr/CR-0001_adopt_blueprint_v1.md`)

## Context

The repository was modernized under an "Antigravity" multi-agent blueprint
process: nine versioned chapters (scope, architecture, component design,
interfaces, data design, concurrency, build/toolchain, tooling, versioning),
a CR template and directory, an append-only `decision_log.md`, and a
machine-checked `implementation_checklist.yaml`, all cross-referencing
roughly 150 stable `[REQ-*]`/`[ARCH-*]`/`[API-*]`/... IDs.

## Decision (as originally made)

Adopt the blueprint v1.0 chapter set as the source of truth for
requirements, architecture, and change governance; target product release
v0.2.0 for the first implementation pass.

## Disposition

The full original content is preserved verbatim under
`docs/archive/legacy-governance-antigravity/` (`blueprint/`, `cr/`,
`.agent/rules/follow-blueprint.md`, `COMPLIANCE_REPORT.md`). Superseded in
full by ADR-0003 (2026-08-30): the CR/blueprint apparatus itself is retired,
not merely this version of it. Stable IDs it defined remain resolvable via
`requirements/ID_INDEX.md`.
