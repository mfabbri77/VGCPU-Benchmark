#!/usr/bin/env python3
"""
check_no_temp_dbg.py - TEMP-DBG Marker Scanner
Blueprint Reference: [REQ-104], [REQ-105], [REQ-121], [REQ-122]

Scans source files for [TEMP-DBG] markers and exits with error if found.
TEMP-DBG markers must use the format:
    // [TEMP-DBG] START <reason> <owner> <date>
    ...temporary debug code...
    // [TEMP-DBG] END
"""

import argparse
import re
import sys
from pathlib import Path

# Pattern to match TEMP-DBG markers
TEMP_DBG_PATTERN = re.compile(r'\[TEMP-DBG\]', re.IGNORECASE)

# File extensions to scan
SOURCE_EXTENSIONS = {'.cpp', '.hpp', '.h', '.c', '.cxx', '.hxx'}

def scan_file(filepath: Path) -> list[tuple[int, str]]:
    """Scan a single file for TEMP-DBG markers."""
    matches = []
    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            for line_num, line in enumerate(f, 1):
                if TEMP_DBG_PATTERN.search(line):
                    matches.append((line_num, line.rstrip()))
    except Exception as e:
        print(f"Warning: Could not read {filepath}: {e}", file=sys.stderr)
    return matches

def find_source_files(root: Path, excludes: list[str]) -> list[Path]:
    """Find all source files, excluding specified directories."""
    files = []
    exclude_paths = [Path(e) for e in excludes]
    
    for ext in SOURCE_EXTENSIONS:
        for filepath in root.rglob(f'*{ext}'):
            # Check if file is in an excluded directory
            excluded = False
            for exclude in exclude_paths:
                try:
                    filepath.relative_to(root / exclude)
                    excluded = True
                    break
                except ValueError:
                    pass
            if not excluded:
                files.append(filepath)
    return files

def main():
    parser = argparse.ArgumentParser(
        description='Scan for [TEMP-DBG] markers in source files'
    )
    parser.add_argument(
        'root',
        nargs='?',
        default='.',
        help='Root directory to scan (default: current directory)'
    )
    parser.add_argument(
        '--exclude',
        action='append',
        default=['build', '_deps', 'third_party', '.git'],
        help='Directories to exclude (can be specified multiple times)'
    )
    parser.add_argument(
        '--quiet',
        action='store_true',
        help='Only output errors'
    )
    args = parser.parse_args()

    root = Path(args.root).resolve()
    
    if not args.quiet:
        print(f"[REQ-105] Scanning for TEMP-DBG markers in {root}")
    
    files = find_source_files(root, args.exclude)
    
    if not args.quiet:
        print(f"Found {len(files)} source files to scan")
    
    total_markers = 0
    files_with_markers = []
    
    for filepath in sorted(files):
        matches = scan_file(filepath)
        if matches:
            files_with_markers.append((filepath, matches))
            total_markers += len(matches)
    
    if files_with_markers:
        print(f"\n❌ TEMP-DBG markers found in {len(files_with_markers)} file(s):\n")
        for filepath, matches in files_with_markers:
            rel_path = filepath.relative_to(root)
            print(f"  {rel_path}:")
            for line_num, line in matches:
                print(f"    L{line_num}: {line[:80]}...")
        print(f"\n[REQ-121] Total markers: {total_markers}")
        print("[REQ-122] Build MUST fail when TEMP-DBG markers are present.")
        sys.exit(1)
    else:
        if not args.quiet:
            print("\n✅ No TEMP-DBG markers found")
        sys.exit(0)

if __name__ == '__main__':
    main()
