import json, re, collections
inits=json.load(open('an/timing_initializers.json'))
targets={}
for o in inits:
    for s in o['stores']:
        shape='half' if ('sar eax, 1' in ' '.join(o['asm']) and 'FPS30' in o['uses']) else ('msframe' if '0x3e8' in ' '.join(o['asm']) else 'ltr')
        targets[int(s,16)]=shape
hdr=re.compile(r'^; ===== (.*) @ ([0-9a-f]+) =====')
pat=re.compile(r'0x00([0-9a-f]{6})\b')
uses=collections.defaultdict(list); cur=('?','?')
with open('ghw/out/game.dat/listing.asm',encoding='utf-8') as f:
    for line in f:
        m=hdr.match(line)
        if m: cur=(m.group(2),m.group(1)); continue
        for m in pat.finditer(line):
            a=int(m.group(1),16)
            if a in targets: uses[a].append((line.split(':')[0],cur[0],cur[1],line.split(': ',1)[1].strip()))
c=collections.Counter(); used=collections.Counter()
for a,s in targets.items():
    c[s]+=1
    if uses[a]: used[s]+=1
print('targets by shape',dict(c),' with >=1 code use',dict(used))
tot=collections.Counter(s for a,s in targets.items() for _ in uses[a]); print('total uses by shape',dict(tot))
for a,s in targets.items():
    if s!='half':
        print(f"\n{s} {a:#x}: {len(uses[a])} uses")
        for u in uses[a][:12]: print('   ',u)
# sample half uses
hs=[(a,u) for a,s in targets.items() if s=='half' for u in uses[a]]
print('\nhalf uses sample:'); 
for a,u in hs[:25]: print(f"  {a:#x}",u)
json.dump({hex(a):{'shape':s,'uses':uses[a]} for a,s in targets.items()}, open('an/init_target_uses.json','w'), indent=1)
