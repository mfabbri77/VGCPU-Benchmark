# Chapter 5 — Data Design & Hot Path (Layouts, Ownership, Perf Hygiene)

This chapter specifies the data layouts and ownership rules that must remain stable for correctness and performance, especially around the benchmark hot path and the new v1.1 artifact/SSIM features.

## [MEM-01] Hot-path definition and non-interference rules
- [MEM-01-01] The **measured hot path** is the adapter `Render()` call inside the measured repetition loop plus the harness timing/aggregation bookkeeping.
- [MEM-01-02] PNG encoding and SSIM computation MUST NOT execute inside the measured loop ([DEC-ARCH-11]).
- [MEM-01-03] Any memory allocation in the measured loop MUST be avoided; buffers must be allocated/resized during preparation/warmup or prior to measurement.

## [MEM-02] Canonical surface buffer contract (RGBA8 premul)
- [MEM-02-01] Canonical render output buffer is byte-addressable `RGBA8_PREMULTIPLIED`:
  - 4 bytes per pixel, channel order `[R,G,B,A]` ([API-04-02]).
  - tightly packed, `stride_bytes == width * 4` ([API-04-01b]).
- [MEM-02-02] Canonical size:
  - `pixels.size() == (size_t)height * (size_t)stride_bytes` ([API-04-01c]).
- [MEM-02-03] The canonical surface config is immutable per measured run for a (scene, backend).

## [MEM-03] Buffer ownership and lifecycle (per scene/backend)
### [MEM-03-01] Measured loop buffer (per backend)
- [MEM-03-01a] Owner: Harness.
- [MEM-03-01b] Storage: `std::vector<std::uint8_t> render_pixels;`
- [MEM-03-01c] Lifecycle:
  - resized once for given width/height before warmup,
  - reused across warmup and measured repetitions,
  - reused for post-benchmark render (untimed) when artifacts enabled.

### [MEM-03-02] Ground truth buffer (per scene)
- [MEM-03-02a] Owner: Harness (scene scope).
- [MEM-03-02b] Storage: `std::vector<std::uint8_t> gt_pixels;`
- [MEM-03-02c] When SSIM enabled:
  - After ground truth post-benchmark render, the harness MUST copy pixels into `gt_pixels` and retain until all other backends for the scene are compared.
- [MEM-03-02d] Copy policy:
  - copy is performed exactly once per scene (ground truth only), outside measured loops.

### [MEM-03-03] Views passed to artifact code
- [MEM-03-03a] Artifact functions accept `std::span<const std::uint8_t>`; they do not take ownership.
- [MEM-03-03b] Stride is passed explicitly even though v1.1 requires `width*4` (allows future padding with explicit CR).

## [MEM-04] Data structures (implementation-ready)
### [MEM-04-01] `ImageViewRGBA8Premul`
Internal utility type used only inside harness/artifacts (not public API):

```cpp
// [API-05] Internal-only helper (placed in include/vgcpu/internal/artifacts/image_view.h)
struct ImageViewRGBA8Premul {
  int width = 0;
  int height = 0;
  int stride_bytes = 0; // must equal width*4 in v1.1
  std::span<const std::uint8_t> pixels;
};
````

* [MEM-04-01a] Invariant: `pixels.size() >= (size_t)height * (size_t)stride_bytes`.
* [MEM-04-01b] Invariant: `stride_bytes == width * 4` (enforced by asserts + runtime validation returning `StatusCode::kInvalidArgument`).

### [MEM-04-02] Result row extensions

* [MEM-04-02a] Each (scene, backend) report row stores:

  * `png_path` (string, may be empty)
  * `ssim_vs_ground_truth` (optional double)
  * `ground_truth_backend` (string, empty unless SSIM enabled)
  * `ssim_threshold` (double, present when SSIM enabled)
* [MEM-04-02b] These fields must not require storing pixel data beyond the scope described in [MEM-03].

## [MEM-05] Memory layout notes (cache & vectorization)

* [MEM-05-01] `RGBA8` is an AoS 4-byte pixel format; SSIM compute will traverse it linearly.
* [MEM-05-02] SSIM implementation should operate on contiguous rows to preserve cache locality.
* [MEM-05-03] The SSIM library integration must not introduce hidden heap allocations per call; if it does, wrap it with a workspace object allocated once per scene.

## [MEM-06] Allocation strategy and limits

* [MEM-06-01] Allocation points:

  * render buffer resize occurs during scene/backend preparation (not measured).
  * ground truth buffer copy allocates once per scene (not measured).
* [MEM-06-02] Upper bound memory:

  * two full image buffers may coexist for SSIM compare: `(render + gt)`.
  * additional working storage from SSIM library should be bounded by O(width*height) or less and preferably O(width) (windowed) if supported.
* [MEM-06-03] Fail-fast on overflow:

  * If `width*height*4` overflows `size_t`, return `StatusCode::kInvalidArgument` and abort run with exit code `2` or `3` depending on where detected.

## [MEM-07] Artifact path construction data dependencies

* [MEM-07-01] Path depends only on:

  * output_dir
  * scene_name (original → sanitized via [MEM-10-04])
  * backend_id (registry id, already sanitized per [API-06-02])
* [MEM-07-02] Filename is deterministic:

  * `<scene_sanitized>_<backend_id>.png` under `<output_dir>/png/` ([DEC-SCOPE-05]).

## [MEM-08] Diagrams (ownership and lifetimes)

```mermaid
flowchart TB
  subgraph SceneScope[Per Scene Scope]
    GT[gt_pixels (vector<uint8_t>)\nowned by harness]:::own
  end

  subgraph BackendScope[Per Backend Scope]
    RB[render_pixels (vector<uint8_t>)\nowned by harness]:::own
  end

  RB -->|span view| PNGW[WritePngRGBA8Premul]
  RB -->|span view| SSIMC[ComputeSSIM_RGBA8Premul]
  GT -->|span view| SSIMC

  classDef own fill:#f5f5f5,stroke:#999,stroke-width:1px;
```

* [MEM-08-01] Only the harness owns buffers; artifacts module consumes spans.
* [MEM-08-02] Ground truth buffer is only live during a scene’s comparison phase.

## [MEM-09] Hot-path measurement hygiene (rules/tests hooks)

* [MEM-09-01] Measured repetition loop must be free of:

  * filesystem I/O
  * PNG encoding
  * SSIM computation
  * allocations (except unavoidable inside backend, which is considered backend behavior)
* [MEM-09-02] Harness MUST expose internal counters (not logged per-iteration) to verify:

  * post-benchmark render count
  * PNG write count
  * SSIM compute count
    These counters are used only in tests to assert separation of concerns.

## [TEST-55] Memory & data correctness tests (v1.1)

* [TEST-55-01] Buffer sizing:

  * for a chosen `width/height`, after render the output buffer size is exactly `w*h*4` and stride equals `w*4`.
* [TEST-55-02] Ground truth retention:

  * when SSIM enabled, GT pixels are copied once per scene and remain unchanged while comparing other backends.
* [TEST-55-03] SSIM dimension mismatch:

  * if a backend returns an image of different dimensions (forced by a test adapter), `ComputeSSIM…` returns invalid-argument and run fails with a clear error.
* [TEST-55-04] Hot-path non-interference:

  * with instrumentation enabled in tests, assert `png_write_count == 0` and `ssim_count == 0` during measured iterations, and non-zero only after measurement.

## [DEC-MEM-01] Stride policy (v1.1)

* [DEC-MEM-01] v1.1 enforces `stride_bytes == width*4` for all adapters.

  * Rationale: simplifies PNG encoding and SSIM compare; consistent with current adapters.
  * Alternatives: allow padded stride; store stride in `SurfaceConfig`; handle in writers/comparators.
  * Consequences: adapters that require padding must internally convert/copy to tight stride before returning.

## [DEC-MEM-02] Alpha handling policy (v1.1)

* [DEC-MEM-02] SSIM compares **premultiplied RGBA directly** (no un-premultiply; no alpha ignore).

  * Rationale: matches user requirement; avoids extra transforms.
  * Alternatives: un-premultiply; ignore alpha (RGB only).
  * Consequences: backends with slightly different coverage/alpha edges may show lower SSIM; threshold is configurable ([REQ-154]).
