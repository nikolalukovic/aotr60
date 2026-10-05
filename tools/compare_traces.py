#!/usr/bin/env python3
"""Determinism check for AotR60 logic traces (Telemetry = 2 writes aotr60_trace_<date>_<time>.txt).

A trace line is: logic_frame sub seed crc mode. '# reset' lines separate games (menu map, a match, a replay).

    compare_traces.py                    newest trace file: compare the game played with 60 FPS against the
                                         replay of it watched later in the same session (paired by the logic
                                         RNG seed at the first logic tick, which a replay reproduces)
    compare_traces.py TRACE              same, for the given file
    compare_traces.py TRACE_A TRACE_B    compare the longest game of each file

PASS means: every logic tick both games reached has the same logic RNG state and the same engine sync CRC
(GameLogic::getCRC), i.e. 60 FPS mode did not change the game's logic.
"""
import glob
import os
import sys

DATA = os.path.join(os.environ.get('APPDATA', ''), 'Age of the Ring', 'aotr60')


def segments(path):
    segs, cur = [], []
    with open(path, encoding='ascii', errors='replace') as f:
        for line in f:
            if line.startswith('# reset'):
                if cur:
                    segs.append(cur)
                cur = []
                continue
            if line.startswith('#') or not line.strip():
                continue
            frame, sub, seed, crc, mode = line.split()
            cur.append((int(frame), int(sub), seed, crc, int(mode)))
    if cur:
        segs.append(cur)
    return [s for s in segs if len(s) >= 30]


def describe(i, s):
    m60 = sum(1 for r in s if r[4] == 60)
    return (f'game {i}: {len(s)} logic calls, frames {s[0][0]}..{s[-1][0]}, '
            f'{100.0 * m60 / len(s):.0f}% at 60 FPS, first seed {first_seed(s)}')


def first_seed(s):
    for r in s:
        if r[1] == 1:
            return r[2]
    return s[0][2]


def compare(a, b):
    ka = {(r[0], r[1]): r for r in a}
    kb = {(r[0], r[1]): r for r in b}
    common = [key for key in kb if key in ka]
    if not common:
        print('FAIL: the two games share no logic frames')
        return 1
    bad = []
    for key in common:
        ra, rb = ka[key], kb[key]
        if ra[2] != rb[2] or (key[1] == 1 and ra[3] != rb[3]):
            bad.append((key, ra, rb))
    ticks = sum(1 for k in common if k[1] == 1)
    print(f'compared {len(common)} logic calls ({ticks} logic ticks, frames {min(common)[0]}..{max(common)[0]})')
    if bad:
        key, ra, rb = bad[0]
        print(f'FAIL: {len(bad)} mismatches; first at logic frame {key[0]} sub {key[1]}: '
              f'seed {ra[2]} vs {rb[2]}, crc {ra[3]} vs {rb[3]}')
        return 1
    print('PASS: identical logic RNG state and sync CRC on every common logic tick')
    return 0


def main(argv):
    if len(argv) > 2:
        a = max(segments(argv[1]), key=len, default=None)
        b = max(segments(argv[2]), key=len, default=None)
        if not a or not b:
            print('FAIL: no game found in one of the traces')
            return 1
        return compare(a, b)
    path = argv[1] if len(argv) == 2 else max(glob.glob(os.path.join(DATA, 'aotr60_trace_*.txt')),
                                                 key=os.path.getmtime, default=None)
    if not path:
        print(f'no trace found in {DATA} (set Telemetry = 2 in aotr60.ini and play)')
        return 1
    print(f'trace: {path}')
    segs = segments(path)
    for i, s in enumerate(segs):
        print('  ' + describe(i, s))
    played = [i for i, s in enumerate(segs) if any(r[4] == 60 for r in s)]
    for i in played:
        for j in range(i + 1, len(segs)):
            if first_seed(segs[j]) == first_seed(segs[i]):
                print(f'pair: game {i} (played at 60 FPS) vs game {j} (its replay)')
                return compare(segs[i], segs[j])
    print('FAIL: no game played at 60 FPS followed by its replay in this trace')
    return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
