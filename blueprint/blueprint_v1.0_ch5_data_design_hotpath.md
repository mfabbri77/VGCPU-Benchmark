# Chapter 5 — Data Design & Hot Path (Layouts, Ownership, Allocators, Memory Tests)

## [MEM-01] Goals (data + hot path)
- [MEM-01-01] Keep the **measured loop** (`IBackendAdapter::Render`) free of VGCPU-side allocations and I/O (enforces [REQ-21], [REQ-23]).
- [MEM-01-02] Ensure IR and manifest data are **immutable after load** and safe to share across components ([DEC-ARCH-07]).
- [MEM-01-03] Make stats/report computation deterministic and stable-order ([REQ-20], [REQ-38], [REQ-42]).
- [MEM-01-04] Provide predictable memory usage with explicit budgets and leak detection hooks.

## [MEM-02] Ownership & lifetime map (authoritative)
### [MEM-02-01] High-level ownership graph
```mermaid
flowchart LR
  CLI[CLI] -->|owns| CFG[RunConfig]
  CLI -->|owns| REG[Registry]
  CLI -->|owns| MAN[Manifest]
  CLI -->|owns| RPT[RunReport]

  MAN -->|references| SCENES["SceneMeta[]"]
  SCENES -->|paths| IRFILES[(.irbin files)]

  CLI -->|loads| IR[SceneIR]
  IR -->|prepare| PS[PreparedScene]

  REG -->|creates| ADP[IBackendAdapter]
  ADP -->|prepare| PBS[PreparedBackendScene]
  ADP -->|render| BUF[RGBA buffer]

  RPT -->|contains| STATS["SceneStats[]"]
  RPT -->|contains| ERRORS["SceneError[]"]
````

### [REQ-61] Lifetime rules (binding)

* [REQ-61-01] `Manifest` lives for the whole run (load once pre-run).
* [REQ-61-02] `SceneIR` and `PreparedScene` live at least until:

  * all adapters have prepared their backend scenes, and
  * the run has completed for that scene.
* [REQ-61-03] `PreparedBackendScene` is owned by the adapter instance and freed in adapter `Shutdown` / destructor.
* [REQ-61-04] Output pixel buffer is owned by the harness and reused across iterations.
* [REQ-61-05] `RunReport` owns only stable, copyable summaries (stats + small metadata); it MUST NOT retain large IR blobs.

## [MEM-03] Core data structures (concrete, implementation-ready)

### [MEM-03-01] Fixed-size string for errors and metadata

* [DEC-MEM-01] Use an internal fixed-size string type `FixedString<N>` for:

  * error messages,
  * short metadata fields that must not allocate in error paths.
* Rationale: supports [REQ-53-03] and avoids heap churn.
* Alternatives: `std::string` everywhere.
* Consequences: truncation rules must be explicit.

**API (internal)**
File: `include/vgcpu/internal/fixed_string.h`

```cpp
#pragma once
#include <cstddef>
#include <cstring>
#include <string_view>

namespace vgcpu::common {

template <size_t N>
struct FixedString {
  char buf[N]{};
  size_t len{0};

  constexpr FixedString() noexcept = default;

  constexpr void set(std::string_view s) noexcept {
    len = (s.size() < (N - 1)) ? s.size() : (N - 1);
    if (len) std::memcpy(buf, s.data(), len);
    buf[len] = '\0';
  }

  constexpr const char* c_str() const noexcept { return buf; }
  constexpr std::string_view view() const noexcept { return {buf, len}; }
};

} // namespace vgcpu::common
```

### [MEM-03-02] Timing samples storage

* [DEC-MEM-02] Store per-scene measured samples in a contiguous `std::vector<uint64_t>` sized once per scene (`reps`) **before** measurement begins.

  * Rationale: simplest and deterministic percentile computation.
  * Alternatives: streaming quantile estimator; ring buffer.
  * Consequences: memory cost scales with `reps * scenes`.

**Rule**

* [REQ-62] For each scene, the harness MUST reserve/size the sample vector outside the measured loop (before calling `Render()`).

### [MEM-03-03] Scene stats computation inputs

* [REQ-63] Stats computation MUST operate on:

  * sorted copy of samples OR
  * in-place sort of the preallocated sample vector
    deterministically.
* [DEC-MEM-03] Use in-place sort of the preallocated vector after measurement to avoid extra allocations; percentiles computed by deterministic index selection.

  * Consequences: report stores only aggregated stats; raw samples optionally emitted behind a flag (out of scope by default).

### [MEM-03-04] Output pixel buffer

* [REQ-64] The harness MUST allocate an RGBA8 premultiplied buffer sized:

  * `height * stride_bytes` with `stride_bytes >= width * 4`.
* [DEC-MEM-04] Default stride is tightly packed (`width * 4`) and aligned to 64 bytes when feasible.

  * Rationale: cache-line friendliness; simple interop with backends.
  * Alternatives: backend-provided stride.
  * Consequences: adapters must accept caller-provided buffer and stride ([API-06-05]).

**Alignment rule**

* [REQ-65] Output buffer base pointer should be 64-byte aligned in Release builds (best-effort).

## [MEM-04] IR/PreparedScene memory model (backend-independent)

Because the IR format is already present in the repo, the blueprint constrains *how it is stored and shared*, without redefining every IR opcode.

### [REQ-66] IR immutability + sharing

* [REQ-66-01] `SceneIR` and `PreparedScene` MUST be immutable after creation.

* [REQ-66-02] Shared ownership must be explicit:

  * `std::shared_ptr<const SceneIR>` / `std::shared_ptr<const PreparedScene>` OR
  * single-owner container in harness with `const&` views passed to adapters.

* [DEC-MEM-05] Use a single-owner harness container that stores `SceneBundle` objects:

  * `SceneBundle { SceneMeta meta; SceneIR ir; PreparedScene prepared; }`
    and passes `const PreparedScene&` to adapters (no shared_ptr).
  * Rationale: simplest ownership, avoids atomic refcount overhead.
  * Alternatives: shared_ptr for caching across runs.
  * Consequences: if we later add caching across multiple runs/commands, we can revisit via CR.

### [MEM-04-01] PreparedScene structure guidelines

* [REQ-67] `PreparedScene` must use SoA-style arrays for hot iteration where adapters traverse commands:

  * arrays of opcodes, indices, and numeric payloads stored contiguously.
* [DEC-MEM-06] Use a compact command stream:

  * `struct Cmd { uint16_t op; uint16_t a; uint32_t b; }` (8 bytes) and separate payload arrays for floats/points.
  * Rationale: good cache density and easy backend translation.
  * Alternatives: variant-per-command.
  * Consequences: IR decoder/prep phase must normalize into this stream.

> Note: This is a constraint on the internal prepared representation; it does not change on-disk `.irbin` format without a CR.

## [MEM-05] Allocator strategy (prepare vs measured loop)

### [MEM-05-01] Allocation phases

* [REQ-68] All VGCPU allocations MUST occur in one of:

  * **Load phase** (manifest + IR decode),
  * **Prepare phase** (scene preprocessing + backend prepare),
  * **Report phase** (serialization buffers),
    and MUST NOT occur in the measured loop.

### [DEC-MEM-07] Use a monotonic arena for scene preparation (VGCPU side)

* Provide `Arena` (bump allocator) used only during `PrepareScene` to build `PreparedScene` buffers.
* After `PreparedScene` is fully built, its memory is owned by the `PreparedScene` object (arena memory becomes part of it), not freed until the end of run (or scene unload).
* Rationale: fast, deterministic, avoids fragmentation.
* Alternatives: `std::pmr::monotonic_buffer_resource`.
* Consequences: we add a small internal allocator implementation; unit-test it.

**API (internal)**
File: `include/vgcpu/internal/arena.h`

```cpp
#pragma once
#include <cstddef>
#include <cstdint>

namespace vgcpu::common {

class Arena {
public:
  explicit Arena(size_t initial_bytes) noexcept;
  ~Arena() noexcept;

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  void* alloc(size_t bytes, size_t alignment) noexcept;
  void  reset() noexcept; // optional; not used in measured loop

  size_t bytes_committed() const noexcept;

private:
  // implementation-defined: chunks linked list
};

} // namespace vgcpu::common
```

### [MEM-05-02] Adapter allocations

* [REQ-69] Adapter `Prepare()` may allocate (backend resources) but MUST do so only in prepare stage (outside measured loop).
* [REQ-70] Adapter `Render()` must not allocate VGCPU-side memory; backend internals are out of VGCPU control but should be minimized where possible.

## [MEM-06] Hot path rules (measured loop)

### [REQ-71] Measured loop invariants (binding)

During each measured iteration:

* [REQ-71-01] No VGCPU filesystem access.
* [REQ-71-02] No VGCPU logging.
* [REQ-71-03] No VGCPU heap allocations (including implicit allocations via `std::string` growth, iostreams, locale, regex, etc.).
* [REQ-71-04] No dynamic dispatch beyond a single virtual call boundary is acceptable (the adapter `Render()` call).
* [REQ-71-05] Timers: only two PAL calls (`start=NowMonotonicNs`, `end=NowMonotonicNs`).

### [DEC-MEM-08] Allocation instrumentation policy

* In Debug/RelWithDebInfo builds, VGCPU will optionally enable an allocation counter for the measured loop:

  * start counter before loop; end counter after loop; assert delta==0.
* Rationale: enforces [REQ-23], [REQ-71-03].
* Alternatives: rely on reviews only.
* Consequences: platform-specific malloc hooks may be needed; if too invasive, restrict to Linux/macOS or implement a scoped `new/delete` override only for VGCPU libs (Ch8).

## [MEM-07] Memory budgets (operational)

These are *tool* budgets (not backend internals), aligned with [REQ-11].

### [REQ-72] VGCPU-side memory budgets

| Item                      |           Budget | Notes                                 |
| ------------------------- | ---------------: | ------------------------------------- |
| Manifest + scene metadata |          ≤ 2 MiB | depends on scene count                |
| IR decode per scene       |         ≤ 64 MiB | large scenes; typical much less       |
| PreparedScene per scene   |        ≤ 128 MiB | includes normalized arrays            |
| Samples per scene         | `reps * 8 bytes` | e.g. 10,000 reps = ~80 KiB            |
| Output buffer per run     | `width*height*4` | reused; default dims should be modest |

* [DEC-MEM-09] Default benchmark surface size is **1024×1024** unless scenes specify otherwise.

  * Rationale: reasonable CPU workload while keeping memory bounded.
  * Alternatives: 512×512; 2048×2048.
  * Consequences: CLI should allow `--width/--height` override; report includes dimensions.

## [MEM-08] Serialization/report data model (schema stability)

* [REQ-73] `RunReport` must store:

  * stable-ordered per-scene stats,
  * per-scene error records (if any),
  * run configuration fields needed for reproduction.
* [REQ-74] `RunReport` must not store raw pointers or references to temporary buffers used in report writers.

## [MEM-09] Memory & hot-path tests

These tests are required to make the memory rules enforceable.

* [TEST-25] `arena_alignment`: allocations respect requested alignment and do not overlap.
* [TEST-26] `arena_reset_reuse`: reset reuses memory without leaks (if reset supported).
* [TEST-27] `no_alloc_in_measured_loop_null`:

  * run `null` backend with `reps=1000` in debug instrumentation mode and assert allocation delta == 0.
* [TEST-28] `samples_preallocated`:

  * verify sample vector capacity/size is set before measurement begins (can be tested by a harness hook in test builds).
* [TEST-29] `report_does_not_reference_ir`:

  * after freeing `SceneIR/PreparedScene`, `RunReport` remains valid and serializable (ensures [REQ-61-05], [REQ-74]).
* [TEST-30] `output_buffer_alignment`:

  * best-effort assert 64-byte alignment for output buffer in supported builds/platforms.

## [MEM-10] Traceability notes

* [MEM-10-01] This chapter defines [REQ-61..74] and [TEST-25..30].
* [MEM-10-02] Ch6 will define concurrency rules that interact with:

  * per-thread adapter instances,
  * per-thread output buffers (if multi-thread mode),
  * determinism and race-free sample collection.
* [MEM-10-03] Ch7/Ch8 define how allocation instrumentation and sanitizers are wired into builds/CI.

