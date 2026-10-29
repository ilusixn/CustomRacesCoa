#!/usr/bin/env python3
"""Compara dois CharacterAdvancement.dbc registro a registro (por id), resolvendo os textos (tipo, nome, descricao).
Uso: compara_ca.py <a> <b> [classtype ...]  -> lista diferencas; tambem ChrSpecs/Essence se passar --dir"""
import struct, sys
def load(p):
    b=open(p,"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20]); return [b[20+i*rs:20+(i+1)*rs] for i in range(n)], b[20+n*rs:]
u=lambda r,i: struct.unpack_from("<I",r,i*4)[0]
def norm(recs,strs,strf):
    out={}
    for r in recs:
        t=[strs[u(r,i):strs.find(b"\x00",u(r,i))] for i in strf]
        rr=bytearray(r)
        for i in strf: struct.pack_into("<I",rr,i*4,0)
        out[u(r,0)]=(bytes(rr),tuple(t))
    return out
a,b=sys.argv[1],sys.argv[2]; strf=[int(x) for x in sys.argv[3].split(",")]
A=norm(*load(a),strf); B=norm(*load(b),strf)
diff=[i for i in set(A)|set(B) if A.get(i)!=B.get(i)]
print(f"{a.split('/')[-1]}: registros {len(A)} x {len(B)}, diferentes {len(diff)}", sorted(diff)[:8])
