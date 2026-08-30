# Architecture Decision Records — VGCPU-Benchmark

Append-only index. Each decision is one file, numbered sequentially,
never renumbered. A superseding decision links back to the one it replaces;
the superseded file's `Status` line is updated in place (the record stays,
only its status changes — this index is append-only, individual files are
not silently deleted).

Template for a new entry:

```markdown
# ADR-XXXX — <short title>

Status: Proposed | Accepted | Superseded by ADR-YYYY
Date: YYYY-MM-DD
Tracker: <issue/PR link, or n/a>

## Context
## Decision
## Consequences
## Alternatives rejected
```

## Index

| ADR | Title | Status |
| --- | --- | --- |
| [ADR-0001](ADR-0001-adopt-blueprint-v1.md) | Adopt Antigravity blueprint v1.0 (historical) | Superseded by ADR-0003 |
| [ADR-0002](ADR-0002-png-artifacts-ssim-regression.md) | PNG artifacts + SSIM regression against a ground-truth backend | Accepted — implemented in v0.2.0 |
| [ADR-0003](ADR-0003-adopt-lightweight-omp-native-governance.md) | Adopt lightweight, OMP-native governance; retire the blueprint/CR apparatus | Accepted |
| [ADR-0004](ADR-0004-correctness-oracle-suite.md) | Correctness oracle suite: self-overlap coverage, conflation, sub-pixel census | Accepted |
