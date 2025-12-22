# VGCPU-Benchmark

## Overview
VGCPU-Benchmark is a comprehensive, cross-platform benchmarking suite designed to evaluate the **CPU-only** rendering performance of modern 2D vector graphics libraries. It provides a reproducible, fair, and statistically rigorous method to compare libraries like Skia, Blend2D, ThorVG, and Cairo on identical workloads.

## Key Features
*   **CPU-Only Focus**: Explicitly enforces software rasterization to measure raw algorithmic efficiency on x86_64 and ARM64.
*   **Canonical IR**: Uses a custom **Vector Graphics Intermediate Representation (VGIR)** to ensure all backends render the exact same path data and commands.
*   **Cross-Platform**: Supports Linux, Windows, and macOS.
*   **Reproducible**: Pinned dependency versions and a containerized build environment ensure consistent results.

## Supported Backends
The suite includes adapters for the following libraries (see `blueprint/Appendix A` for versions):
*   **Skia** (Google)
*   **Blend2D**
*   **ThorVG** (Samsung)
*   **Cairo**
*   **Vello** (CPU fallback)
*   **PlutoVG**
*   **Raqote**
*   **AmanithVG SRE**
*   **Qt QPainter**
*   **Anti-Grain Geometry (AGG)**

## Project Structure
*   `blueprint/`: Detailed technical specifications and architecture documentation.
    *   `Chapter 5`: Data Design (IR Binary Specs)
    *   `Chapter 6`: C++ Interfaces
    *   `Appendix A`: Tools & Dependencies
*   `src/`: Source code (CLI, Harness, Adapters, PAL).
*   `assets/`: Scene manifests and IR files.

## Documentation
For detailed architectural documents, please refer to the [blueprint](./blueprint/) directory.

## License
MIT License (see [LICENSE](./LICENSE)).

Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
