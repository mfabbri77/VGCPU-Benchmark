<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Appendix C — Debugging & Incident Response Playbook

## Purpose

This appendix provides operational procedures for diagnosing failures, verifying CPU-only compliance, investigating semantic mismatches, and responding to release-impacting issues. It defines triage steps, diagnostic modes, and incident handling practices appropriate for a local benchmark tool distributed via GitHub Releases.

## C.1 Principles

1. Debugging workflows **MUST** avoid contaminating benchmark timings unless explicitly in diagnostic mode.
2. Every issue report **SHOULD** be reproducible with a minimal case descriptor:

   * BackendId, SceneId, ConfigurationId,
   * tool version (git sha/tag),
   * platform/arch,
   * benchmark policy parameters,
   * output schema version.
3. Diagnostics **MUST** be opt-in and recorded in run metadata.

## C.2 Common Failure Categories and First Checks

### C.2.1 Asset/IR Validation Failures

**Symptoms**

* `IR_VALIDATION_FAILED`, `IR_VERSION_UNSUPPORTED`, manifest hash mismatch.

**First checks**

1. Run `validate` and confirm:

   * manifest path,
   * scene count,
   * failing scene IDs.
2. Confirm SceneHash computed matches manifest.
3. Confirm IR major version matches runtime support.

**Actions**

* If IR hash mismatch: regenerate manifest hashes or restore correct IR assets.
* If version unsupported: use matching binary release or migrate IR assets.

### C.2.2 Backend Initialization Failures

**Symptoms**

* `BACKEND_INIT_FAILED`, `SURFACE_CREATE_FAILED`, backend missing (`BACKEND_NOT_AVAILABLE`).

**First checks**

1. Run `list` to confirm adapter availability (compiled-in).
2. Run `metadata` to capture:

   * backend version,
   * dependency detection/version strings,
   * CPU-only enforcement configuration.
3. Confirm correct CMake options were used to enable the backend.

**Actions**

* If missing: rebuild with adapter enabled, or use a binary that includes it.
* If init fails: enable `--log-level DEBUG` and capture error details.

### C.2.3 Render Failures at Runtime

**Symptoms**

* `RENDER_FAILED` during timed loop or warm-up.

**First checks**

1. Reduce to minimal:

   * one backend,
   * one scene,
   * minimal iterations (but keep warm-up).
2. Enable diagnostic mode:

   * `--log-level DEBUG`
   * optional: render-to-image diagnostic if implemented.
3. Confirm scene’s required features match backend capabilities; ensure no unintended fallback.

**Actions**

* If backend reports unsupported feature despite planning: verify capability reporting correctness.
* If crash: run under sanitizer build or isolation mode to contain crash and capture logs.

### C.2.4 Output/Schema Problems

**Symptoms**

* `OUTPUT_WRITE_FAILED`, malformed JSON/CSV, missing required fields.

**First checks**

1. Confirm output directory exists and is writable.
2. Verify schema_version presence and required fields.
3. Check whether overwrite policy caused conflicts.

**Actions**

* Use a fresh output directory; ensure identifiers are sanitized.
* Re-run with `--summary text` only to isolate writer issues (if supported).

## C.3 CPU-Only Compliance Verification Playbook

1. Run with `--metadata` or `--cpu-only-audit` (if provided) to print:

   * chosen surface type,
   * GPU disabling flags,
   * backend-reported renderer/device (best-effort).
2. Confirm the adapter reports `cpu_only=true` and a CPU surface identifier.
3. If suspicious (e.g., unexpectedly high performance or OS-level acceleration concerns):

   * run on a machine without a discrete GPU (if available) as a control,
   * compare wall vs CPU time patterns (large divergence may indicate scheduling effects, not necessarily GPU).
4. For backends with known multiple pipelines, explicitly pin the CPU pipeline in adapter configuration and record it.

**Incident threshold**

* If GPU use is confirmed or strongly suspected, the release/run **MUST** be considered invalid for CPU-only reporting.

## C.4 Semantic Mismatch Investigation Playbook

1. Identify the smallest scene that demonstrates mismatch (or create a dedicated sentinel scene variant).
2. Enable diagnostic rendering output (if implemented) for the specific case only.
3. Compare across backends using:

   * visual inspection, and/or
   * tolerance-based image compare (non-gating unless strict mode enabled).
4. Check adapter mapping for:

   * fill rule handling,
   * dash offset semantics,
   * miter limit behavior,
   * gradient stop interpolation/positions,
   * transform concatenation order,
   * premultiplication assumptions.
5. Document deviation:

   * add adapter note entry,
   * update capability matrix if appropriate,
   * optionally adjust scenes to avoid ambiguous edge cases in “standard” suite.

## C.5 Measurement Noise and Variance Triage

1. Confirm the benchmark policy used (warm-up, iters, min-ms, repetitions).
2. Increase stability:

   * raise minimum measurement duration,
   * increase repetitions,
   * ensure consistent CPU frequency/power mode,
   * close background applications.
3. Compare dispersion metrics:

   * high p90/p50 ratio indicates noise; prefer medians over means.
4. If variance persists:

   * use isolation mode (process) to reduce cross-case interference,
   * pin threads/affinity (if supported by PAL/harness), and record it.

## C.6 Isolation Mode (Process) Triage

If isolation mode is enabled and failures occur:

1. Check for `WORKER_CRASH` or `WORKER_TIMEOUT`.
2. Re-run the failing case in single-process mode to determine if the issue is isolation-specific.
3. Capture worker stderr logs and include them in the triage bundle.
4. Validate IPC protocol version match between parent and worker; reject mismatches.

## C.7 Triage Bundle Procedure

When reporting an issue, the reporter **SHOULD** produce a triage bundle containing:

1. Run command line (or serialized ExecutionPlan).
2. JSON results file (and CSV if generated).
3. Human-readable summary.
4. Run metadata output.
5. Logs (with log level noted).
6. Optional traces and diagnostic images (if enabled).
7. System info:

   * OS/arch,
   * CPU model,
   * tool version (tag/sha),
   * backend versions.

The tool **SHOULD** support a flag (e.g., `--triage-bundle`) that writes the above into a single directory or archive under the output directory.

## C.8 Incident Response (Project Maintainers)

### C.8.1 Incident Types

1. **Release Integrity Incident**: wrong artifacts, missing assets, corrupted outputs, incorrect checksums.
2. **CPU-only Violation Incident**: GPU path enabled or suspected for CPU-only results.
3. **Semantic Contract Regression**: adapter change causes broad semantic drift.
4. **Widespread Runtime Failure**: a release fails for many users/platforms.

### C.8.2 Response Steps

1. **Acknowledge and Triage**

   * assign owner,
   * reproduce using triage bundle,
   * classify incident type and severity.
2. **Containment**

   * if release is affected: mark release as deprecated and warn users.
3. **Fix**

   * implement patch release, including updated metadata and tests.
4. **Postmortem**

   * document root cause,
   * add regression tests (IR fixtures, adapter smoke tests, packaging checks),
   * update documentation and release checklist.

### C.8.3 Rollback Actions

1. For critical release issues, maintainers **MUST**:

   * publish a patch release, and
   * update changelog/release notes to discourage use of the faulty release.
2. For semantic regressions, maintainers **SHOULD** revert offending change or gate it behind an opt-in flag until validated.

## Acceptance Criteria

1. The playbook provides step-by-step triage for IR validation failures, backend init failures, runtime render failures, and output/schema issues.
2. CPU-only compliance verification steps are explicit and rely on adapter metadata and best-effort checks.
3. Semantic mismatch investigation steps are provided and tie back to IR semantic contract areas.
4. The triage bundle contents are specified to enable reproducible issue reports.
5. Maintainer incident response procedures include acknowledgement, containment, fix, and postmortem steps.

## Dependencies

1. Chapter 8 — Runtime Behavior (outcomes, reason codes, isolation mode behavior).
2. Chapter 9 — Security, Privacy, and Compliance (safe diagnostics, metadata privacy).
3. Chapter 13 — Risk Management and Rollback Strategy (rollback expectations and release handling).

---

