#!/usr/bin/env python3
"""Lookup helper over the Ghidra dump of AotR game.dat.
usage (run from the folder that holds an/ and ghw/, see README.md):
  python an/ghq.py fn <addr>           decompiled C of the function containing/at addr
  python an/ghq.py asm <addr>          disassembly of the function at/containing addr (Ghidra listing)
  python an/ghq.py callers <addr>      functions calling the function at addr
  python an/ghq.py callees <addr>      functions called by the function at addr
  python an/ghq.py refs <addr>         every instruction referencing absolute address addr (data or code)
  python an/ghq.py str <regex>         strings matching regex, with referencing functions
  python an/ghq.py name <addr>         function name/size/signature
"""
import sys, re, os, json, bisect
D=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','ghw','out','game.dat')
def h(a): return int(a,16) if isinstance(a,str) else a
def funcs():
    out=[]
    with open(os.path.join(D,'functions.tsv'),encoding='utf-8') as f:
        next(f)
        for line in f:
            p=line.rstrip('\n').split('\t')
            out.append((int(p[0],16),p[1],int(p[2]),p[3],p[4],p[6] if len(p)>6 else ''))
    out.sort(); return out
def containing(a):
    fs=funcs(); keys=[x[0] for x in fs]; i=bisect.bisect_right(keys,a)-1
    return fs[i] if i>=0 else None
def index_decomp():
    idx_path=os.path.join(D,'decompiled.idx.json')
    src=os.path.join(D,'decompiled.c')
    if os.path.exists(idx_path) and os.path.getmtime(idx_path)>=os.path.getmtime(src):
        return json.load(open(idx_path))
    idx={}; off=0
    pat=re.compile(rb'^// ===== .* @ ([0-9a-f]+) =====')
    with open(src,'rb') as f:
        for line in f:
            m=pat.match(line)
            if m: idx[m.group(1).decode()]=off
            off+=len(line)
    json.dump(idx,open(idx_path,'w')); return idx
def cmd_fn(a):
    a=h(a); idx=index_decomp(); key=f"{a:08x}"
    if key not in idx:
        c=containing(a); key=f"{c[0]:08x}" if c else None
    if not key or key not in idx: print('not found'); return
    with open(os.path.join(D,'decompiled.c'),'rb') as f:
        f.seek(idx[key]); first=True
        for line in f:
            if not first and line.startswith(b'// ===== '): break
            first=False; sys.stdout.write(line.decode('utf-8','replace'))
def cmd_asm(a):
    a=h(a); c=containing(a); start=f"{c[0]:08x}"
    hdr=f"; ===== {c[1]} @ {start} ====="
    on=False
    with open(os.path.join(D,'listing.asm'),encoding='utf-8') as f:
        for line in f:
            if line.startswith('; ====='):
                if on: break
                on = line.strip()==hdr
                if on: print(line.rstrip())
                continue
            if on and line.strip(): print(line.rstrip())
def cg(col_match, col_out, a):
    a=h(a); key=f"{a:08x}"; seen=set()
    with open(os.path.join(D,'callgraph.tsv'),encoding='utf-8') as f:
        next(f)
        for line in f:
            p=line.rstrip('\n').split('\t')
            if p[col_match]==key:
                o=(p[col_out],p[col_out+1])
                if o not in seen: seen.add(o); print(o[0],o[1])
def cmd_refs(a):
    a=h(a); pat=re.compile(r'0x0*%x\b'%a, re.I); cur='?'
    hdr=re.compile(r'^; ===== (.*) @ ([0-9a-f]+) =====')
    with open(os.path.join(D,'listing.asm'),encoding='utf-8') as f:
        for line in f:
            m=hdr.match(line)
            if m: cur=f"{m.group(1)}@{m.group(2)}"; continue
            if pat.search(line): print(cur,'|',line.rstrip())
def cmd_str(rx):
    r=re.compile(rx, re.I)
    with open(os.path.join(D,'strings.tsv'),encoding='utf-8') as f:
        for line in f:
            p=line.rstrip('\n').split('\t')
            if len(p)>1 and r.search(p[1]): print(line.rstrip())
def cmd_name(a):
    c=containing(h(a)); print(f"{c[0]:08x}\t{c[1]}\tsize={c[2]}\tcallers={c[3]}\tcallees={c[4]}\t{c[5]}")
if __name__=='__main__':
    c=sys.argv[1]; arg=sys.argv[2]
    {'fn':cmd_fn,'asm':cmd_asm,'callers':lambda a:cg(2,0,a),'callees':lambda a:cg(0,2,a),'refs':cmd_refs,'str':cmd_str,'name':cmd_name}[c](arg)
