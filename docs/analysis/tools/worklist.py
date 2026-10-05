import json, re, collections, csv
# 1) direct refs to timing globals, grouped by function, excluding static-init stubs (handled separately)
rows=list(csv.DictReader(open('an/timing_refs.tsv',encoding='utf-8'),delimiter='\t'))
init_addrs=set(int(o['addr'],16) for o in json.load(open('an/timing_initializers.json')))
def in_init(r):
    a=int(r['ref_addr'],16)
    return 0xbc0000<=a<0xbd0000
fn=collections.OrderedDict()
for r in rows:
    if in_init(r): continue
    fn.setdefault(r['func_entry'],{'name':r['func_name'],'refs':[]})['refs'].append((r['ref_addr'],r['global'],r['instruction']))
# 2) uses of init-computed globals (non-init code)
tu=json.load(open('an/init_target_uses.json'))
for g,info in tu.items():
    for (addr,fe,fname,ins) in info['uses']:
        a=int(addr,16)
        if 0xbc0000<=a<0xbd0000: continue
        fn.setdefault(fe,{'name':fname,'refs':[]})['refs'].append((addr,f"INIT_{info['shape']}@{g}",ins))
print('functions to classify:',len(fn), 'refs:',sum(len(v['refs']) for v in fn.values()))
items=[{'entry':k,'name':v['name'],'refs':v['refs']} for k,v in sorted(fn.items(), key=lambda kv:int(kv[0],16) if kv[0]!='?' else 0)]
json.dump(items, open('an/classify_items.json','w'), indent=0)
c=collections.Counter(g for it in items for (_,g,_) in it['refs']); print(c.most_common())
