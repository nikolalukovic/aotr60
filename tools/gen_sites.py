#!/usr/bin/env python3
"""Generates src/sites.gen.h from tools/sites.json (the single source of every patch).

Each site becomes a constexpr record: address, original bytes and a replacement template whose <rel32:NAME> and
<abs32:NAME> placeholders the DLL resolves against its own stubs/variables at install time.

Usage: uv run python tools/gen_sites.py
"""
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'sites.json')
OUT = os.path.join(HERE, '..', 'src', 'sites.gen.h')
OUT_INC = os.path.join(HERE, '..', 'src', 'stubs', 'sites.gen.inc')

KINDS = {'call_gate': 'CallGate', 'jmp_detour': 'JmpDetour', 'func_detour': 'FuncDetour',
         'operand_redirect': 'OperandRedirect', 'ptr_slot': 'PtrSlot', 'data_write': 'DataWrite'}
PHASES = {'telemetry': 'Telemetry', '1': 'Phase1', '2a': 'Phase2a', '2b': 'Phase2b', '3': 'Phase3', 'sp': 'PhaseSp',
          '4b': 'Phase4b', '6': 'Phase6'}


def template_tokens(text):
    """'e8 <rel32:STUB> 90' -> [('rel32','STUB'), ('byte',0x90)]"""
    out = []
    for m in re.finditer(r'<(rel32|abs32):([A-Za-z0-9_]+)>|([0-9a-fA-F]{2})', text):
        if m.group(1):
            out.append((m.group(1), m.group(2)))
        else:
            out.append(('byte', int(m.group(3), 16)))
    return out


def template_length(tokens):
    return sum(4 if kind != 'byte' else 1 for kind, _ in tokens)


def main():
    doc = json.load(open(SRC, encoding='utf-8'))
    lines = [
        '// Generated from tools/sites.json by tools/gen_sites.py - do not edit.',
        '#pragma once',
        '#include <cstdint>',
        '',
        'namespace sites {',
        '',
        'enum class Kind : uint8_t { CallGate, JmpDetour, FuncDetour, OperandRedirect, PtrSlot, DataWrite };',
        'enum class Phase : uint8_t { Telemetry, Phase1, Phase2a, Phase2b, Phase3, Phase4b, Phase6, PhaseSp };',
        'enum class TokenType : uint8_t { Byte, Rel32, Abs32 };',
        '',
        'struct Token {',
        '    TokenType type;',
        '    uint8_t value;      // TokenType::Byte',
        '    const char* symbol; // Rel32 / Abs32',
        '};',
        '',
        'struct Site {',
        '    const char* id;',
        '    Phase phase;',
        '    Kind kind;',
        '    uint32_t address;',
        '    uint32_t length;',
        '    const uint8_t* original;',
        '    const Token* replacement;',
        '    uint32_t tokenCount;',
        '};',
        '',
    ]
    index_names = []
    records = []
    for s in doc['sites']:
        sid = s['id']
        orig = [int(x, 16) for x in s['original_hex'].split()]
        tokens = template_tokens(s['replacement_hex'])
        if template_length(tokens) != len(orig):
            raise SystemExit(f'{sid}: replacement template is {template_length(tokens)} bytes, original {len(orig)}')
        cname = re.sub(r'[^A-Za-z0-9_]', '_', sid)
        lines.append(f'inline constexpr uint8_t k{cname}_original[] = {{{", ".join(f"0x{b:02X}" for b in orig)}}};')
        toks = []
        for kind, val in tokens:
            if kind == 'byte':
                toks.append(f'{{TokenType::Byte, 0x{val:02X}, nullptr}}')
            else:
                toks.append(f'{{TokenType::{"Rel32" if kind == "rel32" else "Abs32"}, 0, "{val}"}}')
        lines.append(f'inline constexpr Token k{cname}_replacement[] = {{{", ".join(toks)}}};')
        records.append(f'    {{"{sid}", Phase::{PHASES[s["phase"]]}, Kind::{KINDS[s["kind"]]}, {s["address"]}, '
                       f'{len(orig)}, k{cname}_original, k{cname}_replacement, {len(tokens)}}},')
        index_names.append(cname)
    lines += ['', 'inline constexpr Site kSites[] = {'] + records + ['};', '']
    lines.append(f'inline constexpr uint32_t kSiteCount = {len(records)};')
    lines.append('')
    lines.append('// Index of each site in kSites (telemetry counters are indexed the same way).')
    lines.append('enum Index : uint32_t {')
    lines += [f'    {name} = {i},' for i, name in enumerate(index_names)]
    lines += ['};', '', '} // namespace sites', '']
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines))
    print(f'wrote {os.path.normpath(OUT)} ({len(records)} sites)')

    inc = ['; Generated from tools/sites.json by tools/gen_sites.py - do not edit.',
           '; Site indices for the per-site telemetry counters (g_siteRun / g_siteSkip).']
    inc += [f'IDX_{name} EQU {i}' for i, name in enumerate(index_names)]
    inc.append(f'SITE_COUNT EQU {len(index_names)}')
    with open(OUT_INC, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(inc) + '\n')
    print(f'wrote {os.path.normpath(OUT_INC)}')


if __name__ == '__main__':
    main()
