# ADR-0003 — Adopt lightweight, OMP-native governance; retire the Antigravity blueprint/CR apparatus

Status: Accepted
Date: 2026-08-30
Tracker: n/a (repository owner request)

## Context

VGCPU-Benchmark was modernized under an "Antigravity" multi-agent blueprint
process (ADR-0001): nine versioned chapters per blueprint release, a CR
template and directory, an append-only `decision_log.md`, and a
machine-checked `implementation_checklist.yaml`, all cross-referencing
roughly 150 stable IDs. It shipped one real feature under that process (PNG
artifacts + SSIM regression, ADR-0002 / v0.2.0) before stalling.

The project resumes now under a different harness (Oh My Pi, "omp") and a
different immediate goal: it is the benchmark instrument for Mazatech's
2D-engine market analysis (`Analisi_Strategica_Mercato_2D.md`, sibling
`Analisi_mercato` project), which needs correctness and speed evidence
across the ten wired backends plus AmanithVG SRE — not a certification-grade
audit trail. SparkLib (Mazatech's MISRA-C:2023 rendering kernel, same owner)
already runs a proportionate governance model for a different problem:
`AGENTS.md` as the single always-active contract, ADRs for architecture
decisions, StrictDoc for MISRA-traceable requirements, a `.omp/` harness
with model routing and subagents, a repo-layout-manifest audit, and a Gitea
issue-tracker wrapper. Porting that apparatus verbatim would add machinery
built for a DO-178C/MISRA supplier with a larger contributor base than this
repository has, to a repository that wraps and measures third-party
libraries rather than shipping a certifiable one.

## Decision

1. Retire the blueprint/CR apparatus. Move `blueprint/` (v1.0 and v1.1
   chapters, `decision_log.md`, `walkthrough.md`,
   `implementation_checklist.yaml`, `migration_intake.md`,
   `upgrade_intake.md`), `cr/`, `.agent/rules/follow-blueprint.md`, and
   `COMPLIANCE_REPORT.md` to `docs/archive/legacy-governance-antigravity/`
   verbatim (`git mv`, history preserved). Nothing in that tree is
   authoritative going forward; it exists only so the ~110 stable IDs
   already cited in 55 source/test/tooling files stay resolvable.
2. Adopt `AGENTS.md` at the repository root as the single always-active
   contract, in the form SparkLib uses: authority table, workbench
   ownership, product/adapter/benchmark-integrity/correctness contracts,
   stop conditions, quality-gate run-rate table — scoped to what this
   repository actually needs.
3. Adopt `docs/adr/` for architecture and governance decisions going
   forward: one file per decision, no chapter/CR/checklist triad.
   `docs/adr/README.md` is the append-only index, replacing
   `decision_log.md`.
4. Freeze `requirements/ID_INDEX.md`: a flat list of the IDs actually cited
   in current code, each pointing at its archived source chapter. New work
   does not mint new IDs in that scheme; it cites its governing ADR number
   instead.
5. Do not port SparkLib's `.omp/` npm harness, StrictDoc requirements tree,
   repo-layout-manifest audit, or Gitea wrapper. GitHub Issues/PRs are this
   repository's tracker; CMake presets + CI are already the build/layout
   authority, with no multi-agent path collision to audit.

## What is kept unchanged

- CMake presets, third-party dependency pinning, the TEMP-DBG marker
  policy, format/lint/sanitizer gates, and report schema versioning: all
  stay in force, they were sound independent of which governance ceremony
  produced them.
- The PNG/SSIM correctness-artifact feature (ADR-0002) is unaffected code;
  only its governance record moved.
- Every stable `[REQ-*]`/`[ARCH-*]`/... identifier already in code comments
  remains valid and unrenumbered (`requirements/ID_INDEX.md`).

## Consequences

- A future architecture or contract change needs one ADR, not a CR plus
  nine chapter edits plus a checklist update.
- Contributors and agents read one file (`AGENTS.md`) instead of
  bootstrapping a 20-file blueprint set before the first change.
- `README.md`, `docs/QUICKSTART.md`, `docs/MIGRATION_v0.2.0.md`, and
  `.github/workflows/*.yml` are updated to point at the new governance
  surface instead of `blueprint/`/`cr/`.
- The next substantive change under this ADR is the correctness-census
  extension (self-overlap coverage oracle, conflation oracle, sub-pixel
  model census) tracked in ADR-0004.

## Alternatives rejected

- Full SparkLib-style port (StrictDoc + `.omp/` harness + layout-manifest
  audit + Gitea wrapper): rejected as disproportionate; this repository has
  no multi-agent path-collision problem to solve and no MISRA/DO-178C
  traceability obligation.
- Keep the blueprint/CR apparatus and simply stop maintaining it: rejected;
  an unmaintained governance tree still reads as authoritative to a new
  contributor, which is worse than none.
- Delete the old blueprint/CR files outright: rejected; ~110 IDs are
  load-bearing in existing code comments and CI/tooling headers — deleting
  the archive would strand every one of them.
