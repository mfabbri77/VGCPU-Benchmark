#!/bin/bash
# format_check.sh - Check code formatting
# Blueprint Reference: [REQ-103], [REQ-116]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Find all source files (exclude build directories and dependencies)
find_sources() {
    find "$ROOT_DIR/src" "$ROOT_DIR/tests" -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) 2>/dev/null || true
}

echo "[REQ-116] Checking code formatting..."

FILES=$(find_sources)
if [ -z "$FILES" ]; then
    echo "No source files found to check"
    exit 0
fi

FAILED=0
for file in $FILES; do
    if ! clang-format --dry-run --Werror "$file" 2>/dev/null; then
        echo "❌ Formatting error: $file"
        FAILED=1
    fi
done

if [ $FAILED -eq 0 ]; then
    echo "✅ All files are properly formatted"
    exit 0
else
    echo ""
    echo "Run 'clang-format -i <file>' to fix formatting issues"
    echo "Or run 'tools/format_all.sh' to format all files"
    exit 1
fi
