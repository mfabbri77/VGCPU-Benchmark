<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Chapter 9 — Security, Privacy, and Compliance

## Purpose

This chapter specifies security, privacy, and compliance requirements for the benchmark suite. It focuses on safe handling of untrusted inputs (IR assets and configuration), supply-chain integrity for dependencies and releases, and controlled disclosure of environment metadata.

## 9.1 Security Objectives and Threat Model

### 9.1.1 Objectives

1. The system **MUST** prevent common classes of vulnerabilities in local tools, including:

   * memory safety issues triggered by malformed IR/manifest inputs,
   * command injection via CLI/configuration,
   * unsafe file writes (path traversal, overwriting sensitive paths),
   * uncontrolled execution of untrusted binaries (in isolation mode).
2. The system **MUST** provide integrity signals for builds and releases (checksums, provenance metadata).
3. The system **MUST** provide transparent disclosure of what metadata is collected and emitted.

### 9.1.2 Threat Model Boundaries

1. The tool is assumed to run locally under the invoking user account.
2. The tool **MUST NOT** require elevated privileges.
3. Remote network services are not required; therefore, the system **MUST** function with networking disabled.
4. The primary untrusted inputs are:

   * scene manifest files,
   * IR binary assets,
   * user-provided CLI arguments and file paths,
   * (optional) worker process messages in isolation mode.

## 9.2 Input Validation and Safe Parsing

### 9.2.1 Manifest and IR Validation

1. The system **MUST** treat manifest and IR assets as untrusted inputs.
2. The IR runtime **MUST** perform bounds-checked parsing and validate:

   * header correctness,
   * section sizes and offsets within file bounds,
   * index ranges for referenced objects,
   * command stream well-formedness (including save/restore nesting),
   * numeric sanity checks (e.g., non-negative stroke widths, valid dash patterns).
3. Validation failures **MUST** produce structured errors and **MUST NOT** result in undefined behavior.
4. The harness **MUST** reject IR payloads with unsupported major versions.

### 9.2.2 Defensive Resource Limits

1. The system **SHOULD** impose configurable limits to mitigate resource exhaustion from malformed or extreme scenes, including:

   * maximum IR file size accepted,
   * maximum number of commands,
   * maximum number of path segments/verbs,
   * maximum gradient stop count,
   * maximum canvas dimensions allowed by CLI overrides.
2. If limits are exceeded, the case **MUST** fail validation with a structured reason code (e.g., `IR_LIMIT_EXCEEDED`).

### 9.2.3 CLI and Configuration Safety

1. CLI parsing **MUST** reject unknown options (unless an explicit “passthrough” mode is implemented, which is not planned).
2. File path arguments **MUST** be treated as data, not executable commands.
3. The system **MUST NOT** evaluate arbitrary code from configuration files.

## 9.3 File System and Output Safety

1. The system **MUST** write outputs only within a user-specified output directory (or a default under the current working directory).
2. The system **MUST** prevent path traversal in generated filenames derived from SceneId/BackendId by:

   * restricting output filenames to a safe character set, and
   * mapping identifiers to sanitized filenames.
3. The system **MUST** define overwrite behavior explicitly:

   * default behavior **SHOULD** be “create if absent; fail if exists,” or “create a timestamped subdirectory,”
   * any overwrite mode **MUST** be opt-in.
4. Output writing failures **MUST** be reported as `FAIL` outcomes with reason `OUTPUT_WRITE_FAILED`.

## 9.4 Supply-Chain Security and Dependency Integrity

### 9.4.1 Dependency Pinning and Provenance

1. The project **MUST** pin and record dependency versions (including commit hashes where applicable).
2. For system-installed dependencies, the project **MUST** record:

   * observed version string,
   * detection mechanism (e.g., pkg-config, library query, compile-time macro).
3. The project **SHOULD** prefer reproducible sources (tagged releases or immutable commit hashes) for fetched dependencies.

### 9.4.2 Release Artifacts and Integrity

1. Binary releases **MUST** include checksums for each artifact.
2. Releases **SHOULD** include provenance metadata sufficient to associate artifacts with:

   * a git tag/commit,
   * a build configuration,
   * dependency pin records.
3. The project **SHOULD** support signing releases (e.g., with Sigstore or GPG) if the maintainer workflow permits; if not implemented, the documentation **MUST** state this explicitly.

## 9.5 Isolation Mode Security (Multi-Process)

If process isolation mode is supported:

1. The parent process **MUST** spawn only the project’s own worker executable (or a controlled entry point), not arbitrary user-provided commands.
2. The worker executable path **MUST** be resolved deterministically from the installed package or build output.
3. The IPC message protocol **MUST** be versioned and validated:

   * message length and structure checks,
   * schema version checks,
   * rejection of unknown required fields.
4. Worker failures **MUST** be contained and reported; the parent **MUST NOT** trust partial/invalid IPC responses.
5. Isolation mode **SHOULD** apply conservative timeouts to prevent hangs (recorded in metadata).

## 9.6 Privacy Requirements for Metadata

### 9.6.1 Metadata Classification

Metadata collected/emitted **MUST** be classified as:

1. **Required (non-identifying)**:

   * OS and version,
   * architecture,
   * CPU model (as reported by OS),
   * logical core count,
   * memory size,
   * compiler/toolchain versions,
   * benchmark suite git revision,
   * backend library versions/commit hashes,
   * benchmark policy parameters.
2. **Potentially identifying (optional)**:

   * hostname,
   * username,
   * full filesystem paths beyond the output directory,
   * environment variables.

### 9.6.2 Default Privacy Posture

1. The system **MUST** exclude potentially identifying fields by default.
2. The system **MAY** provide an explicit opt-in flag (e.g., `--include-identifying-metadata`) to include such fields.
3. Human-readable reports **MUST** clearly indicate whether identifying metadata was included.

## 9.7 Compliance Considerations

1. The project **MUST** comply with third-party licenses for included backends and dependencies; licensing requirements are detailed in Appendix F.
2. The project **MUST** avoid bundling content that triggers additional compliance obligations (e.g., fonts, raster images), consistent with project scope.
3. Documentation **MUST** state that results may vary across systems and that metadata is intended to support reproducibility and interpretation.

## Acceptance Criteria

1. Manifest/IR parsing is specified as untrusted input handling with required bounds checks and validation outcomes.
2. The design includes resource limits (at least as a SHOULD requirement) and a failure mode for limit violations.
3. Output writing is constrained to safe directories with path sanitization and explicit overwrite policy.
4. Dependency pinning and release integrity (checksums, provenance) requirements are explicitly stated.
5. Isolation mode has explicit rules preventing arbitrary command execution and validating IPC.
6. Metadata privacy defaults exclude identifying fields unless opt-in is provided.

## Dependencies

1. Chapter 5 — Data Design (IR and manifest formats, hashing/versioning).
2. Chapter 6 — Interfaces (IPC schema considerations, output schemas).
3. Chapter 8 — Runtime Behavior (isolation mode execution, error/outcome reporting).

---

