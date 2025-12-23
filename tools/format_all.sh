#!/bin/bash
# format_all.sh - Format all source files
# Blueprint Reference: [REQ-103], [REQ-116]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Find all source files (exclude build directories and dependencies)
find_sources() {
    find "$ROOT_DIR/src" "$ROOT_DIR/tests" -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) 2>/dev/null || true
}

echo "[REQ-116] Formatting all source files..."

FILES=$(find_sources)
if [ -z "$FILES" ]; then
    echo "No source files found to format"
    exit 0
fi

COUNT=0
for file in $FILES; do
    clang-format -i "$file"
    COUNT=$((COUNT + 1))
done

echo "✅ Formatted $COUNT files"
