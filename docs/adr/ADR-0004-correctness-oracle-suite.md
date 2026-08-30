# ADR-0004 — Correctness oracle suite: self-overlap coverage

Status: Accepted
Date: 2026-08-30
Tracker: n/a (continuation of the market-analysis benchmark work)

## Context

`Analisi_Strategica_Mercato_2D.md` (sibling `Analisi_mercato` project, §4.1)
measured, with an ad hoc 30-line test outside this repository, that two
modern CPU rasterizers (Blend2D, Vello) compute the coverage of
self-overlapping fill-nonzero subpaths by summing each subpath's coverage
independently and saturating at 1.0, instead of computing the coverage of
the geometric union — producing up to 2x-to-3x errors on shared edges
(measured: 0.098-0.500 absolute error). cairo, Skia, and AmanithVG SRE were
shown correct on the same test. That test was never made reproducible
inside VGCPU-Benchmark, and PlutoVG, ThorVG, AGG, Qt, and Raqote were never
run against it (item 9.3 of the analysis document's roadmap: "complete the
correctness census").

ADR-0002's SSIM regression cannot answer this question by itself (AGENTS.md,
"Correctness contract"): it detects drift against a chosen ground truth, but
if the ground truth backend is itself wrong, or if two backends share the
same defect, SSIM reports agreement on a wrong answer.

## Decision

Add `tests/test_correctness_oracle.cpp`, a permanent, portable oracle test
that:

1. Builds one `PreparedScene` directly (no `.irbin` round-trip, following
   the existing `IrLoader::CreateTestScene` construction pattern) containing
   four independent single-drawcall cases, each one `Path` with one or two
   rectangle contours filled once with `FillRule::kNonZero`:
   - **A** (single rect, baseline AA): one rectangle, 50% pixel coverage.
   - **B** (coincident self-overlap): two identical contours; union coverage
     is unchanged at 50%; a sum-then-saturate implementation reports 100%.
   - **C** (partial overlap): two different contours, 40% and 30% wide with
     a 20% overlap; union = 50%, naive independent sum = 70% — picked below
     saturation so the sum-vs-union failure mode is visible on its own.
   - **D** (adjacent, no overlap; control): two touching, non-overlapping
     contours, 30% and 30%; union = naive sum = 60%, a single correct answer
     regardless of union-vs-sum implementation choice.
2. Iterates every adapter the running binary was built with (via
   `AdapterRegistry`, excluding `null`, which does not render scene
   content), renders the scene, and samples the alpha channel at one pixel
   per case.
3. Treats case A and case D as hard requirements (`CHECK`, fails the
   suite): they have exactly one correct answer independent of the
   union-vs-sum architectural question, so a miss there is a genuine bug,
   not an architectural disagreement.
4. Treats case B and case C as a **census, not a gate** (`MESSAGE`,
   classifies `exact-union` / `sum-then-saturate` / `other-divergence`,
   never fails the suite): the sum-then-saturate behavior in a third-party
   backend is a documented, unfixable-by-us architectural fact, not a
   regression this repository can resolve (AGENTS.md, "Correctness
   contract" and "Work and evidence").

## Result (this host, Tier-1 build: `plutovg`, `blend2d`)

| backend | case A | case D (control) | case B | case C |
| --- | --- | --- | --- | --- |
| blend2d | 0.498 (pass) | 0.596 (pass) | 1.000 — `sum-then-saturate` | 0.698 — `sum-then-saturate` |
| plutovg | 0.502 (pass) | 0.596 (pass) | 1.000 — `sum-then-saturate` | 0.706 — `sum-then-saturate` |

Both wired Tier-1 real rasterizers reproduce the sum-then-saturate defect
exactly as predicted (case B lands on the saturated value 1.0; case C lands
on the uncapped naive-sum prediction 0.7, not on the union value 0.5).
**New finding beyond the source analysis document**: PlutoVG was not part
of the original four-backend census and exhibits the same defect family as
Blend2D. Case A and case D agree with the hand-derived expected value to
within 1-2/255 for both backends, which is the discriminating evidence that
the oracle geometry itself is correct — if it were not, a coordinate or
encoding error would show up as a case A/D failure, not just a case B/C
classification.

Full backend coverage (cairo, Skia, ThorVG, AGG, Qt, AmanithVG SRE, Raqote,
Vello CPU) requires a build with those adapters enabled; the test runs
against whichever adapters `AdapterRegistry` reports at run time, so no
further code change is needed to extend the census once those adapters are
built in CI or locally — closing the "portable to whichever engine you wire
in" requirement from the source document's §8.3 (the test as the asset,
not a slide).

## Consequences

- `tests/test_correctness_oracle.cpp` runs under `ctest --preset ci`
  (Tier-1) and every other preset; it never blocks a release over a
  third-party backend's known architecture, only over a genuine regression
  on cases A/D.
- The market-analysis document's §4.1 finding is now backed by a
  reproducible, permanent, in-repo measurement, not only an external ad hoc
  script; the PlutoVG result is new evidence for that document's
  correctness census.
- Extending the census to FreeType, Qt raster (native, not via Firefox),
  ThorVG, and tiny-skia only requires enabling those adapters in a build;
  no test code changes.

## Alternatives rejected

- Reusing the source document's exact four numeric cases (0.604/0.502/
  0.800/0.502): rejected — those values came from a specific external
  geometry this repository does not have access to. A from-first-principles
  oracle with hand-verifiable expected values (rectangle areas) is
  reproducible and auditable by anyone reading the test, and case B alone
  already reproduces the source document's sharpest finding (coincident
  subpaths, correct=0.5, sum-then-saturate=1.0) almost exactly.
- Hard-failing CI when a backend is `sum-then-saturate`: rejected — every
  wired real rasterizer in this Tier-1 build currently has that
  architecture; gating on it would make CI permanently red for a fact nobody
  here can fix, which is exactly the failure mode AGENTS.md's "Work and
  evidence" section warns against (a predicate that only ever fires is not
  evidence of a regression).
