#!/usr/bin/env python3
"""[SUPPORT-V2] linhas do livro (aba Support, pericia 11901, Paladino) para as 5 magias visiveis. Idempotente.
Uso: add_sla_support_v2.py <SkillLineAbility.dbc entrada> <saida>"""
import struct,sys
ROWS=[(2650001,2600006),(2650002,2600007),(2650003,2600008),(2650004,2600010),(2650005,2600011)]
d=open(sys.argv[1],"rb").read(); n,nf,rs,ss=struct.unpack("<4I",d[4:20]); assert (nf,rs)==(14,56)
have={struct.unpack_from("<I",d,20+r*rs)[0] for r in range(n)}
add=b"".join(struct.pack("<14I",i,11901,s,0,2,0,0,0,0,0,0,0,0,0) for i,s in ROWS if i not in have)
k=len(add)//rs
open(sys.argv[2],"wb").write(d[:4]+struct.pack("<4I",n+k,nf,rs,ss)+d[20:20+n*rs]+add+d[20+n*rs:])
print(sys.argv[2],": +%d linhas (total %d)"%(k,n+k))
