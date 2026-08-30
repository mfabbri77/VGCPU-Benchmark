# Chapter 8 — Tooling (Formatting, Linting, Static Analysis, Scripts, Observability Support)

This chapter specifies repo tooling that enforces the architectural and quality rules defined in Ch1–Ch7, aligned with:
- `code_style_and_tooling.md`
- `temp_dbg_policy.md`
- `testing_strategy.md`
- `ci_reference.md`
- `repo_layout_and_targets.md`

## [BUILD-13] Tooling scope and goals
- [REQ-112] The repo MUST be “tooling-complete”:
  - contributors can run format/lint/tests/package locally with preset-driven commands,
  - CI can enforce the same gates deterministically.
- [REQ-113] Tooling MUST be cross-platform (Windows/macOS/Linux) and avoid requiring proprietary IDE features.
- [REQ-114] Tooling MUST not distort benchmark results:
  - instrumentation and heavy checks are OFF by default in Release benchmarking runs.

## [BUILD-14] Canonical repo directories (tooling-related)
Per `repo_layout_and_targets.md` conventions:
- [REQ-115] Add/standardize directories:
  - `/cmake/` (CMake modules + deps versions + helpers)
  - `/tools/` (scripts: lint checks, schema validators, CI linters)
  - `/docs/` (user docs + moved legacy blueprint docs)
  - `/third_party/` (licenses/notices aggregation)
  - `/tests/` (new test sources; unit + contract)
- [DEC-TOOL-01] Move existing repo `/blueprint` (legacy docs) to `/docs/legacy_blueprint` and treat `/blueprint` as the canonical modernization spec directory.
  - Rationale: resolves collision with required blueprint layout ([DEC-ARCH-05]).
  - Alternatives: rename canonical blueprint dir (not allowed by this blueprint’s repo convention).
  - Consequences: update README links and contributor docs.

## [BUILD-15] Formatting (clang-format)
### [REQ-116] clang-format policy (mandatory)
- [REQ-116-01] Repo MUST include `.clang-format` at root.
- [REQ-116-02] CI MUST enforce `format_check` (see Ch7 [REQ-103-01]).
- [REQ-116-03] Tool version expectation MUST be documented:
  - “clang-format 17+” (matches `code_style_and_tooling.md` guidance).

### [DEC-TOOL-02] Formatting style baseline
- Base style: **LLVM** with explicit overrides for:
  - column limit (e.g., 100),
  - pointer alignment and reference alignment,
  - include sorting ON.
- Rationale: predictable defaults; minimal churn across platforms.
- Alternatives: Google style.
- Consequences: `format`/`format_check` targets use `clang-format` from PATH (CI installs LLVM).

### Tool commands
- [REQ-117] Provide scripts for contributors:
  - `tools/format_all.(sh|ps1)` → runs `cmake --build --target format` via presets.
  - `tools/format_check.(sh|ps1)` → runs `cmake --build --target format_check`.

## [BUILD-16] Static analysis (clang-tidy)
### [REQ-118] clang-tidy policy (mandatory in CI for VGCPU code)
- [REQ-118-01] Repo MUST include `.clang-tidy` at root.
- [REQ-118-02] CI MUST run `lint` in at least one Linux job (see Ch7 [REQ-103-02]).
- [REQ-118-03] Third-party code MUST be excluded:
  - no clang-tidy on `_deps`, `third_party`, fetched sources.

### [DEC-TOOL-03] Pragmatic checks set (low-churn)
Enable (initial baseline):
- `bugprone-*` (excluding overly noisy checks if needed)
- `performance-*`
- `readability-*` (selectively; avoid churn-heavy ones)
- `modernize-use-nullptr`
- `modernize-use-override`
- `modernize-deprecated-headers`
- `cppcoreguidelines-*` (select subset: `interfaces-global-init`, `pro-type-member-init`, `narrowing-conversions` if feasible)

Disable initially (explicitly):
- `modernize-use-trailing-return-type` (churn)
- any check that rewrites large swaths or conflicts with “internal ABI not stable but clean headers” policy.

Rationale: aligns with `code_style_and_tooling.md` incremental adoption guidance.
Consequences: we may maintain a “known warnings baseline” file and fail CI only on new warnings.

### [REQ-119] Lint command contract
- `cmake --build --preset <preset> --target lint` MUST:
  - use `compile_commands.json`,
  - analyze only VGCPU sources,
  - return non-zero on new warnings in CI mode.

## [BUILD-17] Include hygiene + dependency boundary enforcement
### [REQ-120] Forbidden include rules (architectural enforcement)
To prevent “accidental public API leaks” and layering violations:
- [REQ-120-01] `vgcpu_core` MUST NOT include headers from:
  - `src/adapters/**`, `include/vgcpu/internal/adapters.h`, or any backend headers.
- [REQ-120-02] `vgcpu_reporting` MUST NOT include adapter/backend headers.
- [REQ-120-03] `vgcpu_adapters` MUST NOT include CLI-only headers.

### [DEC-TOOL-04] Enforcement mechanism
- Implement a repo-local include linter script:
  - `tools/check_includes.py`
  - Inputs: `compile_commands.json` + file list under `src/` and `include/`
  - Rules: match forbidden include path patterns per target ownership map (Ch2 [ARCH-06], Ch3 [ARCH-11]).
- Integrate into CMake gate target:
  - `cmake --build --preset ci --target check_includes`

Rationale: deterministic and cross-platform; avoids relying on specialized external tools.
Alternatives: IWYU; clang-scan-deps graph analysis.
Consequences: keep patterns conservative to avoid false positives; document exceptions via allowlist file.

## [BUILD-18] TEMP-DBG enforcement (mandatory gate)
### [REQ-121] TEMP-DBG compliance
Per `temp_dbg_policy.md`, temporary debug code MUST be marked exactly:
```cpp
// [TEMP-DBG] START <reason> <owner> <date>
...temporary debug code...
// [TEMP-DBG] END
````

### [REQ-122] Repository-wide detection

* [REQ-122-01] Provide `tools/check_no_temp_dbg.py` that:

  * scans tracked source files (`.c/.cc/.cpp/.h/.hpp/.inl/.mm`) under `src/`, `include/`, `tests/`, `tools/`,
  * fails if any `[TEMP-DBG]` marker is present.
* [REQ-122-02] Wire script into the CMake target `check_no_temp_dbg` (Ch7 [REQ-103-03]).
* [REQ-122-03] CI MUST run `check_no_temp_dbg` on every PR.

## [BUILD-19] Logging/observability support tooling

(This section supports [REQ-02] but avoids runtime overhead.)

### [REQ-123] Structured log schema helpers

* [REQ-123-01] Provide a small schema reference doc:

  * `/docs/log_events.md` listing event names and required fields (`run_id`, `scene_id`, `backend`, `schema_version`, `tool_version`).
* [REQ-123-02] Provide a validator tool:

  * `tools/validate_jsonl_logs.py` that checks:

    * each line is valid JSON,
    * required fields exist for known event types,
    * timestamps and levels are well-formed.

### [DEC-TOOL-05] Logging framework choice (internal)

* Use a lightweight, dependency-minimal approach:

  * internal logger with sinks: console (pretty) and jsonl (structured),
  * avoid heavy third-party logging deps in v0.2.0 to keep build simple.
* Rationale: benchmark tool; dependencies should stay minimal.
* Alternatives: spdlog; fmt-based logger.
* Consequences: if fmt/spdlog is adopted later, do it via CR and preserve log schema stability.

## [BUILD-20] Report schema validation tooling

Supports [REQ-48..50].

* [REQ-124] Provide schema validators:

  * `tools/validate_report_json.py` validates required keys and `schema_version`.
  * `tools/validate_report_csv.py` validates header order and schema header comment.
* [REQ-125] CI MUST run validators on artifacts produced by smoke runs (Tier-1).

## [BUILD-21] Workflow and preset linting

* [REQ-126] Provide `tools/lint_workflows_presets.py` that fails if:

  * any workflow uses raw `cmake -D...` configuration instead of `--preset`,
  * required presets (`dev/release/ci/asan/ubsan/tsan`) are missing.
* [REQ-127] Add a CI job (fast) that runs this linter.

## [BUILD-22] Developer ergonomics

### Editor/IDE support (non-invasive)

* [REQ-128] Provide `.editorconfig` (whitespace, newline, UTF-8).
* [REQ-129] Ensure `compile_commands.json` is generated in `dev` and `ci` presets ([REQ-93-01]).
* [REQ-130] Provide `docs/developer_quickstart.md` with:

  * “clone → configure/build/test/package” commands using presets,
  * how to enable optional backends,
  * how to run smoke benchmarks locally.

### [DEC-TOOL-06] Pre-commit hooks (optional)

* Provide `tools/install_git_hooks.(sh|ps1)` to install local hooks for:

  * `format_check`,
  * `check_no_temp_dbg`,
  * fast unit tests subset.
* Rationale: reduce CI roundtrips without making hooks mandatory.
* Alternatives: mandatory pre-commit framework.
* Consequences: CI remains the source of truth; hooks are best-effort.

## [BUILD-23] Tooling tests (enforcement)

These tests ensure tooling actually enforces rules.

* [TEST-40] `tool_check_no_temp_dbg_finds_marker`:

  * inject a fixture file with marker in a temp dir; ensure script fails.
* [TEST-41] `tool_check_includes_blocks_violation`:

  * fixture includes forbidden header; ensure script fails with actionable message.
* [TEST-42] `tool_validate_report_json_minimal`:

  * validate a minimal valid JSON report passes; missing `schema_version` fails.
* [TEST-43] `tool_validate_report_csv_header`:

  * validate canonical CSV header order; mutated order fails.
* [TEST-44] `tool_lint_workflows_presets`:

  * ensure workflows reference `--preset` only (fixture-based).

## [BUILD-24] Traceability notes

* [BUILD-24-01] This chapter defines [REQ-112..130] and [TEST-40..44].
* [BUILD-24-02] Ch7 defines how tooling targets are exposed via CMake and invoked in CI.
* [BUILD-24-03] Ch9 defines governance: any tooling policy change requires CR + updates to these tests.

