<!-- Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com) -->

# Chapter 5 — Data Design (Domain Model, Storage, Migrations)

## Purpose

This chapter specifies the data model and storage formats for the benchmark suite, including the scene IR, manifests, capability descriptors, result schemas, and versioning/migration rules. It defines the canonical representations required for deterministic replay and reproducible reporting.

## 5.1 Domain Model

### 5.1.1 Core Entities

1. **Scene**

   * **SceneId**: Stable identifier string (ASCII), unique within the suite (e.g., `strokes/dashes_01`).
   * **IR Asset**: Canonical serialized IR payload representing the scene.
   * **Requirements**: Declared feature requirements for compatibility checks.
   * **Defaults**: Output width/height, background clear policy, and any scene-level settings.
   * **SceneHash**: Content hash of the canonical IR payload (algorithm specified in §5.4).

2. **Backend**

   * **BackendId**: Stable identifier string (ASCII) (e.g., `skia_raster`, `cairo_image`).
   * **Adapter Module**: Compiled adapter implementation.
   * **Capabilities**: Feature support flags and constraints.
   * **BackendMetadata**: Version/build/configuration details emitted at runtime.

3. **BenchmarkCase**

   * Tuple: (**SceneId**, **BackendId**, **ConfigurationId**).
   * Includes derived compatibility state (execute/skip/fallback) and reasons.

4. **Run**

   * A single invocation of the benchmark CLI, producing:

     * Run metadata (environment, toolchain, dependency versions),
     * A set of benchmark case results.

5. **Result**

   * Per benchmark case:

     * Timing samples and/or aggregates,
     * Statistics (p50, dispersion),
     * Execution outcome (success/skip/fail) with reason codes,
     * Scene/Backend identifiers and hashes,
     * Configuration and policy parameters.

### 5.1.2 Normative Naming and Identity Requirements

1. SceneId and BackendId **MUST** be stable across releases unless a breaking rename policy is followed (see §5.5.3).
2. SceneHash **MUST** be computed from the canonical IR payload bytes, not from any authoring representation.
3. Result outputs **MUST** include both SceneId and SceneHash, and both BackendId and backend version identifiers.

## 5.2 Scene Manifest and Registry Data

### 5.2.1 Manifest Format

1. The suite **MUST** include a scene manifest that enumerates all scenes and their metadata.
2. The manifest **MUST** be machine-readable and diff-friendly (e.g., JSON or YAML).
3. The manifest **MUST** include, per scene:

   * `scene_id`
   * `ir_path` (relative to repository or install layout)
   * `scene_hash` (computed as specified)
   * `ir_version` (MAJOR.MINOR.PATCH)
   * `default_width`, `default_height`
   * `required_features` (capability requirements)
   * Optional: `tags` (categorization), `notes` (non-normative)

### 5.2.2 Required Features Schema (Logical)

The `required_features` object **MUST** be able to express requirements at least for:

1. Fill rules: `nonzero`, `evenodd`
2. Stroke caps: `butt`, `round`, `square`
3. Stroke joins: `miter`, `round`, `bevel`
4. Dash support: boolean
5. Gradients: `linear`, `radial`
6. Clipping: boolean (only if any scenes use clipping)
7. Compositing: `source_over` (baseline; others optional and future)

### 5.2.3 Manifest Validation Rules

1. At startup, the harness **MUST** validate that each manifest entry references an existing IR asset.
2. The harness **MUST** validate that `scene_hash` matches the computed hash of the IR asset bytes.
3. If validation fails, the harness **MUST** treat it as a fatal error unless an explicit “ignore manifest validation” debug flag is set.

## 5.3 Intermediate Representation (IR) Data Design

### 5.3.1 IR Goals

1. The IR **MUST** be:

   * Deterministic to parse and replay,
   * Efficient to interpret (minimizing per-command overhead),
   * Versioned and self-describing enough for validation.
2. The IR **MUST** allow building a “Prepared Scene” that is immutable during benchmarking iterations.
3. The IR **MUST** support representing all in-scope features from Chapter 1.

### 5.3.2 IR Binary Layout

The canonical IR **MUST** use the following little-endian binary layout. Types used: `u8`, `u16` (2 bytes), `u32` (4 bytes), `f32` (IEEE 754).

**1. File Header** (Fixed 16 bytes)

| Offset | Type | Field | Value / Description |
|---|---|---|---|
| 0 | u8[4] | Magic | `'V', 'G', 'I', 'R'` (Vector Graphics Intermediate Rep) |
| 4 | u8 | MajorVer | `1` (Current Major) |
| 5 | u8 | MinorVer | `0` |
| 6 | u16 | Reserved | `0x0000` |
| 8 | u32 | TotalSize | Total file size in bytes |
| 12 | u32 | SceneCRC | CRC32 of the scene content (excluding header) |

**2. Sections**

The body consists of sequential sections. Each section starts with a header:

| Offset | Type | Description |
|---|---|---|
| 0 | u8 | SectionTypeID |
| 1 | u8 | Reserved |
| 2 | u32 | SectionLength (bytes, including this header) |
| 6 | ... | Payload |

**Section Type IDs:**

* `0x01`: **Info** (Metadata using key-value pairs)
* `0x02`: **Paint** (Color/Gradient table)
* `0x03`: **Path** (Path geometry table)
* `0x04`: **Command** (The rendering command stream)
* `0xFF`: **Extension**

### 5.3.3 IR Opcode Reference

The **Command Section** payload consists of a sequence of commands. Each command begins with a `u8` Opcode, followed by fixed arguments.

**Opcode Encoding:**

| Opcode | Name | Args | Description |
|---|---|---|---|
| `0x00` | `End` | - | End of stream |
| `0x01` | `Save` | - | Push state (matrix, clip, paints) |
| `0x02` | `Restore` | - | Pop state |
| `0x10` | `Clear` | `rgba:u32` | Clear canvas (RGBA8 premul) |
| `0x20` | `SetMatrix` | `m:f32[6]` | Set current transform |
| `0x21` | `ConcatMatrix` | `m:f32[6]` | Multiply current transform |
| `0x30` | `SetFill` | `paint_id:u16`, `rule:u8` | Set fill paint & rule (0=NonZero, 1=EvenOdd) |
| `0x31` | `SetStroke` | `paint_id:u16`, `width:f32`, `opts:u8` | Set stroke paint & params (opts: Cap/Join) |
| `0x40` | `FillPath` | `path_id:u16` | Fill path at index |
| `0x41` | `StrokePath` | `path_id:u16` | Stroke path at index |


### 5.3.3 IR Command Stream (Logical Model)

**Normative Replay Rules:**
* The stream **MUST** be replayed sequentially.
* `paint_id` and `path_id` are 0-based indices into the respective Paint and Path sections.
* `Save`/`Restore` depth **MUST** be limited (e.g., max 64) to prevent stack overflow, enforced by the validator.

### 5.3.4 Path Section Layout (Type `0x03`)

The Path Section contains `N` path definitions.

| Offset | Type | Description |
|---|---|---|
| 0 | u32 | PathCount `N` |
| 4 | u32[N] | Offsets to each Path Data block (relative to section start) |
| ... | ... | Path Data Blocks |

**Path Data Block Format:**

| Type | Field | Description |
|---|---|---|
| u16 | VerbCount | Number of verbs |
| u16 | PointCount | Number of points |
| u8[] | Verbs | Array of verbs (see table) |
| f32[]| Points | Array of (x, y) coordinates |

**Verb Codes (u8):**
* `0`: MoveTo (1 pt)
* `1`: LineTo (1 pt)
* `2`: QuadTo (2 pts: control, end)
* `3`: CubicTo (3 pts: c1, c2, end)
* `4`: Close (0 pts)

### 5.3.5 Paint Section Layout (Type `0x02`)

The Paint Section contains `N` paint definitions.

| Offset | Type | Description |
|---|---|---|
| 0 | u32 | PaintCount `N` |
| 4 | ... | Paint Entries (variable length) |

**Paint Entry Format:**

* **Type** (`u8`): `0`=Solid, `1`=Linear, `2`=Radial
* **Payload**:
    * **Solid**: `u32` (RGBA8 premul)
    * **Linear**:
        * `f32[2]` Start (x,y)
        * `f32[2]` End (x,y)
        * `u8` StopCount
        * `Stops`: `(f32 offset, u32 color)` array
    * **Radial**:
        * `f32[2]` Center (x,y)
        * `f32` Radius
        * `u8` StopCount
        * `Stops`: Array as above

### 5.3.6 Stroke Options (`u8` bitfield)

Used in `SetStroke` opcode:
* Bits 0-1: **Cap** (0=Butt, 1=Round, 2=Square)
* Bits 2-3: **Join** (0=Miter, 1=Round, 2=Bevel)
* Bits 4-7: Reserved (0)

### 5.3.7 Stroke Parameters Representation

A stroke parameter record **MUST** include:

1. Width (float32)
2. Cap (enum)
3. Join (enum)
4. Miter limit (float32)
5. Dash pattern:

   * Array of float32 dash lengths (even count preferred but not required if backend supports odd-length semantics)
   * Dash offset (float32)

**Normative stroke rules**

* Width **MUST** be non-negative; zero-width behavior **MUST** be defined (either no-op or hairline policy) and recorded in semantic contract.
* Dash lengths **MUST** be non-negative; all-zero patterns **MUST** be rejected as invalid.

## 5.4 Hashing, Versioning, and Metadata Fields

### 5.4.1 Hashing

1. SceneHash **MUST** be computed as **SHA-256** over the canonical IR payload bytes.
2. The emitted hash **MUST** be the lowercase hex encoding of the SHA-256 digest.

### 5.4.2 IR Versioning Rules

1. IR versioning **MUST** follow MAJOR.MINOR.PATCH.
2. A runtime **MUST** reject IR payloads with an unsupported MAJOR version.
3. MINOR increments **MAY** add new optional sections/opcodes while preserving backward compatibility.
4. PATCH increments **MUST** be used only for clarifications that do not change binary layout or semantics.

### 5.4.3 Result Schema Versioning

1. Machine-readable result schemas **MUST** include a `schema_version` field (MAJOR.MINOR).
2. Schema MAJOR changes **MUST** be reserved for breaking changes in field meaning or required fields.
3. Schema MINOR changes **MAY** add fields but **MUST NOT** remove or reinterpret existing required fields.

## 5.5 Storage Layout and Migrations

### 5.5.1 Repository and Install Layout (Logical)

1. The project **MUST** define a stable layout for:

   * `scenes/manifest.(json|yaml)`
   * `scenes/ir/<scene_id>.irbin` (or equivalent naming convention)
   * `schemas/` for result schema definitions
   * `reports/` optional templates or generators (if shipped)
2. Binary releases **MUST** include the scene manifest and IR assets required to run the included benchmark suite.

### 5.5.2 IR Migration Policy

1. If IR MAJOR version changes, the project **MUST** provide:

   * A migration guide, and
   * Either a converter tool or a parallel set of IR assets for the new version.
2. The harness **MAY** support multiple IR MAJOR versions concurrently only if it does not compromise determinism or maintainability; otherwise it **MUST** support exactly one MAJOR version at a time.

### 5.5.3 Scene and Backend Identifier Changes

1. SceneId and BackendId renames **MUST** be treated as breaking changes unless an alias mechanism is provided.
2. If aliasing is implemented, aliases **MUST** be recorded in metadata and **MUST** not change the canonical identifier emitted in results.

## Acceptance Criteria

1. A manifest can be authored that lists scenes with IDs, IR paths, hashes, IR version, defaults, and required features, and the harness can validate it deterministically.
2. The IR design includes a header, explicit endianness, sectioning, and referencing rules sufficient to implement a parser and a replay-prepared representation.
3. The data model defines all entities necessary to produce reproducible results, including hashing and versioning rules.
4. Versioning rules are explicit for IR and result schemas, including compatibility and rejection behavior.
5. A stable repository/install layout is defined for scenes and schemas, and binary releases include necessary assets.

## Dependencies

1. Chapter 1 — Introduction and Scope (in-scope features, deliverables).
2. Chapter 2 — Requirements (IR semantic contract, reporting requirements, versioning expectations).
3. Chapter 4 — System Architecture Overview (IR Runtime and manifest roles).

---

