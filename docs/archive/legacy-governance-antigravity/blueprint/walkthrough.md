FILE: /blueprint/walkthrough.md

```markdown
# Walkthrough — Implementation Plan (VGCPU-Benchmark)

## 1) Overview
- **Blueprint Version:** v1.1
- **Target Product Release:** v0.2.0 ([DEC-VER-01])
- **Repo Name:** VGCPU-Benchmark
- **Primary Goal:** Add optional PNG artifact generation and SSIM-based regression against a selectable ground-truth backend, without contaminating benchmark timings. ([REQ-148..REQ-154], [DEC-ARCH-11])
- **Driving CR:** [CR-0002] PNG artifact output + SSIM regression

### Quality Gates (must pass)
- Formatting/lint gates per existing v1.0 tooling ([DEC-TOOL-01])
- No `[TEMP-DBG]` markers committed (see `temp_dbg_policy.md`)
- Unit + integration tests include new artifact/SSIM paths ([TEST-53], [TEST-54], [TEST-55], [TEST-56], [TEST-70..TEST-72])
- Sanitizers cover PNG + SSIM enabled runs ([TOOL-03-01])
- Vendored third-party integrity verification in CI ([REQ-155], [TEST-70], [DEC-TOOL-08])

### How to Run (Quickstart)
> Use the repo’s existing presets. If unsure: `cmake --list-presets`.

- Configure: `cmake --preset <dev>`
- Build: `cmake --build --preset <dev>`
- Tests: `ctest --preset <dev> --output-on-failure`
- Example correctness run (SSIM implies PNG):
  `vgcpu-benchmark --out <dir> --compare-ssim --ground-truth-backend skia --ssim-threshold 0.99` ([API-03-01..04], [DEC-API-07])

---

## 2) Phases
Phases are ordered to keep the project buildable and testable at every step. Each step includes:
- **Refs:** stable IDs from the blueprint (implementers cite these IDs in code comments)
- **Artifacts:** which files/dirs change
- **Commands:** runnable build/test commands
- **DoD:** “definition of done” for the step

---

## 3) Phase 0 — Repo Bootstrap & Tooling

### P0.S1 — Add third_party vendored dependencies
- **Refs:** [REQ-149], [REQ-152], [REQ-155], [DEC-BUILD-20], [DEC-BUILD-21], [DEC-TOOL-08]
- **Artifacts:**
  - `third_party/stb/stb_image_write.h`
  - `third_party/ssim_lomont/ssim_lomont.hpp` (vendored single-file SSIM implementation)
  - `THIRD_PARTY_NOTICES.md` (add entries)
- **Commands:**
  - `cmake --preset <dev>`
  - `cmake --build --preset <dev>`
- **DoD:**
  - Vendored files present with provenance header (upstream, date, license, revision/hash) ([BUILD-02-02b])
  - Notices updated
  - Build still succeeds

### P0.S2 — Wire CMake for artifacts module + hash verification
- **Refs:** [BUILD-04-01..04], [TEST-70], [DEC-BUILD-22], [DEC-BUILD-24]
- **Artifacts:**
  - `src/artifacts/stb_image_write_impl.cpp` (only TU defining `STB_IMAGE_WRITE_IMPLEMENTATION`) ([BUILD-04-03])
  - New target `vgcpu_artifacts` and link it into harness/app as needed
  - CMake updates (`cmake/vgcpu_deps.cmake` + hash verification option)
- **Commands:**
  - `cmake --preset <dev> -DVGCPU_VERIFY_THIRD_PARTY_HASHES=ON`
  - `cmake --build --preset <dev>`
- **DoD:**
  - No ODR issues with stb
  - Configure fails if vendored hashes mismatch when verification ON ([TEST-70-01])
  - Third-party includes treated as `SYSTEM` ([DEC-BUILD-22])

---

## 4) Phase 1 — Core API Skeleton (Artifacts Module)

### P1.S1 — Add internal headers for naming, PNG writing, SSIM
- **Refs:** [API-20..API-24], [MEM-10-04], [DEC-SCOPE-05]
- **Artifacts:**
  - `include/vgcpu/internal/artifacts/naming.h`
  - `include/vgcpu/internal/artifacts/png_writer.h`
  - `include/vgcpu/internal/artifacts/ssim.h`
  - `include/vgcpu/internal/artifacts/image_view.h` (helper type; internal)
- **Commands:**
  - `cmake --build --preset <dev>`
- **DoD:**
  - Headers compile everywhere; no exceptions crossing module boundaries ([REQ-53])
  - Functions are re-entrant/thread-safe by design ([API-09-01])

### P1.S2 — Extend CLI options and validation rules (no behavior yet)
- **Refs:** [REQ-150], [REQ-153], [REQ-154], [API-03-01..09], [API-02]
- **Artifacts:**
  - `src/cli/cli_parser.{h,cpp}` (`CliOptions` new fields)
  - `src/cli/main.cpp` (validation + help text)
- **Commands:**
  - `cmake --build --preset <dev>`
  - `ctest --preset <dev> --output-on-failure` (existing CLI tests, if any)
- **DoD:**
  - New flags parse and validate:
    - missing `--ground-truth-backend` when `--compare-ssim` => exit code 2 ([TEST-54-02])
    - threshold out of range => exit code 2 ([TEST-54-01])
  - `--compare-ssim` implies PNG artifacts internally (documented in `--help`) ([DEC-API-07])

---

## 5) Phase 2 — Core Implementation (Hot Path + Post-benchmark Work)

### P2.S1 — Implement deterministic naming + filesystem-safe paths
- **Refs:** [API-22..API-23], [MEM-10-04], [DEC-SCOPE-05]
- **Artifacts:**
  - `src/artifacts/naming.cpp`
- **Commands:**
  - `ctest --preset <dev> --output-on-failure`
- **DoD:**
  - Sanitization rules implemented exactly (allowed charset, collapse `_`, trim, truncation+hash) ([MEM-10-04])
  - Unit tests for edge cases: spaces, unicode bytes, very long names ([TEST-53-01])

### P2.S2 — Implement PNG writer (stb_image_write)
- **Refs:** [REQ-149], [API-20], [DEC-BUILD-20], [DEC-ARCH-15]
- **Artifacts:**
  - `src/artifacts/png_writer.cpp`
  - `src/artifacts/stb_image_write_impl.cpp`
- **Commands:**
  - `ctest --preset <dev> --output-on-failure`
- **DoD:**
  - Writes `<output_dir>/png/<scene>_<backend>.png` using `stbi_write_png`
  - Fail-fast on write/encode errors when PNG enabled ([DEC-ARCH-15])
  - Unit test validates PNG signature and dimensions ([TEST-53-02])

### P2.S3 — Implement SSIM comparator wrapper (RGBA8 premul)
- **Refs:** [REQ-151], [REQ-152], [API-21], [DEC-MEM-11], [DEC-ARCH-13]
- **Artifacts:**
  - `src/artifacts/ssim_compare.cpp` (+ vendored header usage)
- **Commands:**
  - `ctest --preset <dev> --output-on-failure`
- **DoD:**
  - Computes per-channel SSIM and deterministic aggregate mean ([API-21-03])
  - Unit tests:
    - identical buffers => ~1.0
    - perturbed buffers => lower
    - dimension mismatch => invalid-argument result ([TEST-53-03], [TEST-55-03])

### P2.S4 — Harness integration: post-benchmark render + PNG artifacts (no SSIM yet)
- **Refs:** [REQ-148], [REQ-150], [DEC-ARCH-11], [MEM-03-01], [MEM-09-01]
- **Artifacts:**
  - `src/harness/harness.cpp` (or equivalent harness entry)
  - link `vgcpu_artifacts`
- **Commands:**
  - `vgcpu-benchmark --out <dir> --png ...`
- **DoD:**
  - PNG generation performed only in post-benchmark phase; never in measured loop ([TEST-55-04])
  - Creates `<output_dir>/png/` only when needed ([ARCH-08-02])
  - Writes one PNG per (scene, backend) with required naming

### P2.S5 — Harness integration: SSIM comparison phase (ground truth first)
- **Refs:** [REQ-151..REQ-154], [DEC-ARCH-12], [ARCH-04-02], [MEM-03-02], [API-02-04]
- **Artifacts:**
  - `src/harness/harness.cpp` (per-scene two-step ordering)
  - error/exit-code plumbing to distinguish regression failures
- **Commands:**
  - `vgcpu-benchmark --out <dir> --compare-ssim --ground-truth-backend skia --ssim-threshold 0.99 ...`
- **DoD:**
  - For each scene: GT backend benchmarked and rendered first, GT buffer retained, then other backends compared ([ARCH-04-02])
  - Missing GT backend fails early ([DEC-ARCH-12], [TEST-54-03])
  - Any SSIM < threshold => exit code 4, reports + PNGs still written ([API-02-04], [TEST-72-03])

### P2.S6 — Reporting schema extensions (PNG path + SSIM metrics)
- **Refs:** [API-07-01..04], [DEC-SCOPE-03], [VER-15-01]
- **Artifacts:**
  - `src/reporting/*.cpp` (JSON + CSV writers)
- **Commands:**
  - Run one sample and inspect outputs in `<output_dir>`
- **DoD:**
  - JSON includes `schema_version` and new fields when enabled
  - CSV includes schema marker and new columns
  - Report rows include `png_path` and SSIM metadata when applicable

---

## 6) Phase 3 — Backends (Vulkan/Metal/DX12) (If applicable)
- **DEC:** N/A for this project version ([DEC-ARCH-16], [REQ-07]).
- **DoD:** No GPU backend work is performed.

---

## 7) Phase 4 — Python Bindings (If requested)
- **DEC:** N/A (no bindings requested).
- **DoD:** No binding artifacts are created.

---

## 8) Phase 5 — Hardening, Sanitizers, and CI

### P5.S1 — Add/extend tests for CLI, artifacts, harness ordering, and regression exit codes
- **Refs:** [TEST-53..TEST-56], [TEST-72-03]
- **Artifacts:**
  - `tests/test_artifacts_png.cpp`
  - `tests/test_ssim.cpp`
  - `tests/test_cli_options.cpp`
  - `tests/test_harness_artifacts_ssim.cpp` (integration)
- **Commands:**
  - `ctest --preset <dev> --output-on-failure`
- **DoD:**
  - All new tests pass locally and in CI presets
  - Tests assert separation from measured loop (counters/hooks) ([MEM-09-02])

### P5.S2 — Enable sanitizer and third-party hash verification coverage
- **Refs:** [TOOL-03-01], [TEST-70], [TEST-71]
- **Artifacts:**
  - CI config updates (existing CI system), preset toggles
- **Commands:**
  - `cmake --preset <asan> -DVGCPU_VERIFY_THIRD_PARTY_HASHES=ON`
  - `cmake --build --preset <asan>`
  - `ctest --preset <asan> --output-on-failure`
- **DoD:**
  - At least one CI job runs SSIM-enabled invocation under ASan/UBSan ([TOOL-03-01])
  - Hash verification gate is ON in CI and enforced ([TEST-70], [DEC-TOOL-08])

---

## 9) Phase 6 — Release Readiness

### P6.S1 — Docs, help text, and changelog
- **Refs:** [REQ-150..REQ-154], [DEC-API-07], [VER-12], [VER-15]
- **Artifacts:**
  - `README.md` (document flags + example)
  - `CHANGELOG.md` (v0.2.0 entry)
- **Commands:**
  - `vgcpu-benchmark --help` (spot check)
- **DoD:**
  - Docs explain:
    - `--png`, `--compare-ssim`, `--ground-truth-backend`, `--ssim-threshold`
    - SSIM implies PNG artifacts ([DEC-API-07])
    - Output paths and naming ([DEC-SCOPE-05])

### P6.S2 — Hygiene sweep + final gates
- **Refs:** [TEMP-DBG], [REQ-155]
- **Artifacts:** repo-wide
- **Commands:**
  - `cmake --build --preset <dev> --target format_check` (if present)
  - `ctest --preset <dev> --output-on-failure`
- **DoD:**
  - No `[TEMP-DBG]` markers remain
  - All gates pass; CI green

---

## 10) Mapping to Implementation Checklist
- [REQ-148..REQ-155] are mapped to concrete machine-executable tasks in `/blueprint/implementation_checklist.yaml`.
- Each checklist task references the relevant IDs (without brackets) and has an explicit “done when” condition.

---

## 11) Notes
- SSIM is performed on in-memory premultiplied RGBA buffers generated in the same run (no PNG decoding dependency) ([DEC-BUILD-24], [DEC-MEM-11]).
- Artifact generation is explicitly out-of-band and should be considered a correctness/debugging aid, not part of performance measurement ([DEC-ARCH-11]).
```
