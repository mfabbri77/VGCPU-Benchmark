# Change Request Template — CR-XXXX (Normative)
This template defines the canonical format for **Change Requests (CRs)** used to evolve the repository’s `/blueprint` (Blueprint + checklist) before or alongside code changes.

---

## Header
- **CR ID:** `[CR-0002]`
- **Title:** PNG artifact output + SSIM regression against selectable ground-truth backend
- **Status:** Draft
- **Owner:** maintainer
- **Created:** 2025-12-23
- **Target Release:** v1.1.0
- **Related Issues/Links:** N/A
- **Scope Type:** Feature | Dependency

---

## 1. Summary
Add optional runtime generation of per-backend PNG render artifacts for each scene, and add optional SSIM-based image regression comparing all backends against a selectable “ground truth” backend.

This CR implements:
- [REQ-148] PNG outputs per backend/scene after benchmarking (not timed).
- [REQ-149] PNG encoder from 32-bit RGBA buffer using `stb_image_write`.
- [REQ-150] CLI toggle to enable/disable PNG production.
- [REQ-151] SSIM comparison feature.
- [REQ-152] Lightweight OSS SSIM library integration.
- [REQ-153] CLI parameter to select the ground truth backend.
- [REQ-154] CLI parameter for SSIM threshold; run fails when any comparison < threshold.

---

## 2. Motivation / Problem
The benchmark currently measures performance but does not:
- Persist render outputs for manual inspection or regression triage.
- Provide automated correctness/consistency checks across backends.

Adding deterministic artifact generation and SSIM-based regression enables:
- Visual debugging and CI regression detection without resorting to heavy “pixel-perfect” expectations.
- A practical validation loop when adding/optimizing backends.

---

## 3. Scope

### 3.1 In-Scope
- [REQ-148] After benchmarking a backend for a scene, generate a PNG artifact via a **separate, untimed render** (clean separation).
- [REQ-150] CLI flag to toggle PNG writing (default off).
- [REQ-151] SSIM comparisons using a **freshly generated** ground truth image each run.
- [REQ-153] CLI option selecting which backend is ground truth; fail if unavailable.
- [REQ-154] CLI option `--ssim-threshold` controlling pass/fail; meaningful default.
- Reporting: include artifact path(s) and SSIM metrics in JSON/CSV outputs.

### 3.2 Out-of-Scope (Non-Goals)
- Pixel-perfect comparisons across platforms/font stacks.
- Multi-scale SSIM or perceptual metrics beyond SSIM.
- Loading pre-existing ground truth images from disk as references (ground truth is regenerated each run).

---

## 4. Requirements & Traceability
| ID | Requirement | Notes |
|---|---|---|
| [REQ-148] | Produce PNG per backend/scene into output directory, named with scene name | Naming finalized via [DEC-SCOPE-05] |
| [REQ-149] | Encode raw 32-bit surface buffer to PNG using stb_image_write | Premultiplied RGBA buffer is encoded as-is |
| [REQ-150] | CLI parameter enables/disables PNG output | Default OFF |
| [REQ-151] | Compare PNG images against ground truth via SSIM | Computed on raw buffers; PNGs are artifacts |
| [REQ-152] | Use lightweight open-source SSIM lib | Selected: ChrisLomont/SSIM (MIT, single-header)  |
| [REQ-153] | CLI parameter selects ground truth backend (e.g., skia) | Fail if unavailable |
| [REQ-154] | CLI parameter sets SSIM threshold; fail when below | Default chosen in [DEC-ARCH-14] |

---

## 5. Design Overview

### 5.1 CLI Additions
Add the following options (names subject to final blueprint update):
- `--png` (bool, default: false): enable PNG artifact output.
- `--compare-ssim` (bool, default: false): enable SSIM comparisons.
- `--ground-truth-backend <id>` (string, required when `--compare-ssim`): backend id used as ground truth.
- `--ssim-threshold <float>` (double, default: 0.99): fail run if any SSIM < threshold (only when compare enabled).

### 5.2 Output Naming / Layout
- Output directory: `<output_dir>/png/`
- File naming: `<scene>_<backend>.png`
  Example: `/results/png/test_rect_skia.png`
  (No per-backend subfolders.)
  Decision recorded as [DEC-SCOPE-05].

### 5.3 PNG Generation Timing Policy
- Do **not** write PNGs during measured iterations.
- After benchmark timings are computed for a case, perform an explicit untimed render pass (same scene+surface config), then encode+write PNG.
- Decision recorded as [DEC-ARCH-11].

### 5.4 SSIM “Comparison Phase” Policy
When `--compare-ssim` is enabled:
1) For each scene:
   - Generate ground truth image first (fresh each run):
     - Benchmark ground-truth backend normally.
     - After timing, do untimed render, write PNG, keep the raw buffer in memory for comparisons.
   - For each other backend:
     - Benchmark normally.
     - After timing, do untimed render, write PNG, compute SSIM vs ground truth raw buffer.
2) If specified ground truth backend is missing/disabled: **fail** immediately.
   Decision recorded as [DEC-ARCH-12].

### 5.5 SSIM Computation Contract (RGBA premultiplied)
- Inputs: same width/height; 4 channels; 8-bit per channel; premultiplied RGBA compared directly.
- Compute SSIM per-channel and aggregate as arithmetic mean across 4 channels (deterministic).
- Dynamic range L = 255.
- Default threshold: 0.99 (tunable via CLI; chosen for “meaningful default” but not overly strict across backends/OSes).
- Decisions recorded as [DEC-ARCH-14] and [DEC-ARCH-15].

### 5.6 Dependencies
- Add `stb_image_write.h` (stb, permissive license) for PNG encoding.
- Add a lightweight SSIM library:
  - Selected: ChrisLomont/SSIM single-file C++ header, MIT licensed.
- Update `THIRD_PARTY_NOTICES.md` accordingly.

---

## 6. Alternatives Considered
- **Per-backend subfolders** for outputs: rejected; user wants flat folder + `<scene>_<backend>.png`. ([DEC-SCOPE-05])
- **Compare by decoding PNGs from disk**: rejected; unnecessary I/O and adds PNG decode dependency; raw-buffer comparison is simpler and deterministic.
- **Use OpenCV for SSIM**: rejected; too heavy and adds large dependency surface.

---

## 7. Compatibility / Risk Assessment
- No changes to measured-timing loops; PNG/SSIM runs out-of-band.
- New dependencies are header-only and gated behind runtime options (but still compiled in).
- Risks:
  - Platform variability: some backends produce slightly different rasterization; threshold must be adjustable.
  - Output collisions: avoided by including backend in filename.

---

## 8. Test Plan
New tests (IDs to be finalized/linked in vNEXT blueprint update):
- [TEST-49] PNG encode writes valid PNG for known buffer (smoke + dimensions).
- [TEST-50] SSIM returns ~1.0 for identical buffers and decreases for perturbations.
- [TEST-51] CLI parsing: new flags populate options correctly, invalid combinations rejected.
- [TEST-52] Integration: when PNG disabled -> no png dir; when enabled -> expected files; when SSIM enabled -> failure below threshold triggers non-zero exit.

---

## 9. Implementation Notes / Work Items (High Level)
- Extend `CliOptions` with new fields; update `CliParser::PrintHelp` and `CliParser::Parse`.
- Add `png_writer` utility (raw RGBA8 -> PNG).
- Add `ssim_compare` utility wrapper around selected SSIM implementation (RGBA premul).
- Extend harness/reporting to:
  - Generate artifacts after timing.
  - Run SSIM comparisons in the comparison phase.
  - Record artifact paths + SSIM values in results.
- Update third-party notices and any build glue.

---

## 10. Rollout / Release
- Release as v1.1.0 (feature addition; defaults OFF).
- Document new CLI options in `README.md`.
- Add `CHANGELOG.md` entry.

---

## 11. CR Completion Checklist
- [ ] Update `/blueprint/blueprint_v1.1_ch0_metadata.md`
- [ ] Update `/blueprint/blueprint_v1.1_ch1_scope.md` (new [REQ-148..REQ-154])
- [ ] Update `/blueprint/blueprint_v1.1_ch2_system_architecture.md` (artifact flow, comparison phase)
- [ ] Update `/blueprint/blueprint_v1.1_ch3_component_design.md` (PNG/SSIM components)
- [ ] Update `/blueprint/blueprint_v1.1_ch4_interfaces_api_abi.md` (new internal APIs + CLI contracts)
- [ ] Update `/blueprint/blueprint_v1.1_ch5_data_design_hotpath.md` (buffer ownership for GT)
- [ ] Update `/blueprint/blueprint_v1.1_ch6_concurrency_parallelism.md` (serialize artifact writes)
- [ ] Update `/blueprint/blueprint_v1.1_ch7_build_toolchain.md` (vendored headers + notices)
- [ ] Update `/blueprint/blueprint_v1.1_ch8_tooling.md` (if any; otherwise inherit)
- [ ] Update `/blueprint/blueprint_v1.1_ch9_versioning_lifecycle.md` (SemVer v1.1.0, regression policy)
- [ ] Append new decisions to `/blueprint/decision_log.md` ([DEC-SCOPE-05], [DEC-ARCH-11], [DEC-ARCH-12], [DEC-ARCH-14], [DEC-ARCH-15])
- [ ] Update `/blueprint/walkthrough.md`
- [ ] Update `/blueprint/implementation_checklist.yaml`
- [ ] Update `CHANGELOG.md`
- [ ] Update `THIRD_PARTY_NOTICES.md`
- [ ] Add/update tests per §8
- [ ] Ensure **no** `[TEMP-DBG]` markers remain
- [ ] All CI quality gates pass (format/lint/sanitizers/tests/bench thresholds)

---

## Notes
- Keep CRs small when possible; this CR is feature-scoped but touches CLI, harness, reporting, and deps.
- If implementation reveals large API shifts, split into an additional CR.
