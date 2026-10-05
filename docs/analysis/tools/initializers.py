import pefile, struct, capstone, collections, re
pe=pefile.PE('ghw/bin/game.dat', fast_load=True)
img=pe.get_memory_mapped_image(); B=0x400000
def u32(a): return struct.unpack('<I', img[a-B:a-B+4])[0]
G={0xd9f608:'LTR5',0xd9f60c:'FPS30',0xd9f610:'f0.005',0xd9f614:'f200',0xd9f618:'f5',0xd9f61c:'f0.2',0xd9f620:'f33.3',0xd9f624:'f0.03',0xd9f628:'f30',0xd9f62c:'f0.0333',0xd9f498:'SPEED'}
md=capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail=False
ptrs=[u32(a) for a in range(0xd89000,0xd8af08,4)]
ptrs=[p for p in ptrs if p]
out=[]; stats=collections.Counter()
for idx,p in enumerate(ptrs):
    ins_list=[]
    for ins in md.disasm(img[p-B:p-B+400], p):
        ins_list.append(ins)
        if ins.mnemonic=='ret' or len(ins_list)>80: break
    text=[f"{i.address:#x}: {i.mnemonic} {i.op_str}" for i in ins_list]
    used=set()
    for t in text:
        for a,n in G.items():
            if hex(a) in t: used.add(n)
    if used:
        stores=[re.search(r'\[(0x[0-9a-f]+)\]',t).group(1) for t in text if re.search(r'(mov|fstp|fistp|movss) (dword|qword) ptr \[0x[0-9a-f]+\], ',t)]
        stores=[s for s in stores if int(s,16) not in G]
        calls=[t.split('call ')[1] for t in text if ' call ' in ' '+t.split(': ',1)[1]]
        out.append((idx,p,sorted(used),stores,calls,text))
        for u in used: stats[u]+=1
print('initializers using timing globals:',len(out), dict(stats))
# group by instruction "shape" (strip addresses) to find families
fam=collections.Counter()
shape={}
for o in out:
    sh=' | '.join(re.sub(r'0x[0-9a-f]{5,}','A',t.split(': ',1)[1]) for t in o[5])
    fam[sh]+=1; shape.setdefault(sh,o)
print('distinct shapes:',len(fam))
for sh,c in fam.most_common(40):
    o=shape[sh]; print(f"\n[{c}x] e.g. init#{o[0]} @ {o[1]:#x} uses {o[2]} stores {o[3][:3]} calls {o[4][:2]}\n   "+sh[:400])
import json
json.dump([dict(idx=o[0],addr=hex(o[1]),uses=o[2],stores=o[3],calls=o[4],asm=o[5]) for o in out], open('an/timing_initializers.json','w'), indent=1)
