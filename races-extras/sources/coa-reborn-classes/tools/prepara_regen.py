#!/usr/bin/env python3
import os as _os; COA_ROOT = _os.environ.get("COA_ROOT", ".")  # [release] server tree root
"""Monta a fonte para REGENERAR uma classe a partir do que esta instalado, sem perder as outras classes.
Uso: prepara_regen.py <classtype> <new_base> <classe_chrspecs> <essence_class> <dir_saida>
CA = instalado sem os ids da faixa da classe e com as linhas originais da classe restauradas (de dbc.antes-fase2);
ChrSpecs e Essence: linhas da classe restauradas do original. As strings originais continuam validas (o bloco de strings so cresce)."""
import struct, sys, os
CT,BASE,CLS,ESS,OUT=int(sys.argv[1]),int(sys.argv[2]),sys.argv[3],int(sys.argv[4]),sys.argv[5]
INS=COA_ROOT+"/env/dist/data/dbc"; ORI=COA_ROOT+"/env/dist/data/dbc.antes-fase2"
os.makedirs(OUT,exist_ok=True)
for f in os.listdir(INS):
    if f.endswith(".dbc") and not os.path.exists(f"{OUT}/{f}"): os.symlink(f"{INS}/{f}", f"{OUT}/{f}")
def load(p):
    b=open(p,"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20]); return dict(f=f,rs=rs,recs=[b[20+i*rs:20+(i+1)*rs] for i in range(n)],strs=b[20+n*rs:])
def save(p,d):
    if os.path.islink(p): os.remove(p)
    open(p,"wb").write(struct.pack("<4sIIII",b"WDBC",len(d["recs"]),d["f"],d["rs"],len(d["strs"]))+b"".join(d["recs"])+d["strs"])
u=lambda r,i: struct.unpack_from("<I",r,i*4)[0]
S=lambda d,r,i: d["strs"][u(r,i):d["strs"].find(b"\x00",u(r,i))].decode("utf8","replace")
ins=load(f"{INS}/CharacterAdvancement.dbc"); ori=load(f"{ORI}/CharacterAdvancement.dbc"); orig={u(r,0):r for r in ori["recs"]}
recs=[]; rest=0; drop=0
for r in ins["recs"]:
    i=u(r,0)
    if u(r,32)==CT and BASE<=i<BASE+200: drop+=1; continue
    if u(r,32)==CT and i in orig: r=orig[i]; rest+=1
    recs.append(r)
ins["recs"]=recs; save(f"{OUT}/CharacterAdvancement.dbc",ins)
cs=load(f"{INS}/ChrSpecs.dbc"); cso={u(r,0):r for r in load(f"{ORI}/ChrSpecs.dbc")["recs"]}
cs["recs"]=[cso.get(u(r,0),r) if S(cs,r,1)==CLS else r for r in cs["recs"]]; save(f"{OUT}/ChrSpecs.dbc",cs)
es=load(f"{INS}/CharacterAdvancementEssence.dbc"); eso={u(r,0):r for r in load(f"{ORI}/CharacterAdvancementEssence.dbc")["recs"]}
es["recs"]=[eso.get(u(r,0),r) if u(r,2)==ESS else r for r in es["recs"]]; save(f"{OUT}/CharacterAdvancementEssence.dbc",es)
print(f"fonte {OUT}: CA sem {drop} linhas geradas da classe, {rest} originais restauradas; ChrSpecs/Essence da classe restaurados")
