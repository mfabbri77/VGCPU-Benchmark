# ADR-0005 — Self-contained HTML report generator

Status: Accepted
Date: 2026-08-30
Tracker: n/a (owner request: "generatore di pagine html — test, immagini, SSIM")

## Context

A benchmark run produces `results.json`/`results.csv`, per-(backend, scene)
PNG artifacts (`--png`, ADR-0002), and — since ADR-0004 — a correctness
census. Reading them together required a terminal, `jq`, and an image
viewer. The shipped `--compare-ssim` compares against a *golden directory*
keyed by the same `<backend>_<scene>.png` name, so it cannot express the
question the market analysis actually asks: how does every backend's render
compare against one chosen reference backend, side by side.

## Decision

1. **`tools/html_report.py`** — Python 3 stdlib only, no network, no CDN
   asset, deterministic (the only timestamp shown is the run's own),
   read-only over the benchmark outputs. It reads a results directory
   (`results.json` + PNG artifacts) and an optional correctness-census
   JSON, and emits ONE self-contained HTML file (images base64-embedded):
   - run provenance header (host, CPU, OS, compiler, policy, commit,
     schema version);
   - performance: scene x backend p50 matrix (fastest highlighted) plus a
     sorted horizontal bar chart per scene with p50/p90;
   - rendering gallery: every backend's render per scene with a
     cross-backend SSIM badge against a selectable reference backend
     (default `cairo`), and a lightbox showing the full render next to an
     amplified (x8) difference map;
   - correctness census matrix (backend x oracle case) with
     classification chips and measured-vs-expected values.
   The tool computes SSIM itself (luma composited over white, 8x8
   windows, standard C1/C2) and decodes/encodes PNG with a minimal
   stdlib-only codec — it does not re-run any benchmark or test.
2. **`VGCPU_ORACLE_JSON`** — the ADR-0004 oracle test now additionally
   writes its census as JSON when this environment variable names a
   destination file. Doctest output and pass/fail behavior are unchanged;
   unset means no file I/O.

Workflow:

```sh
vgcpu-benchmark run --all-backends --all-scenes --png --format json --out out/
VGCPU_ORACLE_JSON=out/oracle.json ./build/release/vgcpu_tests --test-suite=correctness
tools/html_report.py out/            # -> out/report.html
```

## Consequences

- One shareable file answers "who is fastest, what does each engine
  actually draw, and who is correct" — offline, no server, no build step.
- The gallery makes adapter gaps visible that timing tables hide: the
  first generated report immediately showed that several adapters render
  `fills/gradients_linear` as solid black (gradients unimplemented, e.g.
  raqote's adapter is solid-fill-only by its own comment — and cairo's
  too), which also means their timing numbers for gradient scenes measure
  drawing nothing. SSIM 1.0 between two such backends is agreement on
  nothing: the report states that SSIM measures agreement with the
  reference, not correctness (AGENTS.md correctness contract), and the
  reference is a per-run, user-selectable choice.
- Pure-Python SSIM over 60 pairs at 800x600 costs ~45 s per report.
  Acceptable for an out-of-band reporting step; not benchmark-path code.

## Alternatives rejected

- Reusing the shipped golden-dir SSIM (`--compare-ssim`) as data source:
  rejected — it answers "did this backend drift from its own golden",
  not "how do backends compare to each other"; and no golden set is
  committed.
- A JS charting library or CDN assets: rejected — the report must open
  from a file share or an air-gapped machine identically forever
  (SparkLib test-report harness discipline, adopted via AGENTS.md).
- Embedding by reference (relative PNG paths) instead of base64:
  rejected as default (a single file survives being mailed or attached to
  an issue); the ~5 MiB size at current scene count is acceptable.
