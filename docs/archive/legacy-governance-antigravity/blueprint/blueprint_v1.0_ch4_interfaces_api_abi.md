# Chapter 4 — Interfaces, API/ABI, Error + Thread-Safety Contracts

## [API-01] Compatibility surfaces (what is “public”)
Even though internal C++ libraries exist, the project has **real external compatibility surfaces** that must be treated as APIs.

### [API-01-01] CLI contract (public)
- [REQ-46] The CLI commands/options are a compatibility surface:
  - `--help`, `list`, `run` and their stable option names.
- [REQ-47] CLI behavior MUST be SemVer-governed (see Ch9):
  - Breaking flag changes require a major bump.
  - Additive flags are minor.
  - Bugfix behavior is patch.

### [API-01-02] Report schemas (public)
- [REQ-48] CSV and JSON output formats are compatibility surfaces (per [DEC-SCOPE-03]).
- [REQ-49] Reports MUST carry:
  - `tool_version` (product version)
  - `schema_version` (report schema version)
  - `run_id` (unique per run)
- [REQ-50] Schema evolution MUST be documented and validated by tests (see [TEST-18], [TEST-19] in Ch3 + [TEST-24] below).

### [API-01-03] Internal C++ “module interfaces” (not a stable external ABI)
- [DEC-API-01] Internal `vgcpu_*` static libs expose **internal-only** headers; no stable external SDK ABI is promised.
  - Rationale: aligns with [DEC-ARCH-01] and reduces ABI constraints.
  - Alternatives: ship public SDK; ship stable C ABI.
  - Consequences: CMake + include layout must prevent “accidental public headers” (Ch7 enforcement).

### [API-01-04] Rust FFI boundary (C ABI, internal)
- [REQ-51] If Rust adapters are enabled, the boundary between C++ and Rust MUST be C ABI (`extern "C"`) with explicit ownership rules and no exceptions crossing the boundary.

---

## [API-02] ABI strategy (mandatory decision)
- [DEC-API-02] ABI Strategy = **Strategy A: Best-effort ABI** (consumers recompile; no ABI stability guarantee across minor).
  - Applies because:
    - libraries are linked statically into the CLI by default, and
    - we are not promising a distributable binary SDK.

### [REQ-52] Symbol visibility and exports
- [REQ-52-01] Build MUST default to hidden visibility on ELF/Mach-O (`-fvisibility=hidden`) and explicit export on Windows where applicable.
- [REQ-52-02] Provide standard export macros even if currently static-only (future-proofing).

#### Export/version macros (authoritative)
- [API-02-10] `include/vgcpu/export.h`
  - `VGCPU_EXPORT` / `VGCPU_NO_EXPORT`
  - `VGCPU_PUBLIC` / `VGCPU_PRIVATE` (attributes)
- [API-02-11] `include/vgcpu/version.h`
  - `VGCPU_VERSION_MAJOR/MINOR/PATCH`
  - `VGCPU_GIT_SHA` (optional; empty if unavailable)
  - `VGCPU_REPORT_SCHEMA_VERSION` (integer)

> Note: These are compile-time macros, generated/filled from CMake configure step (Ch7).

---

## [API-03] Error handling strategy (mandatory decision)
- [DEC-API-03] Error Strategy = **No exceptions as a contract across VGCPU module boundaries**:
  - Use `Status` / `Result<T>` end-to-end across VGCPU components.
  - Exceptions MAY be used inside third-party integration glue **only if caught** and converted to `Status` at the boundary.
  - Rationale: predictable control flow and easy reporting integration (aligns with [DEC-ARCH-09]).
  - Alternatives: exceptions end-to-end.
  - Consequences: all interfaces below are `noexcept` where practical and return `Status/Result`.

### [REQ-53] Canonical error types
#### ErrorCode
- [REQ-53-01] Define `enum class ErrorCode : uint32_t` with explicit values; never reorder; only append.

Suggested reserved ranges (append-only):
- [API-03-01] `0..99` = success/common
- [API-03-02] `100..199` = CLI/args
- [API-03-03] `200..299` = filesystem/assets
- [API-03-04] `300..399` = manifest/JSON
- [API-03-05] `400..499` = IR decode/validation
- [API-03-06] `500..599` = adapter registry / backend enablement
- [API-03-07] `600..699` = backend init/prepare/render
- [API-03-08] `700..799` = harness/statistics
- [API-03-09] `800..899` = reporting/serialization

#### Error / Status / Result
- [REQ-53-02] Define:
  - `struct Error { ErrorCode code; uint32_t subcode; FixedString<192> message; }`
  - `class Status` (ok + error)
  - `template<class T> class Result` (value-or-error)
- [REQ-53-03] Errors MUST be cheap and not allocate on hot paths:
  - fixed-size message buffer; formatting limited.
- [REQ-53-04] Every error MUST be loggable as structured fields:
  - `code`, `subcode`, `message`.

### [REQ-54] Error conversion rules
- [REQ-54-01] Convert third-party error types to VGCPU `ErrorCode` at the boundary (adapter init/prepare/render).
- [REQ-54-02] Never throw across:
  - module boundaries,
  - Rust C ABI boundary,
  - C callbacks from third-party libs.

---

## [API-04] Thread-safety and reentrancy contracts
- [DEC-API-04] Default contract: **single-threaded harness execution** (threads=1) with optional multi-thread mode; adapters are **not thread-safe unless explicitly declared**.
  - Rationale: benchmark comparability and noise reduction (aligns with [DEC-ARCH-10]).
  - Alternatives: always parallelize.
  - Consequences: harness instantiates adapter instances per worker in multi-thread mode.

### [REQ-55] Component thread-safety matrix (authoritative)
| Component | Thread-safety contract | Notes |
|---|---|---|
| Assets/Manifest | thread-safe after load (immutable) | load occurs once pre-run |
| IR Loader | thread-safe if no shared mutable state | prefer immutable `SceneIR` |
| Adapter Registry | thread-safe for read-only enumeration | creation guarded or single-threaded |
| Adapter instance | **NOT** thread-safe by default | one instance per thread/worker |
| Prepared backend scene | thread-safe if immutable | adapter decides; should be immutable |
| Harness | orchestrates concurrency | owns worker threads (Ch6) |
| Reporting | single-threaded emit by default | writes after benchmark |

### [REQ-56] Reentrancy rules
- [REQ-56-01] `Render()` on an adapter must be callable repeatedly with identical semantics (no hidden global mutation).
- [REQ-56-02] If an adapter uses global state (some libraries do), it must serialize internally and declare itself “single-thread-only”.

---

## [API-05] Header layout and ownership rules
### [DEC-API-05] Header structure to avoid “accidental public API”
- Move reusable headers into:
  - `include/vgcpu/internal/...` (internal-only, installed nowhere)
- Keep implementation headers in:
  - `src/...` (private includes)
- Consequences:
  - CMake targets expose only `include/` privately to internal libs; the executable includes internal headers via target links (Ch7).

### [REQ-57] Ownership and lifetime conventions (global)
- [REQ-57-01] “Creator owns” unless otherwise specified.
- [REQ-57-02] Raw pointers are non-owning; owning pointers must be explicit (`unique_ptr`, `shared_ptr`) and must not cross stable ABI boundaries (not relevant here, but still enforced for cleanliness).
- [REQ-57-03] All non-trivial resources must be RAII-managed in C++.

---

## [API-06] Internal interfaces (authoritative sketches)

> Sketches below are **signatures only** (no implementations in the blueprint).
> Namespaces and exact filenames are binding; types may evolve only via CRs.

### [API-06-01] Common: errors and results
File: `include/vgcpu/internal/status.h`
```cpp
#pragma once
#include <cstdint>
#include <string_view>

namespace vgcpu::common {

enum class ErrorCode : uint32_t {
  Ok = 0,

  // 100..199 CLI
  InvalidArgs = 100,
  UnknownCommand = 101,

  // 200..299 FS/assets
  FileNotFound = 200,
  IoError = 201,

  // 300..399 manifest/json
  ManifestInvalid = 300,

  // 400..499 IR
  IrUnsupportedVersion = 400,
  IrDecodeError = 401,
  IrValidationError = 402,

  // 500..599 registry
  BackendNotFound = 500,
  BackendDisabled = 501,

  // 600..699 adapter
  BackendInitFailed = 600,
  BackendPrepareFailed = 601,
  BackendRenderFailed = 602,

  // 700..799 harness
  TimerFailed = 700,
  StatsError = 701,

  // 800..899 reporting
  ReportWriteFailed = 800,
  ReportSerializeFailed = 801,
};

struct Error {
  ErrorCode code{ErrorCode::Ok};
  uint32_t subcode{0};
  // Fixed-size message storage (implementation-defined).
  // Must not allocate.
  const char* message_cstr() const noexcept;
};

class Status {
public:
  static Status Ok() noexcept;
  static Status From(Error e) noexcept;

  bool ok() const noexcept;
  Error error() const noexcept;

private:
  Error err_{};
};

template <class T>
class Result {
public:
  static Result Ok(T v) noexcept;
  static Result Err(Error e) noexcept;

  bool ok() const noexcept;
  const T& value() const noexcept;
  T& value() noexcept;
  Error error() const noexcept;

private:
  // Implementation-defined (union/optional-like) without heap allocation.
};

} // namespace vgcpu::common
````

### [API-06-02] PAL: monotonic clock and environment

File: `include/vgcpu/internal/pal.h`

```cpp
#pragma once
#include <cstdint>

namespace vgcpu::pal {

struct EnvInfo {
  // Fixed-size strings or interned strings (implementation-defined).
  const char* os_name;
  const char* os_version;
  const char* cpu_brand;
  uint32_t cpu_logical_cores;
  const char* toolchain;
  const char* build_type;
};

uint64_t NowMonotonicNs() noexcept;     // [REQ-26]
EnvInfo GetEnvInfo() noexcept;          // best-effort

} // namespace vgcpu::pal
```

### [API-06-03] Assets/Manifest: scene discovery

File: `include/vgcpu/internal/assets.h`

```cpp
#pragma once
#include <string_view>
#include <vector>
#include "vgcpu/internal/status.h"

namespace vgcpu::assets {

struct SceneMeta {
  std::string_view id;     // stable key
  std::string_view name;
  std::string_view path;   // relative path to .irbin
  // tags/dimensions optional
};

struct Manifest {
  std::vector<SceneMeta> scenes; // sorted by id ([REQ-29])
};

common::Result<Manifest> LoadManifest(std::string_view assets_root) noexcept; // [REQ-28]

} // namespace vgcpu::assets
```

### [API-06-04] IR: decoding to canonical representation

File: `include/vgcpu/internal/ir.h`

```cpp
#pragma once
#include <cstdint>
#include <string_view>
#include "vgcpu/internal/status.h"

namespace vgcpu::ir {

struct SceneIR;          // opaque (defined in .cpp)
struct PreparedScene;    // immutable after creation ([DEC-ARCH-07])

common::Result<SceneIR> LoadIrFromFile(std::string_view path) noexcept;     // [REQ-30]
common::Result<PreparedScene> PrepareScene(const SceneIR& ir) noexcept;

} // namespace vgcpu::ir
```

### [API-06-05] Adapters: descriptors, registry, adapter instance

File: `include/vgcpu/internal/adapters.h`

```cpp
#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include "vgcpu/internal/status.h"
#include "vgcpu/internal/ir.h"

namespace vgcpu::adapters {

enum class BackendTier : uint8_t { Tier1, Optional };

struct BackendDescriptor {
  std::string_view name;        // stable CLI id ([REQ-33])
  std::string_view display_name;
  std::string_view version;     // backend lib version if available
  BackendTier tier;
};

struct SurfaceDesc {
  uint32_t width;
  uint32_t height;
  uint32_t stride_bytes;
  enum class PixelFormat : uint8_t { RGBA8_Premul } format;
};

struct BackendCreateInfo {
  SurfaceDesc surface;
  uint32_t threads;             // requested threads; adapter may ignore
  bool force_single_thread;     // [REQ-45]
};

struct PreparedBackendScene;    // opaque backend-specific prepared object

class IBackendAdapter {
public:
  virtual ~IBackendAdapter() = default;

  virtual BackendDescriptor descriptor() const noexcept = 0;

  // Prepare backend resources for a scene (may allocate; not hot path).
  virtual common::Result<PreparedBackendScene>
  Prepare(const vgcpu::ir::PreparedScene& scene) noexcept = 0;

  // Render a single iteration into provided pixel buffer (hot path).
  // Must not perform filesystem I/O or logging inside this call ([REQ-21], [REQ-22]).
  virtual common::Status
  Render(const PreparedBackendScene& prepared,
         void* out_rgba,
         uint32_t out_stride_bytes) noexcept = 0;
};

class Registry {
public:
  // Enumerate compiled-in backends ([REQ-32])
  std::span<const BackendDescriptor> Enumerate() const noexcept;

  // Create adapter by stable name
  common::Result<std::unique_ptr<IBackendAdapter>>
  Create(std::string_view backend_name, const BackendCreateInfo& info) noexcept;
};

} // namespace vgcpu::adapters
```

### [API-06-06] Harness: run orchestration

File: `include/vgcpu/internal/harness.h`

```cpp
#pragma once
#include <cstdint>
#include <string_view>
#include <vector>
#include "vgcpu/internal/status.h"
#include "vgcpu/internal/assets.h"
#include "vgcpu/internal/adapters.h"

namespace vgcpu::harness {

struct RunConfig {
  std::string_view backend;
  std::vector<std::string_view> scene_ids; // resolved & ordered
  uint32_t warmup_iters;
  uint32_t reps;
  uint32_t threads;                 // default 1
  bool continue_on_error;           // per [DEC-ARCH-08]
  uint32_t seed;                    // [REQ-45]
};

struct SceneStats {
  uint64_t min_ns;
  uint64_t mean_ns;
  uint64_t median_ns;
  uint64_t p50_ns;
  uint64_t p90_ns;
  uint64_t p99_ns;
  uint32_t sample_count;
};

struct RunReport {
  uint32_t schema_version;
  const char* tool_version;
  const char* run_id;
  // environment metadata + per-scene stats + per-scene errors (implementation-defined)
};

common::Result<RunReport>
RunBenchmark(const RunConfig& cfg,
             const assets::Manifest& manifest,
             adapters::Registry& registry,
             std::string_view assets_root) noexcept;

} // namespace vgcpu::harness
```

### [API-06-07] Reporting: CSV/JSON emit

File: `include/vgcpu/internal/reporting.h`

```cpp
#pragma once
#include <string_view>
#include "vgcpu/internal/status.h"
#include "vgcpu/internal/harness.h"

namespace vgcpu::reporting {

common::Status WriteJson(std::string_view path, const harness::RunReport& r) noexcept; // [REQ-40]
common::Status WriteCsv(std::string_view path, const harness::RunReport& r) noexcept;  // [REQ-41]
common::Status WriteSummaryToStdout(const harness::RunReport& r) noexcept;             // [REQ-05-03]

} // namespace vgcpu::reporting
```

---

## [API-07] Rust FFI (internal C ABI contract)

If `ENABLE_RAQOTE` or `ENABLE_VELLO_CPU` is ON:

### [REQ-58] C ABI rules

* [REQ-58-01] All exported Rust symbols must be `extern "C"` and `#[no_mangle]`.
* [REQ-58-02] Ownership rules:

  * C++ allocates output buffer; Rust writes into it.
  * Rust returns errors via `(code, message)` using a fixed-size caller-provided buffer.

### Suggested minimal FFI (pattern)

File: `rust_bridge/<backend>_ffi/include/<backend>_ffi.h`

```c
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vgcpu_rust_ctx vgcpu_rust_ctx;

vgcpu_rust_ctx* vgcpu_rust_create(uint32_t w, uint32_t h);
void vgcpu_rust_destroy(vgcpu_rust_ctx* ctx);

// Returns 0 on success, non-zero error code on failure.
// Writes a null-terminated error message into err_buf if provided.
int vgcpu_rust_render_ir(vgcpu_rust_ctx* ctx,
                         const uint8_t* ir_bytes, size_t ir_len,
                         uint8_t* out_rgba, uint32_t out_stride,
                         char* err_buf, size_t err_buf_len);

#ifdef __cplusplus
}
#endif
```

* [DEC-API-06] Use this uniform FFI shape for both Raqote and Vello adapters.

  * Rationale: consistent adapter glue and error handling.
  * Alternatives: bespoke FFI per backend.
  * Consequences: Rust crates must provide this interface (Ch7 build integration + Ch3 adapter contracts).

---

## [API-08] Deprecation + evolution rules for public surfaces

* [REQ-59] Deprecation MUST be explicit:

  * CLI: mark flags as deprecated in `--help` and logs for at least one minor version before removal.
  * Schemas: support reading old schemas only if explicitly required; at minimum, keep writing stable schemas within a major line.
* [REQ-60] Any change to:

  * CLI flags,
  * backend stable names,
  * schema fields,
    MUST be recorded via a CR and mapped to tests (Ch9 governance).

---

## [API-09] Interface-level tests (additions)

These tests complement those defined in Ch3 and specifically guard API/compat behavior.

* [TEST-21] `cli_flags_stable`: parse baseline flags; ensure removed/renamed flags trigger a controlled deprecation path (golden help text subset).
* [TEST-22] `report_json_schema_roundtrip`: validate JSON against a minimal schema checker and required fields ([REQ-49], [REQ-40]).
* [TEST-23] `report_csv_header_stable`: golden test for CSV header order + schema header comment.
* [TEST-24] `schema_version_bump_rule`: if report format changes, `VGCPU_REPORT_SCHEMA_VERSION` must be bumped and change must reference a CR id.

---

## [API-10] Traceability notes

* [API-10-01] This chapter defines compatibility surfaces [REQ-46..60] and their enforcement points.
* [API-10-02] Ch5 and Ch6 refine hot-path and concurrency constraints for `IBackendAdapter::Render()` and harness execution.
* [API-10-03] Ch7 defines how headers, export macros, and feature flags are generated and enforced by CMake + CI.

