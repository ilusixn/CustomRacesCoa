#!/usr/bin/env python3
"""Relata (e com --apply grava) ligacoes mais claras nas arvores de uma classe: so troca ligacoes longas/cruzadas
por ligacoes com um no proximo acima; posicoes, ids, talentos e tiers nao mudam.
Uso: ajusta_ligacoes.py <dbc> --classtype <id ClassTypes> --tabs <aba,aba,...> [--apply]
Ex.: Paladino --classtype 44 --tabs 7,14,25 ; Warrior --classtype 37 --tabs 87,6,5,7"""
import struct, sys, itertools
P=sys.argv[1]; APPLY="--apply" in sys.argv
def arg(name):
    i=sys.argv.index(name); return sys.argv[i+1]
try:
    CLASSTYPE=int(arg("--classtype")); TABS=[int(x) for x in arg("--tabs").split(",")]
except (ValueError, IndexError):
    raise SystemExit("faltam --classtype e --tabs (veja o cabecalho do script)")
b=bytearray(open(P,"rb").read()); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20]); recs=[b[20+i*rs:20+(i+1)*rs] for i in range(n)]; strs=b[20+n*rs:]
u=lambda r,i: struct.unpack_from("<I",r,i*4)[0]; F=lambda r,o: struct.unpack_from("<f",r,o)[0]; FL=lambda r: struct.unpack_from("<I",r,479)[0]
def S(r,i):
    o=u(r,i); return strs[o:strs.find(b"\x00",o)].decode("utf8","replace")
def links(r): return [struct.unpack_from("<I",r,415+4*k)[0] for k in range(15) if struct.unpack_from("<I",r,415+4*k)[0]]
def cross(a,b,c,d):
    # segmentos a-b e c-d se cruzam no interior (ignora extremidades em comum)
    if len({a,b,c,d})<4: return False
    def o(p,q,r): return (q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0])
    return (o(a,b,c)*o(a,b,d)<0) and (o(c,d,a)*o(c,d,b)<0)
changed_total=0
for tab in TABS:
    name=f"aba {tab}"
    rows=[r for r in recs if u(r,32)==CLASSTYPE and u(r,33)==tab and not FL(r)&0x8 and F(r,398)!=10.0 and not (FL(r)&0x100000 and u(r,2))]  # fora: coluna e automaticas-dependencia (com REQ); base fica  # automaticas (coluna e dependencias) ficam fora da teia
    pos={u(r,0):(F(r,398),F(r,402)) for r in rows}; byid={u(r,0):r for r in rows}; nm={i:S(byid[i],47) for i in pos}
    par={i:[p for p in links(byid[i]) if p in pos] for i in pos}
    roots=[i for i in pos if not par[i]]
    # agrupa ids na mesma posicao (no de escolha) -> trata a posicao como unidade
    bypos={}
    for i,p in pos.items(): bypos.setdefault(p,[]).append(i)
    ppar={p:set(pos[q] for i in ids for q in par[i]) for p,ids in bypos.items()}
    def short(p,q): return (p[1]-q[1])<=2 and abs(p[0]-q[0])<=2 and q[1]<p[1]
    new={}; notes=[]
    for p in sorted(bypos, key=lambda p:(p[1],p[0])):
        if not ppar[p]: new[p]=set(); continue
        keep={q for q in ppar[p] if short(p,q)}
        dropped=ppar[p]-keep
        cands=sorted([q for q in bypos if q[1]<p[1] and q!=p], key=lambda q:(p[1]-q[1],abs(p[0]-q[0]),q[0]))
        want=max(1,min(len(ppar[p]),2))
        for q in cands:
            if len(keep)>=want: break
            if q not in keep and short(p,q): keep.add(q)
        if not keep: keep={cands[0]}
        if keep!=ppar[p]: notes.append(f"  {nm[bypos[p][0]][:22]} {tuple(map(int,p))}: pais {sorted(tuple(map(int,q)) for q in ppar[p])} -> {sorted(tuple(map(int,q)) for q in keep)}")
        new[p]=keep
    # cruzamentos restantes: remove a ligacao mais longa se o filho tiver outro pai
    segs=lambda: [(p,q) for p,qs in new.items() for q in qs]
    for _ in range(10):
        found=False
        for (a,b1),(c,d) in itertools.combinations(segs(),2):
            if cross(a,b1,c,d):
                l1=abs(a[0]-b1[0])+(a[1]-b1[1]); l2=abs(c[0]-d[0])+(c[1]-d[1])
                for (p,q) in sorted([(a,b1),(c,d)], key=lambda s:-(abs(s[0][0]-s[1][0])+(s[0][1]-s[1][1]))):
                    if len(new[p])>1: new[p].discard(q); notes.append(f"  cruzamento: tira {nm[bypos[p][0]][:22]} {tuple(map(int,p))} -> {tuple(map(int,q))}"); found=True; break
                if found: break
        if not found: break
    # validacao
    ch={p:[c for c,qs in new.items() if p in qs] for p in new}
    seen=set(); st=[p for p in new if not new[p]]
    while st:
        x=st.pop()
        if x in seen: continue
        seen.add(x); st+=ch[x]
    def depth(parmap):
        d={p:0 for p in parmap if not parmap[p]}; changed=True
        while changed:
            changed=False
            for p,qs in parmap.items():
                if qs and all(q in d for q in qs):
                    v=1+min(d[q] for q in qs)
                    if d.get(p)!=v: d[p]=v; changed=True
                elif qs and any(q in d for q in qs):
                    v=1+min(d[q] for q in qs if q in d)
                    if p not in d or d[p]>v: d[p]=v; changed=True
        return d
    db,da=depth(ppar),depth(new)
    earlier=[(tuple(map(int,p)),db.get(p),da.get(p)) for p in new if p in db and p in da and da[p]<db[p]]
    notes.append(f"  profundidade minima reduzida em {len(earlier)} no(s): {earlier}")
    crossings=sum(1 for (a,b1),(c,d) in itertools.combinations(segs(),2) if cross(a,b1,c,d))
    before=sum(1 for (a,b1),(c,d) in itertools.combinations([(p,q) for p,qs in ppar.items() for q in qs],2) if cross(a,b1,c,d))
    longb=sum(1 for p,qs in ppar.items() for q in qs if not short(p,q)); longa=sum(1 for p,qs in new.items() for q in qs if not short(p,q))
    bif_b=sum(1 for p in ppar if len(ppar[p])>=2); bif_a=sum(1 for p in new if len(new[p])>=2)
    print(f"== {name}: ligacoes {sum(map(len,ppar.values()))}->{sum(map(len,new.values()))}, longas {longb}->{longa}, cruzamentos {before}->{crossings}, nos com 2 pais {bif_b}->{bif_a}, alcancaveis {len(seen)}/{len(new)}, isolados {sum(1 for p in new if new[p]==set() and p not in [pos[r] for r in roots])}")
    for t in notes: print(t)
    changed_total+=len(notes)
    if APPLY:
        for p,ids in bypos.items():
            lk=[i for q in new[p] for i in bypos[q]][:15]
            for i in ids:
                r=byid[i]
                for k in range(15): struct.pack_into("<I",r,415+4*k, lk[k] if k<len(lk) else 0)
if APPLY:
    open(P,"wb").write(b"".join([b[:20]]+[bytes(r) for r in recs]+[bytes(strs)])); print("gravado:",P)
print("mudancas propostas:",changed_total)
