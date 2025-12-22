# Workplan: VGCPU-Benchmark Development

This document outlines the incremental development plan for the Vector Graphics CPU Benchmark suite.

## Development Conventions

All steps in this workplan **MUST** follow **ALL** the conventions defined in the Blueprint, in particular:

1. **Atomic Step Workflow** (Blueprint §12.5): Each step must be completed atomically following the sequence: Implement → Test → Remove Debug Code → Update WORKPLAN.md → Commit and Push.
2. **Temporary Debug Code** (Blueprint §D.11): Any debug code added during development must use the standardised marker format and be removed before committing.
3. **Blueprint Traceability** (Blueprint §D.12): All implemented features must include source code comments referencing the corresponding Blueprint chapter/section.

---

## Phase 1: Foundation & Infrastructure
**Goal**: Establish the repository structure, build system, and core interfaces.

- [x] **Step 1.1: Repository Setup** ✅
    - [x] Initialize Git repository.
    - [x] Setup `CMakeLists.txt` root with `FetchContent` capabilities.
    - [x] Create directory structure: `src/{cli,harness,ir,pal,adapters,assets}`.
    - [x] Define `.clang-format` and `.gitignore`.

- [x] **Step 1.2: Core Interfaces & PAL** ✅
    - [x] Implement `pal` module: Monotonic timer, CPU timer (Linux/Windows/Mac).
    - [x] Define `IBackendAdapter` abstract interface (C++).
    - [x] Define `CapabilitySet` and `Status` structs.

- [x] **Step 1.3: IR Runtime (Basic)** ✅
    - [x] Implement `ir` module headers: `Instruction`, `Command` structs.
    - [x] Implement `IrLoader` (Mock/Placeholder initially, then binary parser).
    - [x] Implement `Scene` and `PreparedScene` classes.

- [x] **Step 1.4: CLI & Harness Skeleton** ✅
    - [x] Implement `cli` argument parsing (generic flags).
    - [x] Implement `harness` loop (warmup, measure, report stub).
    - [x] Register a "Null" or "Debug" backend for testing the harness flow.

**Milestone M1**: CLI runs, loads a dummy scene, runs a loop on a Null backend, and prints timing. ✅

---

## Phase 2: First Integrations & Scene Management
**Goal**: Get real pixels rendered on a simple backend and establish the scene pipeline.

- [x] **Step 2.1: PlutoVG Backend (The "Simple" Pilot)** ✅
    - [x] Integrate PlutoVG via `FetchContent` (v1.3.2).
    - [x] Implement `PlutoVGAdapter`.
    - [x] Verify basic shapes rendering.

- [ ] **Step 2.2: IR Binary Format & Asset Pipeline**
    - [ ] Finalize IR Binary spec (Chapter 5) in code (`ir_format.h`).
    - [ ] Write a simple "SVG-to-IR" compiler (python or cpp) or hand-craft binary IR samples.
    - [ ] Create `assets/scenes` directory with initial test scenes (rects, circles).

- [x] **Step 2.3: Cairo Backend (The "Reference")** ✅
    - [x] Integrate Cairo (System or PkgConfig).
    - [x] Implement `CairoAdapter`.
    - [x] Validate visual output against PlutoVG (manual check).

- [ ] **Step 2.4: Scene Registry & Capability Filtering**
    - [ ] Implement `assets` module: JSON manifest parsing.
    - [ ] Connect `harness` to `SceneRegistry`.
    - [ ] Implement filtering logic (skip Cairo-unsupported features if any).

**Milestone M2**: Benchmark runs PlutoVG and Cairo on real IR assets and outputs comparable timings.

---

## Phase 3: High-Performance Backends
**Goal**: Integrate the heavyweights (Blend2D, Skia) and refine measurement precision.

- [ ] **Step 3.1: Blend2D Integration**
    - [ ] Integrate Blend2D + AsmJit (v0.21.2).
    - [ ] Implement `Blend2DAdapter`.
    - [ ] Verify JIT is functional/enabled (CPU-only).

- [ ] **Step 3.2: Skia Integration**
    - [ ] Setup build infrastructure for Skia (m116/GN or prebuilt).
    - [ ] Implement `SkiaAdapter` (Raster Surface).
    - [ ] Handle Skia-specifics (Premul alpha conversion if needed).

- [ ] **Step 3.3: Measurement Refinement**
    - [ ] Validate CPU-time measurements on all platforms.
    - [ ] add `--isolation process` support (optional/stretch).
    - [ ] Ensure overhead is minimized (profile the harness).

**Milestone M3**: "Big Three" (Cairo, Blend2D, Skia) are comparable.

---

## Phase 4: Diversity & Rust Bridge
**Goal**: Integrate remaining C++ backends and Rust-based engines.

- [ ] **Step 4.1: ThorVG & AmanithVG**
    - [ ] Integrate ThorVG (SW Engine).
    - [ ] Integrate AmanithVG (SDK path).
    - [ ] Implement adapters.

- [ ] **Step 4.2: Rust Bridge Infrastructure**
    - [ ] Setup `Corrosion` CMake integration.
    - [ ] Create `backends/rust_bridge` crate.
    - [ ] Implement `Raqote` integration.

- [ ] **Step 4.3: Vello (Experimental)**
    - [ ] Attempt Vello integration via Rust Bridge.
    - [ ] Mark as experimental/optional if unstable.

---

## Phase 5: Reporting, Packaging & Release
**Goal**: Polish for public consumption.

- [ ] **Step 5.1: Reporting Module**
    - [ ] Implement JSON Schema output (versioned).
    - [ ] Implement CSV output.
    - [ ] Implement specific Dependency Pinning metadata collection.

- [ ] **Step 5.2: CI/CD Pipeline**
    - [ ] Setup GitHub Actions for Linux/Windows/Mac.
    - [ ] Ensure build caching for heavy deps (Skia, Qt).

- [ ] **Step 5.3: Documentation & Release**
    - [ ] Write `BUILD.md` and `USAGE.md`.
    - [ ] Prepare release script (assets bundle).
    - [ ] Tag v0.1.0.

**Final Milestone**: v0.1.0 Release.
