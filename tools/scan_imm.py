import pickle, sqlite3, re, bisect, sys
from pathlib import Path
H=Path(__file__).resolve().parents[1]
lin=pickle.load(open(H/'lin.pkl','rb'))
db=sqlite3.connect(H/'index.db')
funcs=sorted(db.execute('select address,end_ea,name from funcs').fetchall())
starts=[f[0] for f in funcs]
def fn(a):
    i=bisect.bisect_right(starts,a)-1
    if i>=0 and funcs[i][0]<=a<funcs[i][1]: return '%x'%funcs[i][0]
    return '?%x'%(funcs[i][0] if i>=0 else 0)
vals={640,480,639,479,320,240,307200,1280,0x27f00-0x280*0+0, 638,478,641,481,160,120}
vals={640,480,639,479,320,240,307200,1280}
pat=re.compile(r'(?<![\w\[+\-*])(0x[0-9a-f]+|\d+)\b')
out=[]
for a,s,m,o in lin:
    if a<0x410000: continue
    hit=None
    for tok in re.findall(r'0x[0-9a-f]+|\b\d+\b',o):
        v=int(tok,16) if tok.startswith('0x') else int(tok)
        if v in vals: hit=v;break
    if hit is None:
        if m in('shl','sal') and o.endswith(', 7'): hit='shl7'
        elif m=='imul' and ('0x280' in o): hit='imul'
        elif m=='lea' and re.search(r'\*4\]|\*4 ',o) and False: pass
    if hit is not None:
        out.append((a,fn(a),m,o,hit))
for r in out: print('%x %s %-6s %-40s %s'%r)
print(len(out),file=sys.stderr)
