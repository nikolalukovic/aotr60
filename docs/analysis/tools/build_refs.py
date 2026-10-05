import re, collections, sys
O='ghw/out/game.dat/'
G={0xd9f608:'LTR_INT_5',0xd9f60c:'CLIENT_FPS_INT_30',0xd9f610:'F_0.005_per_ms_logic',0xd9f614:'F_200_ms_per_logic',0xd9f618:'F_5_logic_per_s',0xd9f61c:'F_0.2_s_per_logic',0xd9f620:'F_33.33_ms_per_client',0xd9f624:'F_0.03_client_per_ms',0xd9f628:'F_30_client_per_s',0xd9f62c:'F_0.0333_s_per_client',0xd9f498:'F_GAMESPEED_MULT'}
pat=re.compile(r'0x00(d9f6[0-2][0-9a-f]|d9f498)\b', re.I)
hdr=re.compile(r'^; ===== (.*) @ ([0-9a-f]+) =====')
cur=('?','?'); rows=[]
with open(O+'listing.asm',encoding='utf-8') as f:
    for line in f:
        m=hdr.match(line)
        if m: cur=(m.group(2),m.group(1)); continue
        m=pat.search(line)
        if m:
            a=int(m.group(1),16)
            if a in G:
                addr,ins=line.split(': ',1)
                rows.append((addr,cur[0],cur[1],G[a],ins.strip()))
with open('an/timing_refs.tsv','w',encoding='utf-8') as w:
    w.write('ref_addr\tfunc_entry\tfunc_name\tglobal\tinstruction\n')
    for r in rows: w.write('\t'.join(r)+'\n')
c=collections.Counter(r[3] for r in rows); print(c)
funcs=collections.OrderedDict()
for r in rows: funcs.setdefault(r[1],[]).append(r)
print('refs',len(rows),'functions',len(funcs))
# region split
reg=collections.Counter(('static_init_bcxxxx' if int(r[1],16)>=0xbc0000 and int(r[1],16)<0xbd0000 else 'text') for r in rows if r[1]!='?')
print(reg)
