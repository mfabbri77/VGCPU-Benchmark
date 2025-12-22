# Chapter 10 — Observability (Logging, Metrics, Tracing, Alerts)

## Purpose

This chapter specifies the observability requirements of the benchmark suite: what must be logged, which metrics must be captured beyond benchmark timings, how tracing (if any) is handled, and how failures are surfaced. The goal is to support reproducibility, debugging, and trustworthy interpretation of results without contaminating timed benchmark measurements.

## 10.1 Observability Principles and Constraints

1. Observability mechanisms **MUST** not materially affect benchmark timing results.
2. Logging, metric aggregation, and report writing **MUST** occur outside timed measurement regions.
3. The system **MUST** provide deterministic, structured diagnostic output sufficient to troubleshoot:

   * IR validation issues,
   * backend initialization issues,
   * capability mismatches,
   * worker process failures (if isolation mode is used),
   * output generation issues.

## 10.2 Logging

### 10.2.1 Logging Model

1. The system **MUST** implement a logging facility with:

   * severity levels: `ERROR`, `WARN`, `INFO`, `DEBUG` (at minimum),
   * structured fields (key/value pairs),
   * deterministic message formats.
2. The system **MUST** support configuring log verbosity via CLI (e.g., `--log-level` and/or `--verbose`).
3. The default log level **SHOULD** be `INFO` with concise output.

### 10.2.2 Logging Destinations

1. The system **MUST** log to stdout/stderr for console visibility.
2. The system **MAY** support logging to a file in the output directory (opt-in).
3. If file logging is enabled, the log filename **MUST** be deterministic and sanitized (see Chapter 9 output safety).

### 10.2.3 Required Log Events and Fields

The system **MUST** emit log events for the following milestones, each including required structured fields:

1. **Run start**

   * fields: `run_id`, `schema_version`, `git_sha`, `os`, `arch`
2. **Manifest load/validate**

   * fields: `manifest_path`, `scene_count`, `validation_status`
3. **Backend discovery**

   * fields: `backend_id`, `compiled_in`, `version` (if available)
4. **Case planning**

   * fields: `case_count_total`, `case_count_execute`, `case_count_skip`, `case_count_fail_planned` (if any)
5. **Case start / case end**

   * fields: `backend_id`, `scene_id`, `config_id`, `decision`, `outcome`, `reason_codes[]`
6. **Fatal error / run end**

   * fields: `exit_code`, `summary_counts` (success/skip/fail), `output_dir`

### 10.2.4 Logging Restrictions in Timed Regions

1. The system **MUST NOT** emit logs within the timed measurement loop.
2. Any adapter debug logging **MUST** be disabled by default during timed iterations.
3. If adapter logging is enabled for diagnostics, the harness **MUST**:

   * mark the run metadata as “diagnostic mode,” and
   * warn the user that timings may be contaminated.

## 10.3 Metrics and Diagnostics

### 10.3.1 Primary Benchmark Metrics

1. The primary metrics are the benchmark results described in Chapters 2 and 6:

   * wall time statistics,
   * CPU time statistics,
   * sample counts and dispersion.
2. These primary metrics **MUST** be present in machine-readable outputs for all executed cases.

### 10.3.2 Secondary Diagnostics (Untimed)

The system **SHOULD** record secondary diagnostics per case, captured outside timed regions:

1. IR load time and validation time
2. PreparedScene construction time
3. Backend initialization time
4. Surface creation time
5. Optional memory diagnostics:

   * peak RSS snapshot before/after case (platform availability dependent),
   * allocation counters if supported.

**Normative constraints**

* Secondary diagnostics **MUST** be clearly labeled as “untimed diagnostics” and **MUST NOT** be mixed into primary timing stats.

### 10.3.3 Aggregation and Presentation

1. The reporting system **MUST** aggregate and present primary metrics per case.
2. The reporting system **SHOULD** present secondary diagnostics in a separate section/table (human-readable) and/or separate fields (JSON), not interleaved with primary timing columns in CSV unless explicitly configured.

## 10.4 Tracing

### 10.4.1 Tracing Scope

1. Distributed tracing is out of scope (no networked services).
2. The system **MAY** provide local tracing as a diagnostic tool (e.g., event spans for:

   * IR validation,
   * backend init,
   * warm-up,
   * measurement batch execution),
     provided it is disabled by default and does not run inside per-iteration timed loops.

### 10.4.2 Trace Output

1. If tracing is enabled, traces **MUST** be written to files in the output directory.
2. Trace formats **SHOULD** be standard (e.g., Chrome trace event format) to enable offline inspection.
3. Trace enablement **MUST** be recorded in run metadata.

## 10.5 Failure Surfacing and “Alerts”

Because the tool is local and offline, “alerts” are represented as exit codes, summary output, and optional CI signals.

1. The system **MUST** summarize case outcomes at end of run:

   * number of SUCCESS/SKIP/FAIL,
   * top failure reason codes (grouped),
   * list of failed cases (backend_id, scene_id).
2. The system **MUST** return non-zero exit codes when failures occur, per Chapter 6/8 policy.
3. The system **SHOULD** provide a CI-friendly mode (e.g., `--ci`) that:

   * suppresses non-essential logs,
   * ensures deterministic output paths,
   * fails the run on any `FAIL` outcome (configurable).

## 10.6 Observability Data in Output Schemas

1. Machine-readable outputs **MUST** include:

   * run-level summary counts,
   * per-case outcomes and reason codes.
2. If secondary diagnostics are enabled, outputs **MUST** include:

   * explicit `diagnostics_enabled=true`,
   * diagnostic fields separated from timing fields.

## Acceptance Criteria

1. Logging requirements define severity levels, configuration flags, destinations, and required event types with structured fields.
2. The specification forbids logging within timed measurement loops and requires marking diagnostic modes when enabled.
3. Primary benchmark metrics are distinguished from secondary diagnostics, with clear labeling and schema separation.
4. Optional tracing is specified as disabled-by-default, file-based, and recorded in metadata.
5. Failure surfacing includes end-of-run summaries and CI-friendly behavior with deterministic outputs.

## Dependencies

1. Chapter 6 — Interfaces (output schema fields, CLI options, error codes).
2. Chapter 8 — Runtime Behavior (timed boundaries, outcome classification).
3. Chapter 9 — Security, Privacy, and Compliance (output safety, metadata privacy constraints).

---

