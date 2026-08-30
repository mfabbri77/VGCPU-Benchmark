# Chapter 7 — Build & Toolchain (CMake, Dependencies, Reproducibility, CI Gates)

This chapter extends the existing v1.0 build system (CMake ≥ 3.21, centralized deps, preset-first workflow) with v1.1 requirements for PNG writing and SSIM.

## [BUILD-01] Supported toolchains (inherit)
- [DEC-BUILD-01] Inherit toolchain support, presets, and baseline flags from v1.0 blueprint.
  - Rationale: v1.1 adds only small, self-contained CPU-side features.
  - Consequences: no new compilers/SDKs required.

## [BUILD-02] New third-party dependencies (v1.1)
v1.1 introduces two “single-file” dependencies:
- [REQ-148] `stb_image_write` for PNG encoding.
- [REQ-152] a lightweight SSIM implementation library.

### [BUILD-02-01] Dependency selection
- [DEC-BUILD-20] PNG writer: vendor `stb_image_write.h` (single header) from the upstream `nothings/stb` project (version string present in header, currently `stb_image_write v1.16`).
  - Rationale: ubiquitous, stable, no transitive deps.
  - Alternatives: libpng; lodepng.
  - Consequences: PNG output size not optimal (acceptable for benchmark artifacts).
- [DEC-BUILD-21] SSIM library: vendor Chris Lomont’s single-file C++ SSIM implementation (MIT licensed) from the `ChrisLomont/SSIM` project.
  - Rationale: single-file, permissive license, focused scope, no heavyweight dependencies.
  - Alternatives: romigrou/ssim (zlib, grayscale-first); libvpx SSIM (more code, different focus); OpenCV (too heavy).
  - Consequences: we wrap it to compute SSIM on premultiplied RGBA directly by computing per-channel SSIM and aggregating.

### [BUILD-02-02] Vendoring policy (reproducibility)
- [BUILD-02-02a] Both dependencies MUST be checked into the repository under `third_party/` to avoid network fetch for normal builds (aligns with centralized reproducibility rules in the existing `cmake/vgcpu_deps.cmake`).
- [BUILD-02-02b] For traceability, each vendored file MUST include a short header comment:
  - upstream URL
  - retrieval date (YYYY-MM-DD)
  - license
  - upstream revision identifier if available (tag/commit); otherwise record a **local SHA-256** of the vendored file content.
- [TEST-70] Add a CI check that computes SHA-256 of vendored files and compares against recorded values (fails on mismatch).

## [BUILD-03] Repository layout additions (v1.1)
- [BUILD-03-01] Add directories:
  - `third_party/stb/stb_image_write.h`
  - `third_party/ssim_lomont/ssim_lomont.hpp` (exact filename is project-local; content is the vendored single-file header)
  - `src/artifacts/` (new module; see Ch3/Ch4)
- [BUILD-03-02] Add `src/artifacts/stb_image_write_impl.cpp` as the single translation unit that defines `STB_IMAGE_WRITE_IMPLEMENTATION`.

## [BUILD-04] CMake integration (implementation-ready)

### [BUILD-04-01] New internal libraries/targets
- [BUILD-04-01a] Create a small internal library target:
  - `vgcpu_artifacts` (STATIC or OBJECT; follow existing project conventions)
  - Sources:
    - `src/artifacts/png_writer.cpp`
    - `src/artifacts/stb_image_write_impl.cpp`
    - `src/artifacts/ssim_compare.cpp`
    - `src/artifacts/naming.cpp`
  - Public include dirs:
    - `include/` (project headers)
    - `third_party/` as **SYSTEM** include directory.

### [BUILD-04-02] Third-party includes and warning isolation
- [DEC-BUILD-22] Treat `third_party/` include directories as `SYSTEM` to prevent warnings from third-party headers from breaking `VGCPU_WERROR`.
  - Rationale: keep the project’s warning budget actionable.
  - Alternatives: disable Werror globally; patch upstream headers.
  - Consequences: third-party warnings may be hidden; project code still Werror-clean when enabled.

### [BUILD-04-03] stb_image_write compilation unit
- [BUILD-04-03a] `src/artifacts/stb_image_write_impl.cpp` MUST contain:
  - `#define STB_IMAGE_WRITE_IMPLEMENTATION`
  - `#include "third_party/stb/stb_image_write.h"` (via include path; actual include string may be `"stb/stb_image_write.h"` depending on include dirs)
- [BUILD-04-03b] No other TU may define `STB_IMAGE_WRITE_IMPLEMENTATION` (ODR rule).

### [BUILD-04-04] Centralized dependency metadata
- [BUILD-04-04a] Update `cmake/vgcpu_deps.cmake` to include v1.1 entries:
  - `set(VGCPU_DEP_STB_IMAGE_WRITE_VERSION "1.16")`
  - `set(VGCPU_DEP_SSIM_LOMONT_LICENSE "MIT")`
  - `set(VGCPU_DEP_SSIM_LOMONT_REV "vendored")`
  - `set(VGCPU_DEP_THIRD_PARTY_HASH_STB_IMAGE_WRITE "<sha256>")`
  - `set(VGCPU_DEP_THIRD_PARTY_HASH_SSIM_LOMONT "<sha256>")`
- [BUILD-04-04b] Add a small CMake check function (in an existing cmake module or a new `cmake/vgcpu_third_party_hashes.cmake`) to compute file SHA-256 at configure time and compare to these constants when `VGCPU_ENABLE_LINT` (or a new `VGCPU_VERIFY_THIRD_PARTY_HASHES` option) is ON.
  - Default for CI presets: ON.

## [BUILD-05] Presets and build commands (normative)
All commands are preset-based (inherit from v1.0).

- [BUILD-05-01] Configure:
  - `cmake --preset <preset-name>`
- [BUILD-05-02] Build:
  - `cmake --build --preset <preset-name>`
- [BUILD-05-03] Test:
  - `ctest --preset <preset-name> --output-on-failure`

v1.1 does not add new required presets; it adds new tests that must be included in existing CI presets.

## [BUILD-06] CI gates (v1.1 additions)
- [BUILD-06-01] Unit tests MUST include:
  - PNG write smoke test ([TEST-53-02], [TEST-55-01])
  - SSIM known-value tests ([TEST-53-03])
  - CLI validation tests ([TEST-54-01..03])
- [BUILD-06-02] Integration tests MUST include:
  - post-benchmark render separation verification ([TEST-53-04], [TEST-55-04])
  - SSIM threshold failure path exit code validation ([API-02-04], [REQ-154])
- [BUILD-06-03] Third-party hash verification check MUST run in CI ([TEST-70]).

## [BUILD-07] Packaging and notices
- [BUILD-07-01] Update `THIRD_PARTY_NOTICES.md` to include:
  - `stb_image_write` (public domain / MIT alternative text as present in upstream header)
  - `ChrisLomont/SSIM` (MIT)
- [BUILD-07-02] The notices MUST reference the vendored file paths and recorded SHA-256 values (per [BUILD-02-02b]).

## [BUILD-08] Sanitizers and hardening (inherit)
- [DEC-BUILD-23] Inherit sanitizer toggles and CI sanitizer jobs from v1.0; v1.1 adds coverage for artifact/SSIM code paths when enabled.
  - Consequences: run at least one ASan+UBSan job with `--compare-ssim` enabled on a small scene subset.

## [TEST-70] Third-party hash verification (new)
- [TEST-70-01] CMake configure-time test:
  - When `VGCPU_VERIFY_THIRD_PARTY_HASHES=ON`, configure must fail if SHA-256 of vendored files does not match constants in `cmake/vgcpu_deps.cmake`.
- [TEST-70-02] CI must enable this option in at least one preset/job.

## [DEC-BUILD-24] No image decode dependency
- [DEC-BUILD-24] SSIM comparisons operate on in-memory raw RGBA buffers produced in the same run; no PNG decoding library is added.
  - Rationale: user requirement refreshes ground-truth each run and compares directly (no cached references).
  - Alternatives: decode PNG references and compare file-to-file.
  - Consequences: SSIM is robust to filesystem differences; artifact PNGs are for human inspection and debugging.
