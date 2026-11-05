#!/usr/bin/env python3
"""Summarize coarse per-SH2 flush timing from MARS_RENDER_PROFILE builds.

These are shares of measured flush segments, not whole-CPU utilization.
Do not mix resets into a measurement interval.
"""
import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run')
    parser.add_argument('--warmup', type=int, default=480)
    args = parser.parse_args()
    run = json.loads(Path(args.run).read_text())
    if not 0 <= args.warmup < run['frames'] - 1:
        parser.error('warmup must precede the last video frame')
    samples = run['samples']
    first = next(s for s in samples if s['frame'] == args.warmup)
    last = samples[-1]
    a, b = first.get('render_profile'), last.get('render_profile')
    if not a or not b or len(a) != 8 or len(b) != 8:
        parser.error('run must use a MARS_RENDER_PROFILE ROM and matching ELF')
    if any(y < x for x, y in zip(a, b)):
        parser.error('counter reset or wrap inside the measurement interval')
    delta = [y - x for x, y in zip(a, b)]
    result = {}
    for cpu, offset in [('master', 0), ('slave', 4)]:
        work, wait, buckets, flushes = delta[offset:offset+4]
        result[cpu] = dict(work_ticks=work, wait_ticks=wait,
                           buckets=buckets, flushes=flushes,
                           wait_percent=100 * wait / (work + wait) if work + wait else None)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
