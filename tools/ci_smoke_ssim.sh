#!/bin/bash
# Smoke test for artifacts and SSIM regression flow
set -e

# Default binary location (adapt as needed)
BIN="./build/dev/vgcpu-benchmark"
OUT_DIR="smoke_out"
GOLDEN_DIR="smoke_golden"
SCENE="test/simple_rect"  # Use a valid built-in scene

if [ ! -f "$BIN" ]; then
    echo "Error: Binary not found at $BIN"
    exit 1
fi

echo "=== VGCPU-Benchmark Smoke Test (SSIM) ==="
echo "Binary: $BIN"
echo "Scene: $SCENE"

# Cleanup
rm -rf "$OUT_DIR" "$GOLDEN_DIR"

# ---------------------------------------------------------
# Step 1: Generate Golden Image
# ---------------------------------------------------------
echo ""
echo "[Step 1] Generating golden image..."
mkdir -p "$GOLDEN_DIR"
# We use PlutoVG if available (Tier-1), else Null. 
# Null might produce empty image depending on impl, but PlutoVG is safer for real content.
# We assume PlutoVG is built (default).

BACKEND="plutovg"
$BIN run --backend $BACKEND --scene $SCENE --png --output-dir "$GOLDEN_DIR"

# Artifact naming: test/simple_rect -> test_simple_rect
SCENE_SANITIZED="${SCENE//\//_}"
ARTIFACT="$GOLDEN_DIR/${BACKEND}_${SCENE_SANITIZED}.png"
if [ ! -f "$ARTIFACT" ]; then
    echo "Error: Golden artifact not created: $ARTIFACT"
    exit 1
fi
echo "Golden artifact created: $ARTIFACT"

# ---------------------------------------------------------
# Step 2: Run Regression Test
# ---------------------------------------------------------
echo ""
echo "[Step 2] Running regression test (comparing against itself)..."
mkdir -p "$OUT_DIR"

# Enable JSON report to verify SSIM status programmaticallly
$BIN run --backend $BACKEND --scene $SCENE \
    --compare-ssim --golden-dir "$GOLDEN_DIR" \
    --png --output-dir "$OUT_DIR" \
    --format json --out "$OUT_DIR"

# ---------------------------------------------------------
# Step 3: Verify Results
# ---------------------------------------------------------
echo ""
echo "[Step 3] Verifying results..."

REPORT="$OUT_DIR/results.json"
if [ ! -f "$REPORT" ]; then
    echo "Error: Report not generated: $REPORT"
    exit 1
fi

# Simple check for passed: true in ssim block
if grep -q '"passed": true' "$REPORT"; then
    echo "SUCCESS: SSIM validation passed."
else
    echo "FAILURE: SSIM validation failed or not found in report."
    cat "$REPORT"
    exit 1
fi

echo ""
echo "Smoke test passed successfully."
exit 0
