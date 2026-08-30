# Upgrade Intake — VGCPU-Benchmark (vNEXT)

## [META-06] Intake Metadata
- [META-06] Intake mode: **C (upgrade/delta)** — repository already contains a complete `/blueprint/` set.
- [META-07] Observed blueprint version in repo: **v1.0** (`/blueprint/blueprint_v1.0_ch0_metadata.md` … `ch9_versioning_lifecycle.md`).
- [META-08] Observed core targets: `vgcpu-benchmark` and `vgcpu_tests` (`/CMakeLists.txt`).
- [META-09] Date (local): **2025-12-23** (Europe/Rome).
- [META-10] Proposed next version (inferred, non-breaking feature add): **v1.1.0**. (Marking as inferred until confirmed.)
  - Inference rationale: adds optional CLI flags + optional outputs; no existing CLI flag removal implied. See [REQ-148..REQ-156].

## [SCOPE-10] Observed Facts (Grounded)
- [SCOPE-10] CLI parsing is implemented in `src/cli/cli_parser.{h,cpp}` with flags including `--backend`, `--scene`, `--out`, `--format`, etc. (`CliOptions` exists and is extended by adding new fields).
- [SCOPE-11] Backend adapter API exposes `virtual Status Render(const PreparedScene&, const SurfaceConfig&, std::vector<uint8_t>& output_buffer)` producing **RGBA8 premultiplied** output (`src/adapters/adapter_interface.h`).
- [SCOPE-12] Benchmark harness prepares a surface config and reuses a buffer (vector) across iterations (`src/harness/harness.cpp`).
- [SCOPE-13] Results reporting exists for JSON/CSV in `src/reporting/*` and uses an output directory (`CliOptions::output_dir`).
- [SCOPE-14] Backend enablement is controlled via CMake options (e.g., `ENABLE_SKIA`, `ENABLE_CAIRO`, `ENABLE_BLEND2D`, `ENABLE_RAQOTE`, `ENABLE_VELLO`, `ENABLE_AGG`, etc.) in `cmake/vgcpu_options.cmake`, with compile defs forwarded in `/CMakeLists.txt`.

## [SCOPE-15] Compliance Snapshot (v1.0 Blueprint)
- [SCOPE-15] Present and populated: `/blueprint/blueprint_v1.0_ch0..ch9.md`, `/blueprint/decision_log.md`, `/blueprint/walkthrough.md`, `/blueprint/implementation_checklist.yaml`, `/cr/` directory.
- [SCOPE-16] Repo layout matches blueprint conventions: `/src`, `/include`, `/tests`, `/tools`, `/docs`, etc.

## [REQ-148] Delta Summary (Requested Features)
### PNG output (stb_image_write)
- [REQ-148] Add runtime feature: after benchmarking each **scene** per **backend**, produce a **PNG rendering** into the output directory. Filename must include **scene name** (and must remain collision-free when multiple backends are enabled).
- [REQ-149] Implement a function that takes the **32-bit surface raw buffer** (observed runtime buffer is `RGBA8` in `std::vector<uint8_t>`) and encodes it to **PNG** using **stb_image_write**.

### CLI toggle
- [REQ-150] Add CLI parameter to enable/disable PNG production.

### Image comparison (SSIM)
- [REQ-151] Add feature to compare produced PNG images with a “ground truth” reference using **SSIM** (Structural Similarity Index).
- [REQ-152] Choose a lightweight open-source SSIM library and integrate it (pin version, include license/notice updates).

### Ground truth selection
- [REQ-153] Add CLI parameter to specify the **ground truth backend** (e.g., “Skia is ground truth”) and compare other backends’ images against it.

## [ARCH-18] Impact Matrix (What Changes Where)
| Area | Expected Impact | Primary Files/Dirs (Observed) |
|---|---|---|
| CLI | Add flags + plumb options | `src/cli/cli_parser.{h,cpp}`, `src/cli/main.cpp` |
| Harness | Capture a “post-benchmark” render buffer + call PNG writer + optionally SSIM | `src/harness/harness.{h,cpp}` |
| Reporting/Artifacts | Define output paths; record per-backend per-scene artifact paths + SSIM metrics in JSON/CSV | `src/reporting/*` |
| Common utilities | Add PNG encode helper and image load helper (for SSIM compare) | likely `src/common/*` + new headers in `/include` |
| Build/Deps | Add stb_image_write; add SSIM library; update 3rd-party notices | `cmake/vgcpu_deps.cmake` (or vendored headers), `THIRD_PARTY_NOTICES.md` |
| Tests | Add unit tests for PNG writing + SSIM compare; add golden/ground-truth fixtures | `/tests/*`, `/assets/*` (if fixtures live there) |
| Perf hygiene | Ensure PNG/SSIM are strictly optional and don’t pollute benchmark timings | harness + CLI policies |

## [DEC-UPG-01] Key Design/Policy Decisions to Make in vNEXT Blueprint
- [DEC-UPG-01] **When** to render PNG: reuse the last benchmark render buffer vs do an explicit post-benchmark render (avoids affecting timings; explicit render is safer for determinism).
- [DEC-UPG-02] Output layout: `out/renders/<scene>.png` vs `out/renders/<backend>/<scene>.png` (needed to avoid collisions and support ground truth selection cleanly).
- [DEC-UPG-03] SSIM comparison policy: threshold (pass/fail), reporting only, and whether it affects process exit code / CI gates.
- [DEC-UPG-04] Color management assumptions: treat buffers as linear vs sRGB; premultiplied alpha handling before SSIM.

## [SCOPE-17] Gaps / Risks / Edge Cases
- [SCOPE-17] **Pixel format ambiguity**: user mentions “32-bit surface raw buffer,” while adapters currently output `RGBA8 premultiplied` in `std::vector<uint8_t>`. Need an explicit contract for channel order, alpha premultiplication, and row stride before encoding and SSIM. (Will be formalized in vNEXT API + tests.)
- [SCOPE-18] **Ground truth backend availability**: the requested ground truth backend may be disabled at build time; CLI must validate and fail clearly (or auto-skip).
- [SCOPE-19] **Determinism**: some backends can differ subtly across OS/font rasterizers; SSIM thresholds must account for this (or comparisons must be opt-in).
- [SCOPE-20] **Performance contamination**: PNG encode and SSIM must not run inside timed benchmark loops; they must run out-of-band (post timing) and be toggled off by default.
- [SCOPE-21] **I/O and threading**: multi-threaded benchmarks shouldn’t race output files; artifact writing must be serialized or use unique per-case paths.

## [TEST-49] New Test Requirements (Draft)
- [TEST-49] PNG encode produces a valid PNG with correct dimensions and byte-for-byte stable output for a fixed buffer.
- [TEST-50] SSIM comparator returns ~1.0 for identical images and <1.0 for perturbed images.
- [TEST-51] CLI parsing: `--png on/off`, `--ground-truth <backend>`, and `--ssim` (if introduced) map correctly into `CliOptions`.
- [TEST-52] Harness integration: when PNG disabled, no PNGs are written; when enabled, expected paths are created.

## [SCOPE-22] Sharp Questions (Need Answers Before CR/Blueprint Updates)
1) [SCOPE-22] **Output naming**: Do you want filenames to be only `<scene>.png` (risk collisions) or `<backend>__<scene>.png` / subfolders per backend?
2) [SCOPE-23] **When to render**: Should the PNG be from the **last timed iteration buffer** (fast) or from a **separate post-benchmark render** (clean separation from timing)?
3) [SCOPE-24] **SSIM policy**: Do you want SSIM to be **reported only**, or should it **fail the run** below a threshold? If failing, what default threshold?
4) [SCOPE-25] **Ground truth behavior**: If the specified ground truth backend is not enabled/available, should the run **fail**, or **skip comparisons** with a warning?
5) [SCOPE-26] **Image load path**: Should SSIM compare against the **freshly produced PNG** of ground truth from the same run, or against **existing PNGs** in the output directory (cached “reference set”)?
6) [SCOPE-27] **Alpha handling**: For SSIM, should we compare premultiplied RGBA directly, or un-premultiply / ignore alpha and compare RGB only?

(Next step after answers: create CR(s) and update vNEXT blueprint chapters + decision log + walkthrough + checklist.)
