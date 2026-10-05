#!/usr/bin/env python3
"""Summarise the frame-timing stall lines AotR60 writes to aotr60.log at Telemetry >= 1.

    analyze_stalls.py [aotr60.log]

A stall line describes one main-loop iteration that ran far over its slot (16.5 ms at 60 FPS, 33 ms at 30 FPS):
'stall 60 B: 41.3 ms = check 0.0 + render 6.1 (Present 0.2, spacing wait 0.0) + logic 31.0 (sub 1, frame 2345) +
pacer wait 0.0 + other 4.2; owed 3.1'. The summary shows which part of the iteration is responsible, how long the
stalls are, and how regularly they repeat (in logic frames and in seconds).
"""
import os
import re
import statistics
import sys
from collections import Counter

DATA = os.path.join(os.environ.get('APPDATA', ''), 'Age of the Ring', 'aotr60')
LINE = re.compile(r'^(\d\d):(\d\d):(\d\d)\.(\d\d\d) stall (\d+) (\S+): ([\d.]+) ms = check ([\d.]+) \+ render ([\d.]+) '
                  r'\(Present ([\d.]+), spacing wait ([\d.]+)\) \+ logic ([\d.]+) \(sub (\d+), frame (\d+)[^)]*\) \+ '
                  r'pacer wait ([\d.]+) \+ other (-?[\d.]+); owed ([\d.]+)')
PARTS = ('check', 'render', 'logic', 'other')


def main(argv):
    path = argv[1] if len(argv) > 1 else os.path.join(DATA, 'aotr60.log')
    stalls = []
    for line in open(path, encoding='ascii', errors='replace'):
        m = LINE.match(line)
        if not m:
            continue
        h, mi, s, ms = (int(x) for x in m.group(1, 2, 3, 4))
        stalls.append({
            't': h * 3600 + mi * 60 + s + ms / 1000.0, 'mode': int(m.group(5)), 'kind': m.group(6),
            'total': float(m.group(7)), 'check': float(m.group(8)), 'render': float(m.group(9)),
            'present': float(m.group(10)), 'logic': float(m.group(12)), 'sub': int(m.group(13)),
            'frame': int(m.group(14)), 'other': float(m.group(16)),
        })
    if not stalls:
        print(f'no stall lines in {path} (Telemetry must be 1 or 2)')
        return 1
    print(f'{path}: {len(stalls)} stall lines')
    for mode in (60, 30):
        group = [x for x in stalls if x['mode'] == mode]
        if not group:
            continue
        print(f'\n{mode} FPS: {len(group)} stalls, length median {statistics.median(x["total"] for x in group):.1f} ms, '
              f'max {max(x["total"] for x in group):.1f} ms')
        cause = Counter(max(PARTS, key=lambda p: x[p]) for x in group)
        print('  largest part: ' + ', '.join(f'{p} {n}' for p, n in cause.most_common()))
        subs = Counter(x['sub'] for x in group if max(PARTS, key=lambda p: x[p]) == 'logic')
        if subs:
            print('  logic-dominated stalls by sub: ' + ', '.join(f'sub {s}: {n}' for s, n in sorted(subs.items())))
        kinds = Counter(x['kind'] for x in group)
        print('  iteration kind: ' + ', '.join(f'{k} {n}' for k, n in kinds.most_common()))
        for part in PARTS:
            values = sorted(x[part] for x in group)
            print(f'  {part:7s} median {statistics.median(values):6.1f} ms   p90 {values[int(0.9 * (len(values) - 1))]:6.1f} '
                  f'ms   max {values[-1]:6.1f} ms')
        dt = [b['t'] - a['t'] for a, b in zip(group, group[1:]) if 0 < b['t'] - a['t'] < 30]
        df = [b['frame'] - a['frame'] for a, b in zip(group, group[1:]) if 0 < b['frame'] - a['frame'] < 1000]
        if dt:
            print(f'  spacing between stalls: median {statistics.median(dt):.2f} s; most common logic-frame gaps: ' +
                  ', '.join(f'{g} ({n}x)' for g, n in Counter(df).most_common(6)))
        big = [x for x in group if x['total'] > 50]
        if big:
            print(f'  {len(big)} stalls over 50 ms; first: ' + ', '.join(
                f'{x["total"]:.0f} ms ({max(PARTS, key=lambda p: x[p])}, frame {x["frame"]})' for x in big[:8]))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
