#!/usr/bin/env python3
"""Determinism check for AotR60 logic traces (Telemetry = 2 writes aotr60_trace_<date>_<time>.txt).

A trace line is: logic_frame sub seed crc mode. '# reset' lines separate games (menu map, a match, a replay, a
loaded save). On the Living World strategic map each LW logic tick adds a comment line
'# LW lw_frame seed_in seed_out view_hash objects_hash a_renders_since_last_lw_tick mode'.

    compare_traces.py                    newest trace file: find a game played with 60 FPS and a game that starts
                                         from the same logic RNG seed at 30 FPS in the same session - the replay
                                         of a skirmish, or the same save loaded again - and compare them
    compare_traces.py TRACE              same, for the given file
    compare_traces.py TRACE_A TRACE_B    compare the longest game of each file

PASS means: every logic tick both games reached has the same logic RNG state and the same engine sync CRC
(GameLogic::getCRC), and every Living World tick both reached has the same seeds, LW view state and object
transforms, i.e. 60 FPS mode did not change the game's logic.
"""
import glob
import os
import sys

DATA = os.path.join(os.environ.get('APPDATA', ''), 'Age of the Ring', 'aotr60')


class Game:
    def __init__(self):
        self.logic = []  # (frame, sub, seed, crc, mode)
        self.lw = []     # (lw_frame, seed_in, seed_out, view_hash, objects_hash, a_renders, mode)


def segments(path):
    games, cur = [], Game()
    with open(path, encoding='ascii', errors='replace') as f:
        for line in f:
            if line.startswith('# reset'):
                if cur.logic or cur.lw:
                    games.append(cur)
                cur = Game()
                continue
            if line.startswith('# LW '):
                lw_frame, seed_in, seed_out, view, objs, renders, mode = line.split()[2:]
                cur.lw.append((int(lw_frame), seed_in, seed_out, view, objs, int(renders), int(mode)))
                continue
            if line.startswith('#') or not line.strip():
                continue
            frame, sub, seed, crc, mode = line.split()
            cur.logic.append((int(frame), int(sub), seed, crc, int(mode)))
    if cur.logic or cur.lw:
        games.append(cur)
    return [g for g in games if len(g.logic) >= 30]


def share60(g):
    return sum(1 for r in g.logic if r[4] == 60) / len(g.logic)


def describe(i, g):
    text = (f'game {i}: {len(g.logic)} logic calls, frames {g.logic[0][0]}..{g.logic[-1][0]}, '
            f'{100.0 * share60(g):.0f}% at 60 FPS, first seed {first_seed(g)}')
    if g.lw:
        text += f', {len(g.lw)} Living World ticks'
    return text


def first_seed(g):
    for r in g.logic:
        if r[1] == 1:
            return r[2]
    return g.logic[0][2]


def compare_logic(a, b):
    ka = {(r[0], r[1]): r for r in a.logic}
    kb = {(r[0], r[1]): r for r in b.logic}
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


def compare_lw(a, b):
    if not a.lw and not b.lw:
        return 0
    for name, g in (('first', a), ('second', b)):
        odd = [r for r in g.lw[1:] if r[5] != 6]
        if odd:
            print(f'WARN: {name} game: {len(odd)} Living World ticks not 6 A-renders apart '
                  f'(first at LW frame {odd[0][0]}: {odd[0][5]})')
    ka = {r[0]: r for r in a.lw}
    kb = {r[0]: r for r in b.lw}
    common = sorted(key for key in kb if key in ka)
    if not common:
        print('FAIL: the two games share no Living World ticks')
        return 1
    fields = ('seed in', 'seed out', 'view state', 'object transforms')
    bad = []
    for key in common:
        ra, rb = ka[key], kb[key]
        diff = [fields[i] for i in range(4) if ra[1 + i] != rb[1 + i]]
        if diff:
            bad.append((key, diff))
    print(f'compared {len(common)} Living World ticks (LW frames {common[0]}..{common[-1]})')
    if bad:
        key, diff = bad[0]
        print(f'FAIL: {len(bad)} Living World mismatches; first at LW frame {key}: {", ".join(diff)}')
        return 1
    print('PASS: identical Living World seeds, view state and object transforms on every common LW tick')
    return 0


def compare(a, b):
    result = compare_logic(a, b)
    return compare_lw(a, b) or result


def main(argv):
    if len(argv) > 2:
        a = max(segments(argv[1]), key=lambda g: len(g.logic), default=None)
        b = max(segments(argv[2]), key=lambda g: len(g.logic), default=None)
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
    games = segments(path)
    for i, g in enumerate(games):
        print('  ' + describe(i, g))
    for i, g in enumerate(games):
        if share60(g) < 0.5:
            continue
        for j, h in enumerate(games):
            if j != i and share60(h) < 0.5 and first_seed(h) == first_seed(g):
                print(f'pair: game {i} (60 FPS) vs game {j} (30 FPS, same starting seed)')
                return compare(g, h)
    print('FAIL: no game played at 60 FPS with a 30 FPS game from the same starting seed in this trace')
    return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
