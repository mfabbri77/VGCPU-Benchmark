# Chapter 6 — Interfaces (APIs, Events, Integrations)

## Purpose

This chapter defines the external and internal interfaces required to implement the benchmark suite: CLI contract, adapter contract, IR runtime contract, capability contract, timing/measurement contract, and reporting/output schemas. It also specifies optional inter-process interfaces for isolation mode. No third-party service integrations are assumed.

## 6.1 CLI Interface Contract

### 6.1.1 CLI Command Model

1. The executable **MUST** expose a stable command model with subcommands.

2. The CLI **MUST** support at minimum the following subcommands:

   * `list` — enumerate backends and scenes
   * `run` — execute benchmarks
   * `metadata` — print environment/build metadata
   * `validate` — validate scene manifest + IR assets (and optionally adapter availability)

3. The CLI **SHOULD** provide `--help` for global and subcommand help text.

### 6.1.2 CLI Options (Normative Set)

The `run` subcommand **MUST** support options equivalent to the following (exact flag spelling may vary but semantics MUST match):

**Selection**

1. `--backend <id>[,<id>...]` — select backends by BackendId
2. `--scene <id>[,<id>...]` — select scenes by SceneId
3. `--tags <tag>[,<tag>...]` — filter scenes by tags (if tags exist in manifest)
4. `--all-backends` and `--all-scenes` — include all available items

**Benchmark Policy**
5. `--warmup-iters <n>` and/or `--warmup-ms <n>`
6. `--iters <n>` and/or `--min-ms <n>` (minimum measurement duration)
7. `--repetitions <n>` — independent repetitions per case (not per-iteration samples)
8. `--isolation {none,process}` — choose single-process or multi-process isolation mode

**Output**
9. `--out <path>` — output directory
10. `--format {json,csv,both}` — machine-readable outputs
11. `--summary {text,md}` — human-readable summary format (at least text)
12. `--fail-fast` — stop on first case failure (default: continue)

**Compatibility/Fallback**
13. `--on-unsupported {skip,fail,fallback:<mode>}` — default skip; explicit fallback modes are allowed only if defined in the capability policy

**Other**
14. `--threads <n>` — set thread count if a backend is multi-threaded or if the harness controls threading (default: backend default; see Chapter 8)
15. `--seed <n>` — only if randomness exists; default MUST be deterministic (if not used, flag MAY be omitted)

### 6.1.3 CLI Exit Codes

1. The CLI **MUST** return exit code `0` only if:

   * Argument parsing succeeds, and
   * All executed benchmark cases succeed, and
   * Output writing succeeds.
2. The CLI **MUST** return a non-zero exit code for:

   * Invalid arguments/config (usage error),
   * Manifest/IR validation failure (unless explicitly overridden),
   * Backend initialization failure for a selected backend,
   * A benchmark case failure (unless configured to ignore failures),
   * Output write failure.

## 6.2 Adapter Interface Contract

### 6.2.1 Adapter Interface (C++ API)

Adapters **MUST** implement the `IBackendAdapter` abstract interface:

```cpp
struct AdapterInfo {
    std::string id;
    std::string detailed_name;
    std::string version;
};

struct SurfaceConfig {
    int width;
    int height;
    // ... platform specific handles if needed
};

// Return status for all operations
struct Status {
    enum Code { OK, UNSUPPORTED, FAIL };
    Code code;
    std::string message;
};

class IBackendAdapter {
public:
    virtual ~IBackendAdapter() = default;
    
    // Lifecycle
    virtual Status Initialize(const AdapterWrapperArgs& args) = 0;
    virtual void Shutdown() = 0;
    
    // Metadata
    virtual AdapterInfo GetInfo() const = 0;
    virtual CapabilitySet GetCapabilities() const = 0;
    
    // Rendering
    virtual Status Render(const PreparedScene& scene, 
                          const SurfaceConfig& config,
                          std::vector<uint8_t>& output_buffer) = 0;
};
```

### 6.2.2 CPU-Only Enforcement

Each adapter implementation **MUST**:
1. Select the CPU rasterization path of the backend.
2. If the backend has a `GrContext` or `Device` equivalent, explicitly create a software-only version.
3. Report `is_cpu_only = true` in its metadata if successfully enforced.

## 6.3 IR Runtime Interface Contract

### 6.3.1 IR Runtime API

The IR Runtime **MUST** expose a simple C++ API for loading and validation:

```cpp
struct ValidationReport {
    bool valid;
    std::vector<std::string> errors;
};

class IrLoader {
public:
    static std::optional<SceneBytes> LoadFromFile(const std::filesystem::path& path);
    static ValidationReport Validate(const SceneBytes& bytes);
    static PreparedScene Prepare(const SceneBytes& bytes); 
};
```

### 6.3.2 PreparedScene Contract

`PreparedScene` is a **read-only** view into the validated IR, optimized for random access by adapters.

```cpp
struct PreparedScene {
    // Header info
    u32 width;
    u32 height;
    
    // Accessors for resource tables
    std::span<const Paint> paints;
    std::span<const Path> paths;
    
    // The raw command stream (adapters iterate this)
    std::span<const u8> command_stream;
};
```

## 6.4 Capability Interface Contract

### 6.4.1 CapabilitySet Schema

`CapabilitySet` **MUST** be a structured object with fields at least:

1. Fill rules: `supports_nonzero`, `supports_evenodd`
2. Stroke caps: `supports_cap_butt`, `supports_cap_round`, `supports_cap_square`
3. Stroke joins: `supports_join_miter`, `supports_join_round`, `supports_join_bevel`
4. Dash: `supports_dashes`
5. Gradients: `supports_linear_gradient`, `supports_radial_gradient`
6. Clipping: `supports_clipping` (if used by any scene)
7. Compositing: `supports_source_over` (baseline true if backend included)

### 6.4.2 Compatibility Decision Output

The compatibility/policy engine **MUST** output a structured result per case:

* `decision`: `{EXECUTE, SKIP, FAIL, FALLBACK}`
* `reasons`: list of reason codes (stable identifiers)
* If `FALLBACK`: `fallback_mode` (stable identifier)

Default decision **MUST** be `SKIP` when unsupported features are required.

## 6.5 Measurement and Timing Interfaces

### 6.5.1 Timer Abstraction

The PAL timing interface **MUST** include:

1. `NowMonotonic() -> TimePoint`
2. `Elapsed(start, end) -> Duration`
3. `GetCpuTime() -> Duration` (defined as process or thread CPU time; semantics MUST be stated in metadata)

**Normative requirements**

* `NowMonotonic` **MUST** be monotonic and high-resolution.
* CPU time measurement **MUST** be consistent within a given platform build.

### 6.5.2 Measurement Record Schema

The harness **MUST** record per case:

1. Warm-up policy and results (at least counts/duration)
2. Measurement policy (iters/min time/repetitions)
3. Samples (optional) and aggregates (required):

   * `wall_time_ns` statistics
   * `cpu_time_ns` statistics

Statistics **MUST** include:

* `p50`
* One dispersion metric: `p90` and/or `mad` and/or `iqr`
* `n` (sample count)

## 6.6 Reporting and Output Schemas

### 6.6.1 Machine-Readable Output (JSON)

If JSON output is enabled, it **MUST** include top-level objects:

1. `schema_version`
2. `run_metadata`
3. `cases[]`

`run_metadata` **MUST** include:

* Run identifier (timestamp + git tag/sha)
* OS/arch
* CPU model, logical cores
* Memory
* Compiler/toolchain version
* Benchmark suite version (git sha)
* Dependency versions/pins (see §6.6.3)
* Benchmark policy parameters

Each `case` entry **MUST** include:

* `backend_id`, `backend_metadata`
* `scene_id`, `scene_hash`, `ir_version`
* `config_id` (may be derived)
* `decision` and `reasons` (including SKIP reasons)
* Timing stats (wall/cpu)
* Optional: allocation/RSS metrics if collected

### 6.6.2 Machine-Readable Output (CSV)

If CSV output is enabled, it **MUST** include:

1. One row per case execution outcome (including skipped cases).
2. Columns sufficient to uniquely identify case + include required stats, including:

   * `backend_id`, `scene_id`, `scene_hash`, `ir_version`, `decision`
   * `wall_p50_ns`, `cpu_p50_ns`, and the chosen dispersion columns
   * `n_samples`
3. CSV output **MUST** include a header row and use a stable delimiter (comma).

### 6.6.3 Dependency Pin Record Interface

1. The system **MUST** produce a dependency pin record that can be emitted as:

   * Part of JSON `run_metadata`, and/or
   * A separate file (e.g., `deps.lock.json`) in the output directory.
2. Each dependency record **MUST** include:

   * Name
   * Version and/or git commit hash
   * Source (vendored/fetched/system-installed)
   * For system-installed dependencies: detection method and observed version string

## 6.7 Optional Inter-Process Interface (Isolation Mode)

If process isolation mode is implemented:

1. The parent process **MUST** communicate with worker processes using a versioned message protocol.
2. The protocol **MUST** support messages equivalent to:

   * `RunCaseRequest` (backend_id, scene_id, config, policy)
   * `RunCaseResponse` (decision, reasons, stats, metadata, errors)
3. The message protocol **MUST** be stable and versioned independently or as part of the result schema versioning.
4. Workers **MUST** be executable in a headless environment and **MUST** not require GUI subsystems.

## Acceptance Criteria

1. The CLI contract includes required subcommands, option semantics, and exit code rules.
2. The adapter interface contract includes lifecycle, CPU-only enforcement, surface/pixel format requirements, and structured error handling.
3. The IR runtime contract specifies load/validate/prepare responsibilities and PreparedScene contents.
4. CapabilitySet schema and compatibility decision outputs are defined and enforce default skipping behavior.
5. Timing/measurement interfaces specify monotonic and CPU time measurement and required statistics fields.
6. Output schema requirements for JSON and CSV are defined, including metadata and dependency pinning.

## Dependencies

1. Chapter 2 — Requirements (required CLI behaviors, measurement, reporting).
2. Chapter 4 — System Architecture Overview (component boundaries and responsibilities).
3. Chapter 5 — Data Design (manifest, IR versioning, hashing).

---

