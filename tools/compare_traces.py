#!/usr/bin/env python3
"""Compares two AotR60 logic traces (Telemetry=2, aotr60_trace.txt).

Each line: logic_frame sub seed crc mode. Records are matched by (logic_frame, sub) over the frames both traces
cover; a determinism run passes when every common record has the same logic RNG seed and, for sub 1, the same
engine sync CRC (GameLogic::getCRC).

Usage: uv run python tools/compare_traces.py A.aotr60_trace.txt B.aotr60_trace.txt
"""
import sys


def load(path):
    records = {}
    order = []
    modes = {}
    with open(path, encoding='ascii', errors='replace') as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            frame, sub, seed, crc, mode = line.split()
            key = (int(frame), int(sub))
            if key in records:
                # A new game in the same session restarts the frame numbers; keep the first game only.
                break
            records[key] = (seed, crc)
            order.append(key)
            modes[mode] = modes.get(mode, 0) + 1
    return records, order, modes


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    a, order_a, modes_a = load(sys.argv[1])
    b, order_b, modes_b = load(sys.argv[2])
    common = [k for k in order_a if k in b]
    print(f'A: {len(a)} records, modes {modes_a}, frames {order_a[0][0] if order_a else "-"}..{order_a[-1][0] if order_a else "-"}')
    print(f'B: {len(b)} records, modes {modes_b}, frames {order_b[0][0] if order_b else "-"}..{order_b[-1][0] if order_b else "-"}')
    if not common:
        print('FAIL: no common records')
        return 1
    seed_diff = [k for k in common if a[k][0] != b[k][0]]
    crc_diff = [k for k in common if k[1] == 1 and a[k][1] != b[k][1]]
    ticks = sum(1 for k in common if k[1] == 1)
    print(f'common: {len(common)} logic calls, {ticks} logic ticks (frames {common[0][0]}..{common[-1][0]})')
    print(f'seed mismatches: {len(seed_diff)}; CRC mismatches: {len(crc_diff)}')
    if seed_diff or crc_diff:
        first = min(seed_diff + crc_diff)
        print(f'FIRST DIVERGENCE at logic frame {first[0]} sub {first[1]}: A {a[first]} vs B {b[first]}')
        return 1
    print('PASS: identical logic RNG state and sync CRC on every common logic call')
    return 0


if __name__ == '__main__':
    sys.exit(main())
