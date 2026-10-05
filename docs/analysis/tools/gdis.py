import pefile, capstone, sys, struct
pe=pefile.PE('ghw/bin/'+(sys.argv[3] if len(sys.argv)>3 else 'game.dat'), fast_load=True)
img=pe.get_memory_mapped_image()
md=capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
names={0xd9f608:'LTR5',0xd9f60c:'FPS30',0xd9f610:'f0.005',0xd9f614:'f200',0xd9f618:'f5',0xd9f61c:'f0.2',0xd9f620:'f33.3',0xd9f624:'f0.03',0xd9f628:'f30',0xd9f62c:'f0.0333',0xbd0920:'timeGetTime',0xbd03b4:'Sleep',0xbd02e8:'QPC',0xbd02ec:'QPF',0xbd02a0:'GetTickCount'}
s=int(sys.argv[1],16); e=int(sys.argv[2],16)
for ins in md.disasm(img[s-0x400000:e-0x400000], s):
    t=f"{ins.mnemonic} {ins.op_str}"
    for k,v in names.items():
        t=t.replace(hex(k), v)
    # annotate float constants
    import re
    m=re.search(r'\[(0x[0-9a-f]{6,7})\]', t)
    ann=''
    if m:
        a=int(m.group(1),16)
        if 0xbd0000<=a<0xe00000:
            raw=img[a-0x400000:a-0x400000+8]
            ann=f"   ; [{a:#x}]=int {struct.unpack('<i',raw[:4])[0]} f {struct.unpack('<f',raw[:4])[0]:.6g} d {struct.unpack('<d',raw)[0]:.6g}"
    print(f"{ins.address:#x}: {t}{ann}")
