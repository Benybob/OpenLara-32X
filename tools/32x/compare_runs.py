#!/usr/bin/env python3
"""Compare guest page-flip rates from two headless libretro runs."""
import argparse
import json
from pathlib import Path

def main():
    p = argparse.ArgumentParser()
    p.add_argument('baseline')
    p.add_argument('improved')
    p.add_argument('--warmup', type=int, default=480)
    a = p.parse_args()
    runs = [json.loads(Path(n).read_text()) for n in (a.baseline, a.improved)]
    for field in ('region', 'emulated_hz', 'frames', 'core_sha256', 'interpreter', 'overrides'):
        if runs[0][field] != runs[1][field]:
            raise SystemExit('Incompatible runs: ' + field)
    if all('input_events' in r for r in runs) and runs[0]['input_events'] != runs[1]['input_events']:
        raise SystemExit('Incompatible controller replays')
    rates = []
    for r in runs:
        if not 0 <= a.warmup < r['frames']: raise SystemExit('Invalid warmup')
        flips = sum(a.warmup <= n < r['frames'] for n in r['page_flips'])
        rates.append(flips * r['emulated_hz'] / (r['frames'] - a.warmup))
    print(json.dumps({'baseline_fps': rates[0], 'improved_fps': rates[1],
                      'change_percent': (rates[1] / rates[0] - 1) * 100}, indent=2))

if __name__ == '__main__': main()
