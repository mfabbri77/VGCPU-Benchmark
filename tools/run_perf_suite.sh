#!/bin/bash
# run_perf_suite.sh - Run the performance benchmark suite
# Blueprint Reference: [TASK-08.01], [REQ-63]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
PERF_DIR="${ROOT_DIR}/perf"
BENCHMARK="${1:-${ROOT_DIR}/build/release/vgcpu-benchmark}"
OUTPUT_DIR="${2:-${PERF_DIR}/results}"

if [ ! -x "$BENCHMARK" ]; then
    echo "Error: Benchmark executable not found: $BENCHMARK"
    echo "Usage: $0 [executable] [output_dir]"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

# Read scene list, filtering comments and empty lines
SCENES=$(grep -v '^#' "${PERF_DIR}/perf_suite_scenes.txt" | grep -v '^$' | paste -sd, -)

if [ -z "$SCENES" ]; then
    echo "Error: No scenes found in perf_suite_scenes.txt"
    exit 1
fi

echo "[REQ-63] Running performance suite..."
echo "  Executable: $BENCHMARK"
echo "  Scenes: $SCENES"
echo "  Output: $OUTPUT_DIR"
echo ""

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_FILE="${OUTPUT_DIR}/perf_${TIMESTAMP}"

"$BENCHMARK" run \
    --all-backends \
    --scene "$SCENES" \
    --warmup-iters 5 \
    --iters 20 \
    --repetitions 3 \
    --format json \
    --out "$OUTPUT_FILE"

echo ""
echo "✅ Performance suite completed"
echo "   Results: ${OUTPUT_FILE}.json"

# Create symlink to latest results
ln -sf "perf_${TIMESTAMP}.json" "${OUTPUT_DIR}/latest.json"
