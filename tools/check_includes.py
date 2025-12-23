#!/usr/bin/env python3
"""
check_includes.py - Include Boundary Enforcement
Blueprint Reference: [TASK-03.03], [REQ-41]

Enforces include layering rules to prevent violations:
- adapters/ cannot include from cli/ or harness/
- ir/ cannot include from adapters/ or cli/
- pal/ cannot include from any other src/ module

Usage: python3 tools/check_includes.py <source_dir>
Exit code: 0 if no violations, 1 if violations found
"""

import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple

# Layer rules: module -> set of forbidden includes
LAYER_RULES: Dict[str, Set[str]] = {
    "pal": {"adapters", "cli", "harness", "ir", "assets", "reporting"},
    "common": {"adapters", "cli", "harness", "ir", "assets", "reporting", "pal"},
    "ir": {"adapters", "cli", "harness", "reporting"},
    "assets": {"cli", "harness", "adapters", "reporting"},
    "adapters": {"cli", "harness"},
    "harness": {"cli"},
    "reporting": {"cli"},
}

# Include pattern
INCLUDE_PATTERN = re.compile(r'#include\s*[<"]([^>"]+)[>"]')


def get_module(filepath: Path) -> str:
    """Extract module name from file path (e.g., src/pal/timer.cpp -> pal)."""
    parts = filepath.parts
    if "src" in parts:
        src_idx = parts.index("src")
        if src_idx + 1 < len(parts):
            return parts[src_idx + 1]
    return ""


def get_included_module(include_path: str) -> str:
    """Extract module name from include path (e.g., adapters/foo.h -> adapters)."""
    parts = Path(include_path).parts
    if parts:
        # Skip common prefixes
        if parts[0] in ("vgcpu", "src"):
            parts = parts[1:]
        if parts:
            return parts[0]
    return ""


def check_file(filepath: Path) -> List[Tuple[int, str, str, str]]:
    """
    Check a single file for include violations.
    Returns list of (line_num, include_path, current_module, forbidden_module)
    """
    violations = []
    current_module = get_module(filepath)
    
    if current_module not in LAYER_RULES:
        return violations
    
    forbidden = LAYER_RULES[current_module]
    
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        for line_num, line in enumerate(f, 1):
            match = INCLUDE_PATTERN.search(line)
            if match:
                include_path = match.group(1)
                included_module = get_included_module(include_path)
                
                if included_module in forbidden:
                    violations.append((line_num, include_path, current_module, included_module))
    
    return violations


def check_directory(source_dir: Path) -> Dict[Path, List[Tuple[int, str, str, str]]]:
    """Check all source files in directory for include violations."""
    all_violations: Dict[Path, List[Tuple[int, str, str, str]]] = {}
    
    for ext in ("*.cpp", "*.h", "*.hpp"):
        for filepath in source_dir.rglob(ext):
            # Skip build directories
            if "build" in filepath.parts or "_deps" in filepath.parts:
                continue
            
            violations = check_file(filepath)
            if violations:
                all_violations[filepath] = violations
    
    return all_violations


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <source_dir>")
        sys.exit(1)
    
    source_dir = Path(sys.argv[1])
    src_path = source_dir / "src"
    
    if not src_path.is_dir():
        src_path = source_dir  # Fallback if 'src' subdir doesn't exist
    
    print(f"[REQ-41] Checking include boundaries in: {src_path}")
    print(f"Layer rules:")
    for module, forbidden in sorted(LAYER_RULES.items()):
        print(f"  {module}/ cannot include from: {', '.join(sorted(forbidden))}")
    print()
    
    violations = check_directory(src_path)
    
    if not violations:
        print("✅ No include boundary violations found")
        sys.exit(0)
    
    print("❌ Include boundary violations found:\n")
    violation_count = 0
    
    for filepath, file_violations in sorted(violations.items()):
        rel_path = filepath.relative_to(source_dir) if source_dir in filepath.parents else filepath
        for line_num, include_path, current_mod, forbidden_mod in file_violations:
            print(f"  {rel_path}:{line_num}")
            print(f"    Module '{current_mod}' cannot include from '{forbidden_mod}'")
            print(f"    #include \"{include_path}\"")
            print()
            violation_count += 1
    
    print(f"Total violations: {violation_count}")
    sys.exit(1)


if __name__ == "__main__":
    main()
