#!/bin/bash
set -e

# Configuration
BUILD_DIR="build_release"
PACKAGE_DIR="package"
ARTIFACT_NAME="vgcpu-benchmark-release.tar.gz"

echo "=== VGCPU-Benchmark Packaging Script ==="

# 1. Clean
echo "[*] Cleaning..."
rm -rf $BUILD_DIR $PACKAGE_DIR $ARTIFACT_NAME

# 2. Configure
echo "[*] Configuring (Release)..."
cmake -B $BUILD_DIR -G Ninja -DCMAKE_BUILD_TYPE=Release

# 3. Build
echo "[*] Building..."
cmake --build $BUILD_DIR --config Release

# 4. Package
echo "[*] Packaging..."
mkdir -p $PACKAGE_DIR/bin

# Copy Executable (Handle Linux/Mac vs Windows ext if running bash on win)
if [ -f "$BUILD_DIR/vgcpu-benchmark" ]; then
    cp "$BUILD_DIR/vgcpu-benchmark" "$PACKAGE_DIR/bin/"
elif [ -f "$BUILD_DIR/vgcpu-benchmark.exe" ]; then
    cp "$BUILD_DIR/vgcpu-benchmark.exe" "$PACKAGE_DIR/bin/"
else
    echo "Error: Executable not found!"
    exit 1
fi

# Copy Assets
cp -r assets $PACKAGE_DIR/

# Copy Docs
cp README.md $PACKAGE_DIR/
if [ -f "BUILD.md" ]; then cp BUILD.md $PACKAGE_DIR/; fi
if [ -f "USAGE.md" ]; then cp USAGE.md $PACKAGE_DIR/; fi

# create archive
tar -czvf $ARTIFACT_NAME -C $PACKAGE_DIR .

echo "[SUCCESS] Artifact created: $ARTIFACT_NAME"
ls -lh $ARTIFACT_NAME
