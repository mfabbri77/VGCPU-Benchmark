# Appendix D — Coding Standards & Conventions

## Purpose

This appendix defines coding standards and conventions for implementing the benchmark suite in a maintainable, cross-platform, performance-conscious manner. It sets expectations for formatting, naming, module boundaries, error handling, determinism, and performance hygiene—especially around timed benchmark regions.

## D.1 Language and Compilation Standards

1. The codebase **MUST** compile as **C++20**.
2. The project **MUST** compile on:

   * GCC/Clang (Linux),
   * MSVC (Windows),
   * Clang (macOS),
     subject to platform toolchain availability.
3. The project **SHOULD** treat warnings as errors in CI for project-owned code, with adapter third-party warnings handled separately where feasible.

## D.2 Formatting and Style

1. The project **SHOULD** use `clang-format` for automatic formatting.
2. A `.clang-format` file **MUST** be included at repository root.
3. Formatting **MUST** be enforced in CI (format check) or via pre-commit guidance.

**Rationale**: consistent formatting reduces review overhead and avoids style drift.

## D.3 Naming Conventions

1. **Types** (classes/structs/enums) **SHOULD** use `PascalCase`.
2. **Functions/methods** **SHOULD** use `camelCase` or `PascalCase` consistently across the project (choose one and enforce).
3. **Variables** **SHOULD** use `snake_case` or `camelCase` consistently across the project (choose one and enforce).
4. **Constants** **SHOULD** use `kPascalCase` or `SCREAMING_SNAKE_CASE` consistently (choose one and enforce).
5. **Identifiers emitted to users** (SceneId/BackendId/reason codes) **MUST** be stable, ASCII, and use a consistent delimiter strategy (`/` for namespaces in IDs; `:` for reason code qualifiers).

## D.4 Module Boundaries and Dependencies

1. Modules defined in Chapter 7 **MUST** remain acyclic in dependencies.
2. The `adapters` module **MUST NOT** be depended upon by `ir`; `ir` must remain backend-agnostic.
3. The `pal` module **MUST** be the only module using OS-specific APIs directly.
4. Cross-module interfaces **MUST** be defined in a small set of public headers/contracts per module.

## D.5 Error Handling and Status Types

1. Cross-module APIs **MUST** return structured status results:

   * `Status` for operations without return values,
   * `Result<T>` (or equivalent) for operations returning values.
2. Status objects **MUST** include:

   * stable `code` enum,
   * human-readable `message`,
   * optional structured `details`.
3. Exceptions **SHOULD NOT** be thrown across module boundaries.
4. Exceptions **MUST NOT** be used inside timed measurement loops.
5. Errors **MUST** be translated into stable reason codes for case outcomes (Chapter 8).

## D.6 Determinism and Ordering Conventions

1. Any ordering that affects:

   * case planning,
   * execution order,
   * output serialization,
     **MUST** be deterministic.
2. Iteration over unordered containers **MUST** be normalized via sorting before influencing:

   * case lists,
   * report order,
   * “list” command outputs.
3. Hashing **MUST** use the canonical IR bytes as input (Chapter 5) and **MUST** be consistent across platforms.

## D.7 Performance Hygiene (Benchmark Integrity)

1. Code executed within the timed section **MUST** be minimal and stable across iterations.
2. Logging **MUST NOT** occur in timed loops (Chapter 10).
3. Memory allocations in timed loops **SHOULD** be avoided; if unavoidable, they **MUST** be documented per adapter or harness component.
4. Expensive setup work (IR parsing, scene preparation, surface creation) **MUST** occur outside timed loops.
5. Debug-only checks **SHOULD** be compiled out or disabled in release benchmarking builds, except for safety-critical validation performed outside timed regions.

## D.8 Threading and Concurrency Conventions

1. The harness **MUST** treat backend threading behavior as part of configuration/metadata.
2. If the harness controls threading, thread counts **MUST** be explicit, deterministic, and recorded in metadata.
3. Shared mutable state across threads **MUST** be avoided in the harness; if present, it **MUST** be documented and tested.

## D.9 Testing Conventions

1. Unit and integration tests **MUST** be organized to reflect module boundaries.
2. Test names **SHOULD** encode the module and behavior under test (e.g., `IrValidator.RejectsInvalidOffsets`).
3. Tests **MUST** be deterministic and avoid reliance on timing thresholds (Appendix B).

## D.10 Documentation and Comments

1. Public interfaces **MUST** be documented with:

   * purpose,
   * preconditions,
   * postconditions,
   * error codes.
2. Performance-sensitive code **SHOULD** include comments explaining:

   * why it is in a hot path,
   * what invariants enable performance.
3. Any deviation from IR semantic contract in adapters **MUST** be documented and reflected in metadata.

## Acceptance Criteria

1. The appendix defines enforceable conventions for formatting, naming, module boundaries, error handling, determinism, and timed-loop performance hygiene.
2. Conventions explicitly prohibit exceptions and logging in timed loops.
3. Deterministic ordering requirements cover case planning, list outputs, and report serialization.
4. The standards align with cross-platform CMake/C++20 constraints and the adapter-driven architecture.

## Dependencies

1. Chapter 7 — Component Design (module boundaries).
2. Chapter 8 — Runtime Behavior (timed-loop invariants, outcomes).
3. Chapter 10 — Observability (logging restrictions).

---

