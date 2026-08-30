# ADR-0002 — PNG artifacts + SSIM regression against a ground-truth backend

Status: Accepted — implemented in v0.2.0
Date: 2025-12-23
Tracker: n/a (original record: `docs/archive/legacy-governance-antigravity/cr/CR-0002_png_artifacts_and_ssim_regression.md`)

## Context

The harness measured performance but did not persist render outputs for
inspection, and had no automated way to detect that a backend's output
drifted from a chosen reference.

## Decision

Add, gated behind `--png` / `--compare-ssim`:

- an untimed, post-benchmark render pass per (scene, backend) that writes a
  PNG artifact (`stb_image_write`, vendored) to
  `<output_dir>/png/<scene>_<backend>.png`;
- an SSIM comparison (Chris Lomont's single-file MIT implementation,
  vendored) of every backend's premultiplied RGBA buffer against a
  freshly-rendered, selectable ground-truth backend, generated first for
  each scene and retained in memory for the comparison;
- CLI flags `--png`, `--compare-ssim`, `--ground-truth-backend`,
  `--ssim-threshold` (default `0.99`); `--compare-ssim` implies PNG output;
  a missing ground-truth backend fails the run before any comparison; any
  SSIM below threshold yields exit code `4` (reports and PNGs are still
  written).

Both operations are strictly out-of-band: never inside the measured
warmup/repetition loop.

## Consequences

- Report schema (JSON/CSV) gained `png_path`, `ssim_vs_ground_truth`,
  `ground_truth_backend`, `ssim_threshold`, and a `schema_version` marker.
- `third_party/stb/`, `third_party/ssim_lomont/` vendored, pinned by
  recorded SHA-256, verified in CI when
  `VGCPU_VERIFY_THIRD_PARTY_HASHES=ON`.
- SSIM is a regression detector, not a correctness proof — see the
  "Correctness contract" in `AGENTS.md` and ADR-0004: it cannot tell two
  backends apart that agree while both being wrong. It remains useful once
  a backend is already known correct on the oracle set.

## Alternatives rejected

Decoding and diffing PNGs from disk (extra dependency, extra I/O — raw
in-memory buffers are simpler and deterministic); OpenCV for SSIM (too
heavy for a header-only requirement); per-backend output subfolders instead
of a flat `<scene>_<backend>.png` naming (rejected for glob-friendliness).
