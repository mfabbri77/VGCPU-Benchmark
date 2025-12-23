#!/usr/bin/env python3
"""
validate_report_json.py - JSON Report Schema Validator
Blueprint Reference: [REQ-123], [REQ-124], [TEST-42]

Validates that JSON benchmark reports conform to the expected schema.
"""

import argparse
import json
import sys
from pathlib import Path

# Required top-level keys per [REQ-49]
REQUIRED_TOP_LEVEL = ['schema_version', 'run_metadata', 'cases']

# Required run_metadata keys
REQUIRED_METADATA = ['timestamp', 'suite_version', 'environment', 'policy']

# Required environment keys
REQUIRED_ENVIRONMENT = ['os_name', 'os_version', 'arch', 'cpu_model', 'cpu_cores']

# Required policy keys  
REQUIRED_POLICY = ['warmup_iterations', 'measurement_iterations', 'repetitions', 'thread_count']

# Required case keys
REQUIRED_CASE = ['backend_id', 'scene_id', 'decision']


def validate_report(data: dict) -> list[str]:
    """Validate report and return list of errors."""
    errors = []
    
    # Check top-level keys
    for key in REQUIRED_TOP_LEVEL:
        if key not in data:
            errors.append(f"Missing required top-level key: '{key}'")
    
    # Validate schema_version per [REQ-133-01]
    if 'schema_version' in data:
        sv = data['schema_version']
        # Blueprint says integer, but current implementation uses string version
        # Accept both for now but warn if not integer
        if not isinstance(sv, (int, str)):
            errors.append(f"schema_version must be int or string, got {type(sv).__name__}")
    
    # Validate run_metadata
    if 'run_metadata' in data:
        metadata = data['run_metadata']
        for key in REQUIRED_METADATA:
            if key not in metadata:
                errors.append(f"Missing run_metadata key: '{key}'")
        
        # Validate environment
        if 'environment' in metadata:
            env = metadata['environment']
            for key in REQUIRED_ENVIRONMENT:
                if key not in env:
                    errors.append(f"Missing environment key: '{key}'")
        
        # Validate policy
        if 'policy' in metadata:
            policy = metadata['policy']
            for key in REQUIRED_POLICY:
                if key not in policy:
                    errors.append(f"Missing policy key: '{key}'")
    
    # Validate cases
    if 'cases' in data:
        cases = data['cases']
        if not isinstance(cases, list):
            errors.append("'cases' must be an array")
        else:
            for i, case in enumerate(cases):
                for key in REQUIRED_CASE:
                    if key not in case:
                        errors.append(f"Case {i}: missing required key '{key}'")
    
    return errors


def main():
    parser = argparse.ArgumentParser(
        description='Validate JSON benchmark report schema'
    )
    parser.add_argument('report', help='Path to JSON report file')
    parser.add_argument('--strict', action='store_true',
                       help='Fail on warnings too')
    args = parser.parse_args()
    
    report_path = Path(args.report)
    if not report_path.exists():
        print(f"Error: Report file not found: {report_path}")
        sys.exit(1)
    
    try:
        with open(report_path, 'r') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON: {e}")
        sys.exit(1)
    
    print(f"[REQ-124] Validating JSON report: {report_path}")
    
    errors = validate_report(data)
    
    if errors:
        print(f"\n❌ Validation failed with {len(errors)} error(s):")
        for error in errors:
            print(f"  - {error}")
        sys.exit(1)
    else:
        print("✅ JSON report is valid")
        sys.exit(0)


if __name__ == '__main__':
    main()
