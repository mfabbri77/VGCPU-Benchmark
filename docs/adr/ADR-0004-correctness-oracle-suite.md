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

## Update 2026-08-30 — full 11-backend run, `release` preset

All optional backends were built and the census re-run
(`cmake --preset release`, all `ENABLE_*` default ON). Two corrections to
the claims above:

- **"No test code changes needed" was wrong.** `tests/test_main.cpp`
  registered only `null`/`plutovg`/`blend2d` regardless of which backends
  the build compiled in, so every existing test using
  `AdapterRegistry::GetAdapterIds()` — this oracle included — was silently
  Tier-1-only even in a full build. Fixed by mirroring
  `src/cli/main.cpp`'s registration list in `test_main.cpp`, gated by the
  same `VGCPU_ENABLE_*` macros (already propagated PUBLIC from
  `vgcpu_core`); the redundant Tier-1-only `target_compile_definitions`
  block on the `vgcpu_tests` target was removed as misleading dead weight.
- **Dependency pins were stale enough to block configure entirely**,
  unrelated to this ADR but discovered while getting the full build
  running: `VGCPU_DEP_AMANITHVG_COMMIT` pointed at a commit no longer
  reachable upstream (history rewritten); `VGCPU_DEP_RUST_TOOLCHAIN` still
  said `nightly` after the toolchain moved to stable, so Corrosion looked
  for a nonexistent rustup toolchain; Corrosion `v0.5.0` cannot parse
  `rustup >= 1.28.0`'s `toolchain list --verbose` output
  (corrosion-rs#590); the stable pin needed bumping to 1.86.0 because
  `vello_cpu` 0.0.4 needs `edition2024` (stable since 1.85.0) and declares
  an MSRV of 1.86.0. All four fixed as routine dependency-pin maintenance
  (VER-07, archived blueprint chapter 9), not architecture changes.

### Full census result

| backend | case A | case D (control) | case B | case C |
| --- | --- | --- | --- | --- |
| cairo | pass (0.502) | pass (0.604) | 0.502 — `exact-union` | 0.502 — `exact-union` |
| skia | pass (0.502) | pass (0.592) | 0.502 — `exact-union` | 0.502 — `exact-union` |
| blend2d | pass (0.498) | pass (0.596) | 1.000 — `sum-then-saturate` | 0.698 — `sum-then-saturate` |
| plutovg | pass (0.502) | pass (0.596) | 1.000 — `sum-then-saturate` | 0.706 — `sum-then-saturate` |
| qt | pass (0.502) | pass (0.596) | 1.000 — `sum-then-saturate` | 0.706 — `sum-then-saturate` |
| agg | pass (0.502) | pass (0.604) | 1.000 — `sum-then-saturate` | 0.702 — `sum-then-saturate` |
| vello | pass (0.502) | pass (0.600) | 1.000 — `sum-then-saturate` | 0.702 — `sum-then-saturate` |
| amanithvg | pass (0.498) | **FAIL** (0.624, expected 0.600) | 0.498 — `exact-union` | 0.498 — `exact-union` |
| raqote | pass (0.502) | **FAIL** (0.502, expected 0.600) | 0.502 — `exact-union`* | 0.502 — `exact-union`* |
| thorvg | **FAIL** (1.000, expected 0.500) | pass (0.596) | 1.000 — `sum-then-saturate` | 0.706 — `sum-then-saturate` |

`*` flagged, see below — the classification is arithmetically correct but
the reading is suspect.

### Confirmed

- 7 of 10 non-null backends (blend2d, plutovg, qt, agg, vello, and now
  **thorvg**) are `sum-then-saturate`. 3 are `exact-union` (cairo, skia,
  amanithvg on cases B/C specifically).

### Open follow-ups (not yet root-caused, not certified as findings)

- **AmanithVG case D**: 0.624 vs 0.600 expected is a genuine miss against
  this oracle's `kEpsilon` (3/255 ≈ 0.012), but is consistent with
  AmanithVG's own documented 1/16 sub-pixel lattice on axis-aligned edges
  (source document §4.3): snapping 70.3/70.6 to the nearest 1/16 gives
  70.3125/70.625, i.e. a union of 0.625 pixel-widths — matching the
  0.624 measurement almost exactly. This oracle's epsilon assumes
  near-continuous precision and was not calibrated for a backend with a
  coarser, documented lattice; not a defect, but the test does not yet
  encode that distinction and will keep flagging it. Fix belongs in the
  test (either a per-backend tolerance or a lattice-aware expected value),
  tracked, not applied yet.
- **Raqote cases B, C, D all read back identical to case A's pixel
  value** (0.501961, byte-exact). That is not the signature of a
  sum-then-saturate or an exact-union backend; it looks like
  `raqote_adapter.cpp`/`raqote_ffi` only renders the first `FillPath` call
  per surface, or `rqt_get_pixels` reads a stale/wrong region. Needs
  adapter-level (possibly Rust FFI-level) debugging, not covered here.
  Until resolved, raqote's classification above is not trustworthy for
  cases B/C/D.
- **ThorVG case A measures 1.000 (fully opaque) instead of ~0.5**, despite
  case D (also a two-contour, no-overlap case) reading correctly at
  0.596 and cases B/C showing graduated (non-binary) values. A single
  isolated rectangle rendering with no antialiasing while more complex
  paths in the same run get antialiased is inconsistent with "AA is off"
  as an explanation; needs `thorvg_adapter.cpp`-level investigation
  (possibly a ThorVG API call ordering or shape-flag issue specific to a
  lone, small shape).

These three are recorded here rather than silently fixed or silently
ignored, per AGENTS.md's "Work and evidence": a failing control case is
either a real backend defect or an oracle-calibration bug, and this run
does not yet have enough evidence to tell which for two of the three.

