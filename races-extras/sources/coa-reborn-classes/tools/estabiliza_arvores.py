#!/usr/bin/env python3
"""[ESTABILIZA-1210] Pos-processo depois de gera_support.py (idempotente; NUNCA apaga linha, so desativa).
 A) Support das 4 classes -> arvore interna General2 (29), que o cliente conhece (os nomes de arvore
    sao uma lista FIXA no Extensions.dll, que termina em WitchKnight=94; 95-98 sao invisiveis ao cliente).
    ChrSpecs 102-105 passa a ter o token GENERAL2 (nome visivel "Support" preservado).
 B) Desativa talento PAGO cuja magia e a mesma da identidade da spec (a identidade ja da a magia no 10;
    o servidor reconhece talento pela magia conhecida e o contaria como comprado).
 C) Desativa talento PAGO de classe cuja magia o treinador ja ensina (Dual Wield / Call of the Elements)
    e as dependencias automaticas que so existiam por causa dele (Call of the Ancestors / Spirits).
 Tambem tira das CONNECTED as ligacoes que apontavam para linhas desativadas (so desenho).
Uso: estabiliza_arvores.py <dir_dbc_origem> <dir_saida> [--apply]"""
import struct, sys, os, json
SRC, OUT = sys.argv[1], sys.argv[2]; APPLY = "--apply" in sys.argv
def load(name):
    b=open(os.path.join(SRC,name),"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20])
    return dict(n=n,f=f,rs=rs,recs=[bytearray(b[20+i*rs:20+(i+1)*rs]) for i in range(n)],strs=bytearray(b[20+n*rs:]))
def save(name,d):
    os.makedirs(OUT,exist_ok=True)
    open(os.path.join(OUT,name),"wb").write(struct.pack("<4sIIII",b"WDBC",len(d["recs"]),d["f"],d["rs"],len(d["strs"]))+b"".join(d["recs"])+bytes(d["strs"]))
def u(r,i): return struct.unpack_from("<I",r,i*4)[0]
def su(r,i,v): struct.pack_into("<I",r,i*4,v)
def ub(r,o): return struct.unpack_from("<I",r,o)[0]
def sub_(r,o,v): struct.pack_into("<I",r,o,v)
def S(d,r,i):
    o=u(r,i); e=d["strs"].find(b"\x00",o); return bytes(d["strs"][o:e]).decode("utf8","replace") if 0<o<len(d["strs"]) else ""
def addstr(d,t):
    i=d["strs"].find(b"\x00"+t.encode()+b"\x00")
    if i>=0: return i+1
    off=len(d["strs"]); d["strs"]+=t.encode()+b"\x00"; return off
F_ID,F_SPELLS,F_AE,F_TE,F_CLASSTYPE,F_TAB,F_NAME=0,5,14,15,32,33,47
B_CONNECTED,B_FLAGS=415,479; N_CONNECTED=15; ROW_DISABLED=0x8
GENERAL2=29
SUPPORT={44:(95,102),40:(96,103),42:(97,104),43:(98,105)}   # classtype -> (aba antiga, spec)
CLASSIC_TYPES={36,37,38,39,40,41,42,43,44}
# C) lista fechada e conferida na auditoria de 12/10 (treinador ensina a mesma magia)
TRAINER_DUP={140501:674, 141299:66842}
TRAINER_DEPS={141300:(141299,66843), 141301:(141299,66844)}

ca=load("CharacterAdvancement.dbc"); specs=load("ChrSpecs.dbc")
byid={u(r,F_ID):r for r in ca["recs"]}
def spells(r): return [x for x in (u(r,F_SPELLS+k) for k in range(5)) if x]
def paid(r): return u(r,F_AE) or u(r,F_TE)
def active(r): return not ub(r,B_FLAGS)&0x9
log=dict(A=[],B=[],C=[],conexoes=[])

# A) Support -> General2
for r in ca["recs"]:
    ct=u(r,F_CLASSTYPE)
    if ct in SUPPORT and u(r,F_TAB) in (SUPPORT[ct][0],GENERAL2) and (u(r,F_ID)//100)*100 in (142000,142100,142200,142300):
        if u(r,F_TAB)!=GENERAL2:
            log["A"].append((u(r,F_ID),u(r,F_TAB))); su(r,F_TAB,GENERAL2)
for r in specs["recs"]:
    if u(r,0) in (102,103,104,105) and S(specs,r,2)!="GENERAL2":
        log["A"].append(("ChrSpecs",u(r,0),S(specs,r,2))); su(r,2,addstr(specs,"GENERAL2"))
# B) identidade x talento pago com a mesma magia (mesma classe, mesma spec)
ident={}
for r in specs["recs"]:
    if u(r,28): ident[u(r,28)]=u(r,0)
idspell={}
for i in ident:
    r=byid.get(i)
    if r is not None and active(r):
        for s in spells(r): idspell[(u(r,F_CLASSTYPE),s)]=i
for r in ca["recs"]:
    ct=u(r,F_CLASSTYPE)
    if ct not in CLASSIC_TYPES or not active(r) or not paid(r) or u(r,F_ID) in ident: continue
    hit=[(s,idspell[(ct,s)]) for s in spells(r) if (ct,s) in idspell]
    if hit:
        log["B"].append((u(r,F_ID),S(ca,r,F_NAME),hit[0][0],hit[0][1])); sub_(r,B_FLAGS,ub(r,B_FLAGS)|ROW_DISABLED)
# C) treinador
for eid,sp in list(TRAINER_DUP.items())+[(k,v[1]) for k,v in TRAINER_DEPS.items()]:
    r=byid[eid]
    assert sp in spells(r), (eid,sp,spells(r))
    if active(r):
        log["C"].append((eid,S(ca,r,F_NAME),sp,"pago" if paid(r) else "automatica")); sub_(r,B_FLAGS,ub(r,B_FLAGS)|ROW_DISABLED)
# ligacoes para linhas desativadas (so na mesma classe classica)
# so as linhas desta etapa (B e C); ligacoes antigas ficam como estao
off={x[0] for x in log["B"]}|set(TRAINER_DUP)|set(TRAINER_DEPS)
for r in ca["recs"]:
    if u(r,F_CLASSTYPE) not in CLASSIC_TYPES: continue
    con=[ub(r,B_CONNECTED+4*k) for k in range(N_CONNECTED)]
    keep=[c for c in con if c and c not in off]
    if len(keep)!=len([c for c in con if c]):
        log["conexoes"].append((u(r,F_ID),[c for c in con if c in off]))
        for k in range(N_CONNECTED): sub_(r,B_CONNECTED+4*k, keep[k] if k<len(keep) else 0)
print(json.dumps({k:(len(v)) for k,v in log.items()}))
os.makedirs(OUT,exist_ok=True); json.dump(log,open(os.path.join(OUT,"estabiliza_log.json"),"w"),indent=1,default=str)
if APPLY:
    save("CharacterAdvancement.dbc",ca); save("ChrSpecs.dbc",specs); print("gravado em",OUT)
