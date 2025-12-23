#!/usr/bin/env python3
"""
validate_report_csv.py - CSV Report Schema Validator
Blueprint Reference: [REQ-125], [TEST-43]

Validates that CSV benchmark reports conform to the expected schema.
"""

import argparse
import csv
import sys
from pathlib import Path

# Required CSV columns per report format
REQUIRED_COLUMNS = [
    'backend_id',
    'scene_id', 
    'decision',
    'wall_p50_ns',
    'wall_p90_ns',
    'cpu_p50_ns',
    'cpu_p90_ns',
    'sample_count'
]


def validate_csv(filepath: Path) -> list[str]:
    """Validate CSV and return list of errors."""
    errors = []
    
    try:
        with open(filepath, 'r', newline='') as f:
            # Check for schema version comment (optional per current impl)
            first_line = f.readline()
            if first_line.startswith('#'):
                # Schema version comment found
                pass
            else:
                # No comment, reset to beginning
                f.seek(0)
            
            reader = csv.DictReader(f)
            headers = reader.fieldnames
            
            if not headers:
                errors.append("CSV file has no headers")
                return errors
            
            # Check required columns
            for col in REQUIRED_COLUMNS:
                if col not in headers:
                    errors.append(f"Missing required column: '{col}'")
            
            # Validate row data
            row_count = 0
            for i, row in enumerate(reader, 1):
                row_count += 1
                
                # Check backend_id is not empty
                if not row.get('backend_id', '').strip():
                    errors.append(f"Row {i}: empty backend_id")
                
                # Check scene_id is not empty
                if not row.get('scene_id', '').strip():
                    errors.append(f"Row {i}: empty scene_id")
                
                # Check numeric fields are valid
                for col in ['wall_p50_ns', 'wall_p90_ns', 'cpu_p50_ns', 'cpu_p90_ns', 'sample_count']:
                    val = row.get(col, '')
                    if val and not val.replace('-', '').isdigit():
                        errors.append(f"Row {i}: '{col}' should be numeric, got '{val}'")
            
            if row_count == 0:
                errors.append("CSV file has no data rows")
    
    except Exception as e:
        errors.append(f"Error reading CSV: {e}")
    
    return errors


def main():
    parser = argparse.ArgumentParser(
        description='Validate CSV benchmark report schema'
    )
    parser.add_argument('report', help='Path to CSV report file')
    args = parser.parse_args()
    
    report_path = Path(args.report)
    if not report_path.exists():
        print(f"Error: Report file not found: {report_path}")
        sys.exit(1)
    
    print(f"[REQ-125] Validating CSV report: {report_path}")
    
    errors = validate_csv(report_path)
    
    if errors:
        print(f"\n❌ Validation failed with {len(errors)} error(s):")
        for error in errors:
            print(f"  - {error}")
        sys.exit(1)
    else:
        print("✅ CSV report is valid")
        sys.exit(0)


if __name__ == '__main__':
    main()
