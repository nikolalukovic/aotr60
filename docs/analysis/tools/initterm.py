import pefile, struct, capstone, collections
pe=pefile.PE('ghw/bin/game.dat', fast_load=True)
img=pe.get_memory_mapped_image(); B=0x400000
def u32(a): return struct.unpack('<I', img[a-B:a-B+4])[0]
# find runs of >=50 consecutive pointers (or 0) into .text inside .rdata/.data
best=[]
for sec in pe.sections:
    nm=sec.Name.rstrip(b'\0').decode(errors='replace')
    if nm not in ('.rdata','.data'): continue
    s=B+sec.VirtualAddress; e=s+sec.Misc_VirtualSize
    a=s; run_start=None; n=0
    while a<e-4:
        v=u32(a)
        ok = (0x401000<=v<0xbd0000) or v==0
        if ok:
            if run_start is None: run_start=a; n=0
            if v: n+=1
        else:
            if run_start is not None and n>=50:
                ptrs=[u32(x) for x in range(run_start,a,4)]
                inbc=sum(1 for p in ptrs if 0xbc0000<=p<0xbd0000)
                best.append((run_start,a,n,inbc))
            run_start=None
        a+=4
for r in sorted(best,key=lambda r:-r[3])[:8]: print(f"table {r[0]:#x}-{r[1]:#x} ptrs={r[2]} in_bc={r[3]}")
