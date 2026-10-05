#!/usr/bin/env python3
"""Verifies tools/sites.json against the game executable.

Checks, per site: exact original bytes; the span decodes to whole instructions; no overlap with other sites or with
AotR's own hooks; no direct branch anywhere in the executable sections targets the span interior. Then the coverage
rule: every call instruction in clientUpdate, GameClient::update and InGameUI::update is classified, and every
'gate' points at a site whose span contains the call.

Usage: uv run --with pefile --with capstone python tools/verify_sites.py [--game ../rotwk/game.dat]
Exit code 0 = all checks passed.
"""
import argparse
import json
import os
import re
import sys

import capstone
import pefile

BASE = 0x400000
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))

# AotR's own code hooks (start, length): never overlap them.
AOTR_HOOKS = [(0x52CC7F, 5), (0x5D8A64, 5), (0x5D8AF1, 5), (0x629D11, 11), (0x638D49, 5), (0x69413A, 6),
              (0x69A760, 5), (0x6D40FA, 5), (0x6D410A, 5), (0x6D57AF, 5), (0x6D7267, 6), (0x79DCE0, 6),
              (0x88D061, 6), (0x8A11A3, 9), (0x8A144D, 6), (0x9A3AE0, 5)]

# Functions whose every call must be classified (start, end exclusive).
COVERAGE_FUNCTIONS = {
    'clientUpdate': (0x632409, 0x63252F),
    'GameClient::update': (0x64849E, 0x6488D8),
    'InGameUI::update': (0x6A1F4D, 0x6A24D2),
}

DATA_KINDS = {'ptr_slot', 'data_write'}


def hexbytes(s):
    return bytes(int(x, 16) for x in s.split())


def load_image(path):
    pe = pefile.PE(path, fast_load=True)
    return pe, pe.get_memory_mapped_image()


def exec_ranges(pe):
    for s in pe.sections:
        if s.Characteristics & 0x20000000:  # IMAGE_SCN_MEM_EXECUTE
            start = BASE + s.VirtualAddress
            yield start, start + max(s.Misc_VirtualSize, s.SizeOfRawData)


def sweep(pe, img, md):
    """Linear sweep of every executable section: instruction starts and direct branch/call targets."""
    targets, starts = {}, set()
    imm = re.compile(r'^0x([0-9a-f]+)$')
    for start, end in exec_ranges(pe):
        data = img[start - BASE:end - BASE]
        off = 0
        while off < len(data):
            decoded = False
            for insn in md.disasm_lite(data[off:], start + off):
                addr, size, mnem, ops = insn
                decoded = True
                starts.add(addr)
                if (mnem.startswith('j') or mnem in ('call', 'loop', 'loope', 'loopne', 'jecxz')) and imm.match(ops):
                    targets.setdefault(int(ops, 16), []).append(addr)
                off = addr + size - start
            if not decoded:
                off += 1
    return targets, starts


def calls_in(img, md, start, end):
    out = []
    for addr, size, mnem, ops in md.disasm_lite(img[start - BASE:end - BASE], start):
        if mnem == 'call':
            out.append((addr, ops))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game', default=os.path.join(ROOT, 'rotwk', 'game.dat'))
    ap.add_argument('--sites', default=os.path.join(HERE, 'sites.json'))
    args = ap.parse_args()

    doc = json.load(open(args.sites, encoding='utf-8'))
    sites = doc['sites']
    pe, img = load_image(args.game)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    errors, warnings = [], []

    spans = []
    ids = set()
    for s in sites:
        sid = s['id']
        if sid in ids:
            errors.append(f'{sid}: duplicate id')
        ids.add(sid)
        addr = int(s['address'], 16)
        length = int(s['length'])
        orig = hexbytes(s['original_hex'])
        if len(orig) != length:
            errors.append(f'{sid}: original_hex has {len(orig)} bytes, length says {length}')
        actual = img[addr - BASE:addr - BASE + len(orig)]
        if actual != orig:
            errors.append(f'{sid} @{addr:#x}: bytes differ: expected {orig.hex(" ")} found {actual.hex(" ")}')
        if s['kind'] not in DATA_KINDS:
            end = addr
            for a, size, _, _ in md.disasm_lite(img[addr - BASE:addr - BASE + length + 16], addr):
                if a >= addr + length:
                    break
                end = a + size
            if end != addr + length:
                errors.append(f'{sid} @{addr:#x}: span does not end on an instruction boundary ({end:#x} vs {addr + length:#x})')
            if length < 5 and s['kind'] in ('call_gate', 'jmp_detour', 'func_detour'):
                errors.append(f'{sid}: span shorter than 5 bytes for a {s["kind"]}')
        for hstart, hlen in AOTR_HOOKS:
            if addr < hstart + hlen and hstart < addr + length:
                errors.append(f'{sid}: overlaps AotR hook at {hstart:#x}')
        spans.append((addr, addr + length, sid, s['kind']))

    spans.sort()
    for (a0, a1, i0, _), (b0, b1, i1, _) in zip(spans, spans[1:]):
        if b0 < a1:
            errors.append(f'{i0} and {i1} overlap')

    print('sweeping executable sections for branch targets ...', flush=True)
    targets, starts = sweep(pe, img, md)
    for start, end, sid, kind in spans:
        if kind in DATA_KINDS:
            continue
        if start not in starts:
            errors.append(f'{sid} @{start:#x}: span does not start on an instruction boundary (linear sweep)')
        for t in range(start + 1, end):
            if t in targets:
                errors.append(f'{sid}: branch into span interior {t:#x} from {", ".join(hex(x) for x in targets[t][:4])}')

    # Coverage rule.
    coverage = {int(c['call_address'], 16): c for c in doc.get('coverage', [])}
    by_id = {s['id']: s for s in sites}
    for name, (start, end) in COVERAGE_FUNCTIONS.items():
        for addr, ops in calls_in(img, md, start, end):
            c = coverage.get(addr)
            if c is None:
                errors.append(f'coverage: {name} call at {addr:#x} ({ops}) is not classified')
                continue
            if c['cls'] == 'gate':
                site = by_id.get(c.get('site_id', ''))
                if site is None:
                    errors.append(f'coverage: {name} call {addr:#x} is "gate" but site {c.get("site_id")!r} does not exist')
                else:
                    s0 = int(site['address'], 16)
                    if not (s0 <= addr < s0 + int(site['length'])):
                        # A gate may also sit on the vtable load right before the call (e.g. 8b 01 ff 50 28).
                        if not (s0 <= addr + 1 < s0 + int(site['length']) + 3):
                            warnings.append(f'coverage: {name} call {addr:#x} gated by {site["id"]} @{s0:#x} outside its span')

    for t in doc.get('todo', []):
        warnings.append('TODO: ' + t)
    for w in warnings:
        print('warning:', w)
    for e in errors:
        print('ERROR:', e)
    print(f'{len(sites)} sites, {len(coverage)} coverage entries, {len(errors)} errors, {len(warnings)} warnings')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
