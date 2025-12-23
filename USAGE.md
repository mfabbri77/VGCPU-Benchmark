# Usage Guide

The **VGCPU-Benchmark** CLI allows you to inspect assets, validate integrity, and run benchmark cases across various 2D vector graphics backends.

## Basic Commands

### Help
Print available commands and flags:
```bash
./vgcpu-benchmark --help
```

### List Assets
List all registered backends and scenes:
```bash
./vgcpu-benchmark list
```

### Validate Assets
Check integrity of scenes against the manifest:
```bash
./vgcpu-benchmark validate
```

---

## Running Benchmarks

The `run` command executes benchmark cases. You can filter by backend or scene.

### Simple Run
Run a specific scene on the PlutoVG backend:
```bash
./vgcpu-benchmark run --backend plutovg --scene-id rect_100x100
```

### Run All
Run all scenes on all backends:
```bash
./vgcpu-benchmark run
```
*Note: This may take a while depending on the number of scenes and backends.*

### Configuration Flags

*   `--backend <id>`: Filter by backend ID (comma-separated for multiple, e.g., `plutovg,cairo`).
*   `--scene-id <id>`: Filter by scene ID.
*   `--warmup <N>`: Number of warmup iterations (default: 5).
*   `--measure <N>`: Number of measurement iterations (default: 10).
*   `--repeat <N>`: Number of repetitions for statistical stability (default: 3).

### Output Formats

Save results to files for analysis:

**JSON Output:**
```bash
./vgcpu-benchmark run --json results.json
```

**CSV Output:**
```bash
./vgcpu-benchmark run --csv results.csv
```

---

## Interpreting Results

*   **Wall Time**: Total elapsed time for the rendering call. Influenced by system load.
*   **CPU Time**: Time the CPU spent executing the thread. More stable metric for isolated benchmarks.
*   **p50 (Median)**: The middle value. Good for typical performance.
*   **p90**: 90th percentile. Indicates tail latency.

### JSON Schema
The JSON output contains:
*   `run_metadata`: Timestamp, environment info (OS, CPU), and policy settings.
*   `cases`: Array of results, each containing `backend_id`, `scene_id`, `status`, and `stats` (timings).
