# Chapter 4 — Interfaces, API/ABI, CLI & Schema Contracts

This chapter follows the rules in:
- `cpp_api_abi_rules.md` (public surface, visibility, ABI discipline)
- `error_handling_playbook.md` (Status/Result conventions)
- `logging_observability_policy_new.md` (structured logging expectations)

## [API-01] Public compatibility surfaces (what we promise)
- [API-01-01] **CLI flags** are a compatibility surface (SemVer governed; see Ch9).
- [API-01-02] **Report schemas** (JSON/CSV) are a compatibility surface (versioned; see [DEC-SCOPE-03]).
- [API-01-03] Internal C++ symbols/headers under `vgcpu_*` are **not** a stable ABI surface ([DEC-ARCH-01]).

## [API-02] Process exit codes (CLI contract)
- [API-02-01] Exit code `0`: success (all requested actions completed; if SSIM enabled, all comparisons ≥ threshold).
- [API-02-02] Exit code `2`: CLI usage/config error (invalid flags, missing required params, unknown backend/scene).
- [API-02-03] Exit code `3`: runtime failure (backend init/render failure, I/O failure writing outputs).
- [API-02-04] Exit code `4`: SSIM regression failure (at least one comparison < threshold) — reports + artifacts still written.

## [API-03] CLI options (v1.1 additions)
### 4.1 New flags (names are normative)
- [API-03-01] `--png`
  - Type: boolean flag
  - Default: `false`
  - Meaning: enables PNG artifact generation per (scene, backend) per [REQ-148..REQ-150].
- [API-03-02] `--compare-ssim`
  - Type: boolean flag
  - Default: `false`
  - Meaning: enables SSIM comparisons per [REQ-151].
- [API-03-03] `--ground-truth-backend <id>`
  - Type: string
  - Default: empty
  - Required iff `--compare-ssim` is set ([REQ-153]).
  - Validation: must match an available adapter registry id; if not available, fail with exit code `2` (usage) or `3` (runtime) as per below.
- [API-03-04] `--ssim-threshold <float>`
  - Type: double
  - Default: `0.99` ([DEC-ARCH-14])
  - Validation: must be within `[0.0, 1.0]` inclusive; otherwise exit code `2`.
  - Meaning: fail the run when any SSIM < threshold ([REQ-154]).

### 4.2 CLI validation rules (normative)
- [API-03-05] If `--compare-ssim` is set:
  - [API-03-05a] `--ground-truth-backend` MUST be provided (non-empty).
  - [API-03-05b] The specified ground-truth backend MUST be available/enabled; otherwise the program MUST fail before comparisons begin ([DEC-ARCH-12]).
  - [API-03-05c] The ground-truth images MUST be generated fresh each run, as the first step for each scene (see Ch2/Ch3 ordering).
- [API-03-06] If `--png` is false and `--compare-ssim` is true:
  - [DEC-API-01] The program MUST still generate PNG artifacts for traceability (force-enable PNG internally).
    - Rationale: SSIM failures must be diagnosable by reading the written images.
    - Alternatives: allow SSIM without writing PNGs (harder triage).
    - Consequences: docs/help must state SSIM implies PNG artifacts regardless of `--png`.
- [API-03-07] Unknown backend ids or scene ids MUST cause exit code `2` with a list of valid values.
- [API-03-08] Scene/backend name canonicalization:
  - backend ids are **lowercase** and match `[a-z0-9._-]+` (registry enforces uniqueness; see [API-06-02]).

### 4.3 `CliOptions` struct changes (implementation-ready)
- [API-03-09] Extend `vgcpu::CliOptions` (`src/cli/cli_parser.h`) with:
  - `bool write_png = false;`
  - `bool compare_ssim = false;`
  - `std::string ground_truth_backend;`
  - `double ssim_threshold = 0.99;`

## [API-04] Render output contract (adapter → harness)
The current adapter interface is:
- `Status Render(const PreparedScene&, const SurfaceConfig&, std::vector<uint8_t>& output_buffer)`

v1.1 formalizes the buffer contract required by PNG and SSIM.

- [API-04-01] For `SurfaceConfig { width, height }`, adapters MUST output:
  - [API-04-01a] Pixel format: **RGBA8 premultiplied alpha**
  - [API-04-01b] Layout: tightly packed rows (stride = `width * 4`)
  - [API-04-01c] Size: `output_buffer.size() == width * height * 4`
- [API-04-02] Channel order is byte order `[R, G, B, A]` per pixel.
- [API-04-03] Undefined behavior is prohibited: adapter MUST resize/overwrite the buffer fully and deterministically for a given scene/config.
- [API-04-04] On failure, adapters return a non-OK `Status` and MAY leave buffer contents unspecified.

## [API-05] Artifact pipeline internal APIs (new in v1.1)
These are internal headers under `include/vgcpu/internal/` and are not a public ABI promise (per `cpp_api_abi_rules.md`).

### 4.4 Header: `include/vgcpu/internal/artifacts/png_writer.h`
- [API-20] Function:
  - `Status WritePngRGBA8Premul(const std::filesystem::path& path,`
  - `                           int width, int height,`
  - `                           int stride_bytes,`
  - `                           std::span<const std::uint8_t> pixels) noexcept;`
- [API-20-01] Preconditions:
  - `width > 0`, `height > 0`, `stride_bytes == width*4`
  - `pixels.size() >= height * stride_bytes`
- [API-20-02] Errors:
  - returns `StatusCode::kIOError` for filesystem failures,
  - returns `StatusCode::kInternal` for encoding failures.
- [API-20-03] Implementation requirement:
  - uses `stb_image_write.h` (`stbi_write_png`) with deterministic parameters.

### 4.5 Header: `include/vgcpu/internal/artifacts/ssim.h`
- [API-24] Types:
  - `struct SsimResult { double aggregate; std::array<double,4> rgba; };`
- [API-21] Function:
  - `Result<SsimResult> ComputeSSIM_RGBA8Premul(int width, int height, int stride_bytes,`
  - `                                           std::span<const std::uint8_t> a,`
  - `                                           std::span<const std::uint8_t> b) noexcept;`
- [API-21-01] Preconditions:
  - same `width/height/stride_bytes` for both inputs
  - same buffer sizes (at least `height*stride_bytes`)
- [API-21-02] Failure:
  - dimension mismatch → `StatusCode::kInvalidArgument`
  - compute error → `StatusCode::kInternal`
- [API-21-03] Contract:
  - compares **premultiplied RGBA directly** ([SCOPE-27 answer])
  - returns per-channel SSIM and aggregate mean (deterministic).

### 4.6 Header: `include/vgcpu/internal/artifacts/naming.h`
- [API-23] `std::string SanitizeName(std::string_view original);`
- [API-22] `std::filesystem::path MakePngPath(const std::filesystem::path& output_dir,`
- [API-22] `                                  std::string_view scene_name,`
- [API-22] `                                  std::string_view backend_id);`
- [API-22-01] `MakePngPath` MUST implement [DEC-SCOPE-05] (`<output_dir>/png/<scene>_<backend>.png`).

## [API-06] Adapter registry contracts (for ground truth selection)
- [API-06-01] Registry MUST provide enumeration of available backend ids for help/validation.
- [API-06-02] Backend id rules:
  - lowercase, filesystem-safe charset `[a-z0-9._-]+`
  - stable across versions unless a SemVer breaking change is declared (Ch9)
  - unique across all compiled-in adapters
- [API-06-03] `--ground-truth-backend` MUST match one of these ids; otherwise fail per [API-03-05b].

## [API-07] Report schema extensions (v1.1)
### 4.7 JSON schema (row-level fields)
- [API-07-01] Add fields per result row (scene, backend):
  - `png_path` : string (empty or omitted if PNG disabled; present when written)
  - `ssim_vs_ground_truth` : number in [0,1] (omitted for ground truth row or when SSIM disabled)
  - `ground_truth_backend` : string (present when SSIM enabled)
  - `ssim_threshold` : number (present when SSIM enabled)
- [API-07-02] Add top-level metadata fields:
  - `schema_version` : integer (increment when schema changes; see [DEC-SCOPE-03])
  - `features` : object containing `{ "png": bool, "ssim": bool }`

### 4.8 CSV schema (columns)
- [API-07-03] Add columns:
  - `png_path`
  - `ground_truth_backend`
  - `ssim_vs_ground_truth`
  - `ssim_threshold`
- [API-07-04] CSV MUST include a schema marker header line (or dedicated column) per [DEC-SCOPE-03].

## [API-08] Error handling rules (Status/Result)
(Per `error_handling_playbook.md` and existing `src/common/status.h`.)

- [REQ-53] All non-trivial operations in artifacts (write/compare/path creation) MUST return `Status` or `Result<T>` and must not throw across module boundaries.
- [API-08-01] Exceptions MAY be used internally only if caught and converted to `Status` before returning.
- [API-08-02] SSIM threshold failure is not a `Status` error; it is a **run outcome**:
  - record metrics in the report,
  - return success from compute, then set process exit code to `4` ([API-02-04]).

## [API-09] Thread-safety and reentrancy
- [API-09-01] Artifact functions (`WritePng…`, `ComputeSSIM…`, `SanitizeName`, `MakePngPath`) MUST be thread-safe and reentrant (no global mutable state).
- [API-09-02] `stb_image_write` global state usage (if any) must be avoided; PNG writer must be called with explicit parameters only.
- [API-09-03] Harness may serialize artifact writing even if benchmarking becomes parallel in the future (see Ch6).

## [API-10] ABI & visibility rules
- [API-10-01] Only `vgcpu-benchmark` executable is shipped; internal libs are not intended for external linking.
- [API-10-02] Symbol visibility:
  - use existing `include/vgcpu/internal/export.h` macros for any exported symbols (even internal), consistent with `cpp_api_abi_rules.md`.
- [API-10-03] Header placement rule:
  - internal headers live under `include/vgcpu/internal/…`
  - no new “public stable” headers are introduced in v1.1.

## [TEST-54] Compatibility tests (contract enforcement)
- [TEST-54-01] CLI: invalid SSIM threshold (<0 or >1) exits with code 2.
- [TEST-54-02] CLI: `--compare-ssim` without `--ground-truth-backend` exits with code 2.
- [TEST-54-03] CLI: missing/unavailable ground truth backend exits non-zero before comparisons (code 2 or 3 per implementation; recommended 2 if purely “unknown id”).
- [TEST-54-04] Render contract: for a small scene, adapter output buffer size equals `w*h*4` and channel order is consistent (spot-check via known color pattern in a synthetic adapter).
- [TEST-54-05] Report schema includes `schema_version` and new fields when features enabled.

## [DEC-API-02] SSIM library selection (implementation constraint)
- [DEC-API-02] SSIM implementation MUST be vendored/pinned and lightweight (header-only or single TU) and permissive-licensed ([REQ-152]).
  - Rationale: deterministic, fast builds; avoid heavy image toolkits.
  - Alternatives: OpenCV; custom SSIM implementation.
  - Consequences: implementation MUST record the upstream project + commit/tag in `THIRD_PARTY_NOTICES.md` and in a comment header within the vendored file.

