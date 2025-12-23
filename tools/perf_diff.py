#!/usr/bin/env python3
"""
perf_diff.py - Performance Difference Tool
Blueprint Reference: [TASK-08.01], [REQ-63..65]

Compares two benchmark result files and reports performance differences.
Ignores nondeterministic metadata (timestamps, git commit, etc).
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List, Tuple


def load_results(filepath: Path) -> Dict[str, Dict]:
    """Load benchmark results from JSON file into a dict keyed by (backend_id, scene_id)."""
    with open(filepath, 'r') as f:
        data = json.load(f)
    
    results = {}
    for case in data.get('cases', []):
        key = f"{case['backend_id']}:{case['scene_id']}"
        results[key] = {
            'backend_id': case['backend_id'],
            'scene_id': case['scene_id'],
            'decision': case['decision'],
            'stats': case.get('stats', {})
        }
    return results


def compute_diff(baseline: Dict, current: Dict, threshold_pct: float) -> List[Tuple]:
    """
    Compare baseline and current results.
    Returns list of (key, pct_change, is_regression, baseline_val, current_val)
    """
    diffs = []
    
    all_keys = set(baseline.keys()) | set(current.keys())
    
    for key in sorted(all_keys):
        base = baseline.get(key)
        curr = current.get(key)
        
        if not base or not curr:
            # New or removed test
            continue
        
        if base['decision'] != 'EXECUTE' or curr['decision'] != 'EXECUTE':
            continue
        
        base_p50 = base['stats'].get('wall_p50_ns', 0)
        curr_p50 = curr['stats'].get('wall_p50_ns', 0)
        
        if base_p50 == 0:
            continue
        
        pct_change = ((curr_p50 - base_p50) / base_p50) * 100
        is_regression = pct_change > threshold_pct
        
        diffs.append((key, pct_change, is_regression, base_p50, curr_p50))
    
    return diffs


def ns_to_ms(ns: int) -> float:
    return ns / 1_000_000


def main():
    parser = argparse.ArgumentParser(
        description='Compare benchmark results for performance regressions'
    )
    parser.add_argument('baseline', help='Baseline results JSON file')
    parser.add_argument('current', help='Current results JSON file')
    parser.add_argument('--threshold', type=float, default=10.0,
                       help='Regression threshold percentage (default: 10%%)')
    parser.add_argument('--fail-on-regression', action='store_true',
                       help='Exit with error if regressions detected')
    parser.add_argument('--output', help='Output file for diff report (optional)')
    args = parser.parse_args()
    
    baseline_path = Path(args.baseline)
    current_path = Path(args.current)
    
    if not baseline_path.exists():
        print(f"Error: Baseline file not found: {baseline_path}")
        sys.exit(1)
    
    if not current_path.exists():
        print(f"Error: Current file not found: {current_path}")
        sys.exit(1)
    
    print(f"[REQ-65] Comparing performance results...")
    print(f"  Baseline: {baseline_path}")
    print(f"  Current:  {current_path}")
    print(f"  Threshold: {args.threshold}%")
    print()
    
    baseline = load_results(baseline_path)
    current = load_results(current_path)
    
    diffs = compute_diff(baseline, current, args.threshold)
    
    # Sort by percent change (worst regressions first)
    diffs.sort(key=lambda x: -x[1])
    
    regressions = [d for d in diffs if d[2]]
    improvements = [d for d in diffs if d[1] < -args.threshold]
    
    print(f"{'Backend:Scene':<40} {'Base (ms)':>10} {'Curr (ms)':>10} {'Change':>10}")
    print("-" * 72)
    
    for key, pct, is_regression, base_ns, curr_ns in diffs:
        indicator = "⚠️ " if is_regression else ("✨" if pct < -args.threshold else "  ")
        print(f"{indicator}{key:<38} {ns_to_ms(base_ns):>10.3f} {ns_to_ms(curr_ns):>10.3f} {pct:>+9.1f}%")
    
    print()
    print(f"Summary: {len(regressions)} regressions, {len(improvements)} improvements")
    
    # Output to file if requested
    if args.output:
        report = {
            'threshold': args.threshold,
            'regressions': len(regressions),
            'improvements': len(improvements),
            'diffs': [
                {'key': k, 'pct_change': p, 'is_regression': r, 'base_ns': b, 'curr_ns': c}
                for k, p, r, b, c in diffs
            ]
        }
        with open(args.output, 'w') as f:
            json.dump(report, f, indent=2)
        print(f"\nReport written to: {args.output}")
    
    if args.fail_on_regression and regressions:
        print(f"\n❌ {len(regressions)} performance regression(s) detected!")
        sys.exit(1)
    else:
        print("\n✅ No significant regressions detected")
        sys.exit(0)


if __name__ == '__main__':
    main()
