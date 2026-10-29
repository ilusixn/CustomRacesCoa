#!/usr/bin/env python3
"""Somente leitura. Valida as arvores de uma classe num CharacterAdvancement.dbc.
Uso: valida_classe.py <dbc> --classtype N --tabs a,b,c"""
import struct, sys, collections
P=sys.argv[1]; arg=lambda n: sys.argv[sys.argv.index(n)+1]
CT=int(arg("--classtype")); TABS=[int(x) for x in arg("--tabs").split(",")]
b=open(P,"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20]); recs=[b[20+i*rs:20+(i+1)*rs] for i in range(n)]; strs=b[20+n*rs:]
u=lambda r,i: struct.unpack_from("<I",r,i*4)[0]; F=lambda r,o: struct.unpack_from("<f",r,o)[0]; FL=lambda r: struct.unpack_from("<I",r,479)[0]
byid={u(r,0):r for r in recs}
links=lambda r:[struct.unpack_from("<I",r,415+4*k)[0] for k in range(15) if struct.unpack_from("<I",r,415+4*k)[0]]
ids=[u(r,0) for r in recs]; print("ids duplicados no DBC:", len(ids)-len(set(ids)))
for tab in TABS:
    rows=[r for r in recs if u(r,32)==CT and u(r,33)==tab and not FL(r)&0x8]
    col=[r for r in rows if F(r,398)==10.0 or (FL(r)&0x100000 and u(r,2))]; tree=[r for r in rows if F(r,398)!=10.0 and not (FL(r)&0x100000 and u(r,2))]
    pos={u(r,0):(F(r,398),F(r,402)) for r in tree}
    bypos=collections.defaultdict(list)
    for r in tree: bypos[pos[u(r,0)]].append(u(r,0))
    par={i:[p for p in links(byid[i])] for i in pos}
    bad=[(i,p) for i,ps in par.items() for p in ps if p not in byid or u(byid[p],32)!=CT or u(byid[p],33)!=tab or FL(byid[p])&0x8]
    up=[(i,p) for i,ps in par.items() for p in ps if p in pos and pos[p][1]>=pos[i][1]]
    req=[(u(r,0),u(r,k)) for r in tree for k in (2,3,4) if u(r,k) and u(r,k) not in byid]
    ch=collections.Counter(p for ps in par.values() for p in ps if p in pos)
    roots=[i for i in pos if not par[i]]
    seen=set(); st=list(roots)
    kids=collections.defaultdict(list)
    for i,ps in par.items():
        for p in ps: kids[p].append(i)
    while st:
        x=st.pop()
        if x in seen: continue
        seen.add(x); st+=kids[x]
    toprow=min((p[1] for p in pos.values()), default=0)
    iso=[i for i in pos if not par[i] and pos[i][1]>toprow+1 and not FL(byid[i])&0x100000]  # base automatica pode ficar sem pai
    pairs=[p for p,v in bypos.items() if len(v)>1]
    print(f"== aba {tab}: talentos {len(tree)} (+ automaticas {len(col)}) | posicoes {len(bypos)} | ligacoes {sum(len(v) for v in par.values())} | "
          f"bifurcacoes (no com 2+ filhos) {sum(1 for c in ch.values() if c>=2)} | juncoes (2+ pais) {sum(1 for v in par.values() if len(v)>=2)} | "
          f"nos de escolha {len(pairs)} {[(int(x),int(y)) for x,y in pairs]} | alcancaveis {len(seen)}/{len(pos)} | isolados {len(iso)} | "
          f"ligacao invalida {len(bad)} | ligacao para cima/lado {len(up)} | REQ pendurado {len(req)}")
